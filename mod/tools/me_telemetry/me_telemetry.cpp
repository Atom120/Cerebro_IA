// Mirror's Edge read-only telemetry recorder and movement-state mapper (B2).
// Owner: Benja (mod/tools/me_telemetry/).
//
// Strictly read-only: only PROCESS_VM_READ + ReadProcessMemory are used on MirrorsEdge.exe.
#include "me_telemetry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace ar::telemetry {

namespace {

// Minimal IGuestPawnAdapter used when driving aperture_runner::GuestBridge to validate and
// synthesize landing/Skill Roll reference curves without touching a live game process.
class TelemetryPawnSimAdapter final : public aperture_runner::IGuestPawnAdapter {
public:
    aperture_runner::Vec3 position_uu{};
    aperture_runner::Vec3 velocity_uu{};
    aperture_runner::Quat camera{};
    bool hard_landing_triggered = false;
    float hard_landing_stagger_sec = 0.0f;

    void TeleportPawn(aperture_runner::Vec3 pos, aperture_runner::Vec3 vel,
                      aperture_runner::Quat cam) override {
        position_uu = pos;
        velocity_uu = vel;
        camera = cam;
    }

    bool TraceWall(aperture_runner::Vec3, aperture_runner::Vec3, float,
                   aperture_runner::Vec3*) override {
        return false;
    }

    void ForceWallrun(aperture_runner::Vec3, bool) override {}
    void CancelWallrun() override {}
    void SetCameraRotation(aperture_runner::Quat cam) override { camera = cam; }
    void SetVelocity(aperture_runner::Vec3 vel) override { velocity_uu = vel; }
    void BeginHardLanding(float stagger_sec, float) override {
        hard_landing_triggered = true;
        hard_landing_stagger_sec = stagger_sec;
        velocity_uu = {0.0f, 0.0f, 0.0f};
    }
};

#ifdef _WIN32
DWORD FindMirrorsEdgePid() {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                if (_stricmp(pe.szExeFile, "MirrorsEdge.exe") == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

template <typename T>
bool ReadMem(HANDLE proc, uintptr_t addr, T* out) {
    SIZE_T bytes_read = 0;
    return ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(addr), out, sizeof(T), &bytes_read) !=
               0 &&
           bytes_read == sizeof(T);
}

bool ReadLiveSampleFromHandle(HANDLE proc, LiveProbeResult& out) {
    uint32_t pe_ofs = 0;
    if (ReadMem(proc, gog_v101::kImageBase + 0x3Cu, &pe_ofs) && pe_ofs < 0x1000u) {
        ReadMem(proc, gog_v101::kImageBase + pe_ofs + 8u, &out.pe_timestamp);
    }

    if (!ReadMem(proc, gog_v101::kGEngineVa, &out.gengine_ptr) || out.gengine_ptr == 0) {
        return false;
    }
    if (!ReadMem(proc, out.gengine_ptr + gog_v101::kOffsetGamePlayersData,
                 &out.game_players_data_ptr) ||
        out.game_players_data_ptr == 0) {
        return false;
    }
    if (!ReadMem(proc, out.game_players_data_ptr, &out.local_player_ptr) ||
        out.local_player_ptr == 0) {
        return false;
    }
    if (!ReadMem(proc, out.local_player_ptr + gog_v101::kOffsetLocalPlayerActor,
                 &out.controller_ptr) ||
        out.controller_ptr == 0) {
        return false;
    }
    if (!ReadMem(proc, out.controller_ptr + gog_v101::kOffsetControllerPawn, &out.pawn_ptr) ||
        out.pawn_ptr == 0) {
        return false;
    }

    ReadMem(proc, out.pawn_ptr, &out.pawn_vtable);
    ReadMem(proc, out.pawn_ptr + gog_v101::kOffsetActorPhysics, &out.physics_mode);

    float loc[3] = {0.0f, 0.0f, 0.0f};
    float vel[3] = {0.0f, 0.0f, 0.0f};
    if (!ReadMem(proc, out.pawn_ptr + gog_v101::kOffsetActorLocation, &loc)) {
        return false;
    }
    if (!ReadMem(proc, out.pawn_ptr + gog_v101::kOffsetActorVelocity, &vel)) {
        return false;
    }

    uint8_t move_state = 0;
    uint8_t walk_state = 0;
    ReadMem(proc, out.pawn_ptr + gog_v101::kOffsetTdPawnMovementState, &move_state);
    ReadMem(proc, out.pawn_ptr + gog_v101::kOffsetTdPawnWalkingState, &walk_state);

    out.sample.x = loc[0];
    out.sample.y = loc[1];
    out.sample.z = loc[2];
    out.sample.vx = vel[0];
    out.sample.vy = vel[1];
    out.sample.vz = vel[2];
    out.sample.movement_state = move_state;
    out.sample.walking_state = walk_state;
    return true;
}
#endif

}  // namespace

const char* MeMovementStateName(uint8_t state) {
    switch (static_cast<MeMovementState>(state)) {
        case MeMovementState::MOVE_None: return "MOVE_None";
        case MeMovementState::MOVE_Walking: return "MOVE_Walking";
        case MeMovementState::MOVE_Falling: return "MOVE_Falling";
        case MeMovementState::MOVE_Grabbing: return "MOVE_Grabbing";
        case MeMovementState::MOVE_WallRunningRight: return "MOVE_WallRunningRight";
        case MeMovementState::MOVE_WallRunningLeft: return "MOVE_WallRunningLeft";
        case MeMovementState::MOVE_WallClimbing: return "MOVE_WallClimbing";
        case MeMovementState::MOVE_SpringBoarding: return "MOVE_SpringBoarding";
        case MeMovementState::MOVE_SpeedVaulting: return "MOVE_SpeedVaulting";
        case MeMovementState::MOVE_VaultOver: return "MOVE_VaultOver";
        case MeMovementState::MOVE_GrabPullUp: return "MOVE_GrabPullUp";
        case MeMovementState::MOVE_Jump: return "MOVE_Jump";
        case MeMovementState::MOVE_WallRunJump: return "MOVE_WallRunJump";
        case MeMovementState::MOVE_GrabJump: return "MOVE_GrabJump";
        case MeMovementState::MOVE_IntoGrab: return "MOVE_IntoGrab";
        case MeMovementState::MOVE_Crouch: return "MOVE_Crouch";
        case MeMovementState::MOVE_Slide: return "MOVE_Slide";
        case MeMovementState::MOVE_Melee: return "MOVE_Melee";
        case MeMovementState::MOVE_Snatch: return "MOVE_Snatch";
        case MeMovementState::MOVE_Barge: return "MOVE_Barge";
        case MeMovementState::MOVE_Landing: return "MOVE_Landing";
        case MeMovementState::MOVE_Climb: return "MOVE_Climb";
        case MeMovementState::MOVE_IntoClimb: return "MOVE_IntoClimb";
        case MeMovementState::MOVE_WallKick: return "MOVE_WallKick";
        case MeMovementState::MOVE_180Turn: return "MOVE_180Turn";
        case MeMovementState::MOVE_180TurnInAir: return "MOVE_180TurnInAir";
        case MeMovementState::MOVE_LayOnGround: return "MOVE_LayOnGround";
        case MeMovementState::MOVE_IntoZipLine: return "MOVE_IntoZipLine";
        case MeMovementState::MOVE_ZipLine: return "MOVE_ZipLine";
        case MeMovementState::MOVE_Balance: return "MOVE_Balance";
        case MeMovementState::MOVE_LedgeWalk: return "MOVE_LedgeWalk";
        case MeMovementState::MOVE_GrabTransfer: return "MOVE_GrabTransfer";
        case MeMovementState::MOVE_MeleeAir: return "MOVE_MeleeAir";
        case MeMovementState::MOVE_DodgeJump: return "MOVE_DodgeJump";
        case MeMovementState::MOVE_WallRunDodgeJump: return "MOVE_WallRunDodgeJump";
        case MeMovementState::MOVE_Stumble: return "MOVE_Stumble";
        case MeMovementState::MOVE_Snatched: return "MOVE_Snatched";
        case MeMovementState::MOVE_StepUp: return "MOVE_StepUp";
        case MeMovementState::MOVE_RumpSlide: return "MOVE_RumpSlide";
        case MeMovementState::MOVE_Interact: return "MOVE_Interact";
        case MeMovementState::MOVE_WallRun: return "MOVE_WallRun";
        case MeMovementState::MOVE_WallClimbDodgeJump: return "MOVE_WallClimbDodgeJump";
        case MeMovementState::MOVE_WallClimb180TurnJump: return "MOVE_WallClimb180TurnJump";
        case MeMovementState::MOVE_WallClimbDodgeJumpLeft: return "MOVE_WallClimbDodgeJumpLeft";
        case MeMovementState::MOVE_WallClimbDodgeJumpRight: return "MOVE_WallClimbDodgeJumpRight";
        case MeMovementState::MOVE_StumbleHard: return "MOVE_StumbleHard";
        case MeMovementState::MOVE_Swing: return "MOVE_Swing";
        case MeMovementState::MOVE_Coil: return "MOVE_Coil";
        case MeMovementState::MOVE_JumpIntoGrab: return "MOVE_JumpIntoGrab";
        case MeMovementState::MOVE_FallingUncontrolled: return "MOVE_FallingUncontrolled";
        case MeMovementState::MOVE_SwingJump: return "MOVE_SwingJump";
        case MeMovementState::MOVE_AnimationPlayback: return "MOVE_AnimationPlayback";
        case MeMovementState::MOVE_SoftLanding: return "MOVE_SoftLanding";
        case MeMovementState::MOVE_AutoStepUp: return "MOVE_AutoStepUp";
        case MeMovementState::MOVE_SkillRoll: return "MOVE_SkillRoll";
        case MeMovementState::MOVE_Cutscene: return "MOVE_Cutscene";
        case MeMovementState::MOVE_MAX: return "MOVE_MAX";
        default: return "MOVE_Other";
    }
}

const char* MeWalkingStateName(uint8_t state) {
    switch (static_cast<MeWalkingState>(state)) {
        case MeWalkingState::WAS_Idle: return "WAS_Idle";
        case MeWalkingState::WAS_Sneak: return "WAS_Sneak";
        case MeWalkingState::WAS_Walk: return "WAS_Walk";
        case MeWalkingState::WAS_Jog: return "WAS_Jog";
        case MeWalkingState::WAS_Run: return "WAS_Run";
        case MeWalkingState::WAS_Sprint: return "WAS_Sprint";
        case MeWalkingState::WAS_None: return "WAS_None";
        case MeWalkingState::WAS_MAX: return "WAS_MAX";
        default: return "WAS_Unknown";
    }
}

aperture_runner::TdMovementState MapMeToTdMovementState(uint8_t movement_state,
                                                        uint8_t walking_state) {
    using aperture_runner::TdMovementState;
    switch (static_cast<MeMovementState>(movement_state)) {
        case MeMovementState::MOVE_Walking:
        case MeMovementState::MOVE_StepUp:
        case MeMovementState::MOVE_AutoStepUp:
        case MeMovementState::MOVE_SoftLanding:
            if (static_cast<MeWalkingState>(walking_state) == MeWalkingState::WAS_Sprint) {
                return TdMovementState::Sprinting;
            }
            return TdMovementState::Walking;
        case MeMovementState::MOVE_Crouch: return TdMovementState::Crouching;
        case MeMovementState::MOVE_Slide:
        case MeMovementState::MOVE_RumpSlide: return TdMovementState::Sliding;
        case MeMovementState::MOVE_WallRunningLeft: return TdMovementState::WallRunningLeft;
        case MeMovementState::MOVE_WallRunningRight:
        case MeMovementState::MOVE_WallRun: return TdMovementState::WallRunningRight;
        case MeMovementState::MOVE_WallClimbing: return TdMovementState::WallClimbing;
        case MeMovementState::MOVE_WallRunJump:
        case MeMovementState::MOVE_WallRunDodgeJump: return TdMovementState::WallrunJump;
        case MeMovementState::MOVE_WallClimb180TurnJump:
        case MeMovementState::MOVE_WallClimbDodgeJump:
        case MeMovementState::MOVE_WallClimbDodgeJumpLeft:
        case MeMovementState::MOVE_WallClimbDodgeJumpRight:
            return TdMovementState::Wallclimb180Jump;
        case MeMovementState::MOVE_VaultOver:
        case MeMovementState::MOVE_SpeedVaulting: return TdMovementState::Vaulting;
        case MeMovementState::MOVE_SpringBoarding: return TdMovementState::SpringBoarding;
        case MeMovementState::MOVE_Grabbing:
        case MeMovementState::MOVE_IntoGrab:
        case MeMovementState::MOVE_JumpIntoGrab:
        case MeMovementState::MOVE_GrabPullUp:
        case MeMovementState::MOVE_GrabJump: return TdMovementState::LedgeGrabbing;
        case MeMovementState::MOVE_LedgeWalk:
        case MeMovementState::MOVE_GrabTransfer: return TdMovementState::LedgeShimmy;
        case MeMovementState::MOVE_SkillRoll: return TdMovementState::SkillRolling;
        case MeMovementState::MOVE_Falling:
        case MeMovementState::MOVE_FallingUncontrolled:
        case MeMovementState::MOVE_Jump:
        case MeMovementState::MOVE_Coil: return TdMovementState::Falling;
        case MeMovementState::MOVE_Barge:
        case MeMovementState::MOVE_AirBarge: return TdMovementState::BargeDoor;
        case MeMovementState::MOVE_MeleeAir: return TdMovementState::JumpKick;
        case MeMovementState::MOVE_Snatch: return TdMovementState::TurretDisarm;
        default: return TdMovementState::Other;
    }
}

FaithAnimState MapMeToFaithAnimState(uint8_t movement_state, uint8_t walking_state,
                                     float speed_2d_uu) {
    switch (static_cast<MeMovementState>(movement_state)) {
        case MeMovementState::MOVE_Walking:
        case MeMovementState::MOVE_StepUp:
        case MeMovementState::MOVE_AutoStepUp: {
            const auto ws = static_cast<MeWalkingState>(walking_state);
            if (ws == MeWalkingState::WAS_Sprint ||
                speed_2d_uu >= me_reference::kSprintStartSpeedUu) {
                return FaithAnimState::Sprint;
            }
            if (ws == MeWalkingState::WAS_Run || ws == MeWalkingState::WAS_Jog) {
                return FaithAnimState::Run;
            }
            if (ws == MeWalkingState::WAS_Walk || ws == MeWalkingState::WAS_Sneak ||
                speed_2d_uu > 10.0f) {
                return FaithAnimState::Walk;
            }
            return FaithAnimState::Idle;
        }
        case MeMovementState::MOVE_Crouch:
            return speed_2d_uu > 10.0f ? FaithAnimState::CrouchWalk : FaithAnimState::Crouch;
        case MeMovementState::MOVE_Jump: return FaithAnimState::Jump;
        case MeMovementState::MOVE_Falling: return FaithAnimState::Fall;
        case MeMovementState::MOVE_FallingUncontrolled: return FaithAnimState::FallUncontrolled;
        case MeMovementState::MOVE_Coil: return FaithAnimState::Coil;
        case MeMovementState::MOVE_SoftLanding: return FaithAnimState::LandSoft;
        case MeMovementState::MOVE_Landing:
        case MeMovementState::MOVE_StumbleHard: return FaithAnimState::LandHard;
        case MeMovementState::MOVE_SkillRoll: return FaithAnimState::SkillRoll;
        case MeMovementState::MOVE_Slide:
        case MeMovementState::MOVE_RumpSlide: return FaithAnimState::Slide;
        case MeMovementState::MOVE_WallRunningLeft: return FaithAnimState::WallRunLeft;
        case MeMovementState::MOVE_WallRunningRight:
        case MeMovementState::MOVE_WallRun: return FaithAnimState::WallRunRight;
        case MeMovementState::MOVE_WallRunJump:
        case MeMovementState::MOVE_WallRunDodgeJump: return FaithAnimState::WallRunJump;
        case MeMovementState::MOVE_WallClimbing: return FaithAnimState::WallClimb;
        case MeMovementState::MOVE_WallClimb180TurnJump: return FaithAnimState::WallClimbJump;
        case MeMovementState::MOVE_WallClimbDodgeJump: return FaithAnimState::WallClimbTurn;
        case MeMovementState::MOVE_Grabbing:
        case MeMovementState::MOVE_IntoGrab:
        case MeMovementState::MOVE_JumpIntoGrab:
        case MeMovementState::MOVE_LedgeWalk:
        case MeMovementState::MOVE_GrabTransfer: return FaithAnimState::LedgeHang;
        case MeMovementState::MOVE_GrabPullUp:
        case MeMovementState::MOVE_GrabJump: return FaithAnimState::LedgeClimbUp;
        case MeMovementState::MOVE_VaultOver:
        case MeMovementState::MOVE_SpeedVaulting:
        case MeMovementState::MOVE_SpringBoarding: return FaithAnimState::Vault;
        case MeMovementState::MOVE_180Turn:
        case MeMovementState::MOVE_180TurnInAir: return FaithAnimState::Turn180;
        default: return FaithAnimState::Idle;
    }
}

ConvertedSourceSample ConvertSampleToSource(const TelemetrySample& sample) {
    ConvertedSourceSample out{};
    out.t = sample.t;
    out.origin_su = aperture_runner::Ue3ToSourceVector({sample.x, sample.y, sample.z});
    out.velocity_su = aperture_runner::Ue3ToSourceVector({sample.vx, sample.vy, sample.vz});
    out.speed_2d_su = std::sqrt(out.velocity_su.x * out.velocity_su.x +
                                out.velocity_su.y * out.velocity_su.y);
    const float speed_2d_uu =
        std::sqrt(sample.vx * sample.vx + sample.vy * sample.vy);
    out.td_state = MapMeToTdMovementState(sample.movement_state, sample.walking_state);
    out.faith_anim =
        MapMeToFaithAnimState(sample.movement_state, sample.walking_state, speed_2d_uu);
    return out;
}

const char* CsvHeader() { return "t,x,y,z,vx,vy,vz,movement_state,walking_state"; }

std::string FormatCsvRow(const TelemetrySample& s) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u", s.t, s.x, s.y, s.z,
                  s.vx, s.vy, s.vz, static_cast<unsigned>(s.movement_state),
                  static_cast<unsigned>(s.walking_state));
    return std::string(buf);
}

bool ParseCsvRow(const std::string& line, TelemetrySample& out_sample) {
    if (line.empty() || line[0] == '#' || line[0] == 't') {
        return false;
    }
    std::istringstream ss(line);
    char c1 = 0, c2 = 0, c3 = 0, c4 = 0, c5 = 0, c6 = 0, c7 = 0, c8 = 0;
    double t = 0.0;
    float x = 0.0f, y = 0.0f, z = 0.0f, vx = 0.0f, vy = 0.0f, vz = 0.0f;
    unsigned move_s = 0, walk_s = 0;
    if (!(ss >> t >> c1 >> x >> c2 >> y >> c3 >> z >> c4 >> vx >> c5 >> vy >> c6 >> vz >> c7 >>
          move_s >> c8 >> walk_s)) {
        return false;
    }
    if (c1 != ',' || c2 != ',' || c3 != ',' || c4 != ',' || c5 != ',' || c6 != ',' || c7 != ',' ||
        c8 != ',' || move_s > 255u || walk_s > 255u) {
        return false;
    }
    out_sample.t = t;
    out_sample.x = x;
    out_sample.y = y;
    out_sample.z = z;
    out_sample.vx = vx;
    out_sample.vy = vy;
    out_sample.vz = vz;
    out_sample.movement_state = static_cast<uint8_t>(move_s);
    out_sample.walking_state = static_cast<uint8_t>(walk_s);
    return true;
}

bool WriteCsvFile(const std::string& path, const std::vector<TelemetrySample>& samples) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        return false;
    }
    f << CsvHeader() << "\n";
    for (const auto& s : samples) {
        f << FormatCsvRow(s) << "\n";
    }
    return f.good();
}

bool ReadCsvFile(const std::string& path, std::vector<TelemetrySample>& out_samples) {
    out_samples.clear();
    std::ifstream f(path);
    if (!f.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        TelemetrySample s{};
        if (ParseCsvRow(line, s)) {
            out_samples.push_back(s);
        }
    }
    return !out_samples.empty();
}

LiveProbeResult ProbeLiveMirrorsEdge() {
    LiveProbeResult res{};
#ifdef _WIN32
    const DWORD pid = FindMirrorsEdgePid();
    if (pid == 0) {
        return res;
    }
    res.process_found = true;
    res.pid = static_cast<uint32_t>(pid);

    // Strictly READ-ONLY process handle: PROCESS_VM_READ | PROCESS_QUERY_INFORMATION.
    HANDLE proc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!proc) {
        return res;
    }
    res.handle_opened_readonly = true;
    ReadLiveSampleFromHandle(proc, res);
    CloseHandle(proc);
#endif
    return res;
}

bool RecordLiveMirrorsEdge(const std::string& csv_path, double duration_sec, int hz,
                           std::size_t* out_rows_written) {
    if (out_rows_written) {
        *out_rows_written = 0;
    }
#ifdef _WIN32
    const int clamped_hz = std::clamp(hz, 60, 120);
    const DWORD sleep_ms = static_cast<DWORD>(std::max(1, 1000 / clamped_hz));
    const DWORD pid = FindMirrorsEdgePid();
    if (pid == 0) {
        return false;
    }
    HANDLE proc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!proc) {
        return false;
    }

    LARGE_INTEGER freq{}, t0{}, now{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    std::vector<TelemetrySample> samples;
    samples.reserve(static_cast<std::size_t>(duration_sec * clamped_hz) + 16u);

    while (true) {
        QueryPerformanceCounter(&now);
        const double elapsed =
            static_cast<double>(now.QuadPart - t0.QuadPart) / static_cast<double>(freq.QuadPart);
        if (elapsed > duration_sec) {
            break;
        }
        LiveProbeResult probe{};
        if (ReadLiveSampleFromHandle(proc, probe)) {
            probe.sample.t = elapsed;
            samples.push_back(probe.sample);
        }
        Sleep(sleep_ms);
    }

    CloseHandle(proc);
    if (samples.empty()) {
        return false;
    }
    if (!WriteCsvFile(csv_path, samples)) {
        return false;
    }
    if (out_rows_written) {
        *out_rows_written = samples.size();
    }
    return true;
#else
    (void)csv_path;
    (void)duration_sec;
    (void)hz;
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Reference curves (120 Hz) calibrated to Mirror's Edge TdGame.u constants
// ---------------------------------------------------------------------------

namespace {

uint8_t ClassifyGroundWalkingState(float speed_2d_uu) {
    if (speed_2d_uu < 5.0f) return static_cast<uint8_t>(MeWalkingState::WAS_Idle);
    if (speed_2d_uu < 160.0f) return static_cast<uint8_t>(MeWalkingState::WAS_Walk);
    if (speed_2d_uu < 280.0f) return static_cast<uint8_t>(MeWalkingState::WAS_Jog);
    if (speed_2d_uu < me_reference::kSprintStartSpeedUu)
        return static_cast<uint8_t>(MeWalkingState::WAS_Run);
    return static_cast<uint8_t>(MeWalkingState::WAS_Sprint);
}

}  // namespace

std::vector<TelemetrySample> BuildRefSprintStart(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = rate * 3;  // 3.0 seconds: Idle -> Walk -> Run -> Sprint (720 uu/s)

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    float x = 0.0f;
    float vx = 0.0f;
    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        if (t >= 0.15) {
            // Below 400 uu/s rapid base acceleration; above 400 uu/s inertia ramp to 720 uu/s.
            const float accel = (vx < me_reference::kSprintStartSpeedUu) ? 1600.0f : 220.0f;
            vx = std::min(me_reference::kSprintMaxSpeedUu, vx + accel * static_cast<float>(dt));
            x += vx * static_cast<float>(dt);
        }
        TelemetrySample s{};
        s.t = t;
        s.x = x;
        s.y = 0.0f;
        s.z = 90.0f;  // Capsule centre on flat ground (HalfHeight = 90 uu)
        s.vx = vx;
        s.vy = 0.0f;
        s.vz = 0.0f;
        s.movement_state = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
        s.walking_state = ClassifyGroundWalkingState(vx);
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefJump(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 1.5);

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    float x = 0.0f;
    float z = 90.0f;
    float vx = me_reference::kSprintMaxSpeedUu;
    float vz = 0.0f;
    bool jumped = false;
    bool landed = false;

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_Sprint);

        if (!jumped && t >= 0.20) {
            jumped = true;
            vz = me_reference::kJumpZUu;
            vx = std::min(me_reference::kSprintMaxSpeedUu + me_reference::kJumpAddXYUu,
                          vx + me_reference::kJumpAddXYUu);
        }

        if (jumped && !landed) {
            z += vz * static_cast<float>(dt);
            vz -= me_reference::kFaithGravityUu * static_cast<float>(dt);
            if (z <= 90.0f && vz < 0.0f) {
                z = 90.0f;
                vz = 0.0f;
                vx = me_reference::kSprintMaxSpeedUu;
                landed = true;
                move = static_cast<uint8_t>(MeMovementState::MOVE_SoftLanding);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_Sprint);
            } else {
                move = (vz > 0.0f) ? static_cast<uint8_t>(MeMovementState::MOVE_Jump)
                                   : static_cast<uint8_t>(MeMovementState::MOVE_Falling);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            }
        }
        x += vx * static_cast<float>(dt);

        TelemetrySample s{};
        s.t = t;
        s.x = x;
        s.y = 0.0f;
        s.z = z;
        s.vx = vx;
        s.vy = 0.0f;
        s.vz = vz;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefWallrun(bool left_wall, int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 2.0);

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    float x = 0.0f;
    float y = left_wall ? -30.0f : 30.0f;
    float z = 90.0f;
    float vx = 650.0f;
    float vy = 0.0f;
    float vz = 0.0f;

    const uint8_t wr_state = left_wall
                                 ? static_cast<uint8_t>(MeMovementState::MOVE_WallRunningLeft)
                                 : static_cast<uint8_t>(MeMovementState::MOVE_WallRunningRight);

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_Sprint);

        if (t >= 0.20 && t < 0.85) {
            // Wallrun arc (0.65 s on wall, then WallRunJump off wall while airborne).
            const double wr_t = t - 0.20;
            move = wr_state;
            walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            if (wr_t < dt * 1.5) {
                z = 110.0f;
                vz = me_reference::kWallrunInitialZUu;
            } else {
                // Reduced wallrun gravity along the parabolic wall arc.
                vz -= (me_reference::kFaithGravityUu * 0.28f) * static_cast<float>(dt);
            }
            vx = std::max(me_reference::kWallrunMinSpeedUu,
                          vx - (me_reference::kWallrunHorizDecelUu * 0.25f) * static_cast<float>(dt));
            z = std::max(95.0f, z + vz * static_cast<float>(dt));
        } else if (t >= 0.85 && z > 90.0f) {
            if (t < 0.85 + dt * 1.5) {
                vz = 420.0f;
                vy = left_wall ? 240.0f : -240.0f;
            }
            move = (vz > 0.0f) ? static_cast<uint8_t>(MeMovementState::MOVE_WallRunJump)
                               : static_cast<uint8_t>(MeMovementState::MOVE_Falling);
            walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            z += vz * static_cast<float>(dt);
            y += vy * static_cast<float>(dt);
            vz -= me_reference::kFaithGravityUu * static_cast<float>(dt);
            if (z <= 90.0f) {
                z = 90.0f;
                vz = 0.0f;
                vy = 0.0f;
                move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
                walk = ClassifyGroundWalkingState(vx);
            }
        }
        x += vx * static_cast<float>(dt);

        TelemetrySample s{};
        s.t = t;
        s.x = x;
        s.y = y;
        s.z = z;
        s.vx = vx;
        s.vy = vy;
        s.vz = vz;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefWallclimb(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 1.8);

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    float x = 0.0f;
    float z = 90.0f;
    float vx = 600.0f;
    float vz = 0.0f;

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_Sprint);

        if (t >= 0.25 && t < 1.05) {
            move = static_cast<uint8_t>(MeMovementState::MOVE_WallClimbing);
            walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            vx = 0.0f;
            const double wc_t = t - 0.25;
            vz = std::max(80.0f, 460.0f - static_cast<float>(wc_t) * 420.0f);
            z += vz * static_cast<float>(dt);
        } else if (t >= 1.05) {
            if (t < 1.05 + dt * 1.5) {
                vz = 480.0f;
                vx = -420.0f;  // 180 turn jump away from wall
            }
            move = (vz > 0.0f) ? static_cast<uint8_t>(MeMovementState::MOVE_WallClimb180TurnJump)
                               : static_cast<uint8_t>(MeMovementState::MOVE_Falling);
            walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            z += vz * static_cast<float>(dt);
            x += vx * static_cast<float>(dt);
            vz -= me_reference::kFaithGravityUu * static_cast<float>(dt);
            if (z <= 90.0f) {
                z = 90.0f;
                vz = 0.0f;
                move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
                walk = ClassifyGroundWalkingState(std::fabs(vx));
            }
        } else {
            x += vx * static_cast<float>(dt);
        }

        TelemetrySample s{};
        s.t = t;
        s.x = x;
        s.y = 0.0f;
        s.z = z;
        s.vx = vx;
        s.vy = 0.0f;
        s.vz = vz;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefLedgeGrab(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 1.8);

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    float x = 0.0f;
    float z = 90.0f;
    float vx = 500.0f;
    float vz = me_reference::kJumpZUu;

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Jump);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_None);

        if (t < 0.35) {
            x += vx * static_cast<float>(dt);
            z += vz * static_cast<float>(dt);
            vz -= me_reference::kFaithGravityUu * static_cast<float>(dt);
        } else if (t < 0.50) {
            move = static_cast<uint8_t>(MeMovementState::MOVE_IntoGrab);
            vx = 0.0f;
            vz = 0.0f;
            z = 240.0f;
        } else if (t < 0.90) {
            move = static_cast<uint8_t>(MeMovementState::MOVE_Grabbing);
            vx = 0.0f;
            vz = 0.0f;
            z = 240.0f;
        } else if (t < 1.45) {
            move = static_cast<uint8_t>(MeMovementState::MOVE_GrabPullUp);
            vx = 110.0f;
            vz = 160.0f;
            x += vx * static_cast<float>(dt);
            z = std::min(330.0f, z + vz * static_cast<float>(dt));
        } else {
            move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
            vx = 250.0f;
            vz = 0.0f;
            z = 330.0f;
            x += vx * static_cast<float>(dt);
            walk = ClassifyGroundWalkingState(vx);
        }

        TelemetrySample s{};
        s.t = t;
        s.x = x;
        s.y = 0.0f;
        s.z = z;
        s.vx = vx;
        s.vy = 0.0f;
        s.vz = vz;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefSlide(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 1.8);

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    float x = 0.0f;
    float vx = me_reference::kSprintMaxSpeedUu;

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_Sprint);
        float z = 90.0f;

        if (t >= 0.20) {
            vx = std::max(180.0f, vx - 450.0f * static_cast<float>(dt));
            if (vx > me_reference::kSlideAbortSpeedUu) {
                move = static_cast<uint8_t>(MeMovementState::MOVE_Slide);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
                z = 45.0f;
            } else {
                move = static_cast<uint8_t>(MeMovementState::MOVE_Crouch);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_Walk);
                z = 55.0f;
            }
        }
        x += vx * static_cast<float>(dt);

        TelemetrySample s{};
        s.t = t;
        s.x = x;
        s.y = 0.0f;
        s.z = z;
        s.vx = vx;
        s.vy = 0.0f;
        s.vz = 0.0f;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefHardLanding(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 1.8);

    // Use aperture_runner::GuestBridge to verify hard-landing resolution on touch-down.
    aperture_runner::GuestBridge guest_bridge;
    TelemetryPawnSimAdapter sim_pawn;
    sim_pawn.position_uu = {0.0f, 0.0f, 750.0f};  // > kHardLandingHeightUu (530 uu)
    sim_pawn.velocity_uu = {320.0f, 0.0f, 0.0f};

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    bool landed = false;
    double landed_time = 0.0;
    const float start_z = sim_pawn.position_uu.z;

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Falling);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_None);

        if (!landed) {
            sim_pawn.position_uu.x += sim_pawn.velocity_uu.x * static_cast<float>(dt);
            sim_pawn.position_uu.z += sim_pawn.velocity_uu.z * static_cast<float>(dt);
            sim_pawn.velocity_uu.z -= me_reference::kFaithGravityUu * static_cast<float>(dt);

            const float fall_dist = start_z - sim_pawn.position_uu.z;
            if (fall_dist > me_reference::kHardLandingHeightUu) {
                move = static_cast<uint8_t>(MeMovementState::MOVE_FallingUncontrolled);
            }

            if (sim_pawn.position_uu.z <= 90.0f) {
                sim_pawn.position_uu.z = 90.0f;
                aperture_runner::ApertureEventPacket ev{};
                bool has_ev = false;
                guest_bridge.OnLanded(t, sim_pawn.position_uu, sim_pawn.velocity_uu,
                                      sim_pawn.camera, sim_pawn, &ev, &has_ev,
                                      static_cast<uint64_t>(i));
                sim_pawn.velocity_uu.z = 0.0f;
                landed = true;
                landed_time = t;
                move = static_cast<uint8_t>(MeMovementState::MOVE_Landing);
            }
        } else {
            if (t - landed_time < static_cast<double>(sim_pawn.hard_landing_stagger_sec)) {
                move = static_cast<uint8_t>(MeMovementState::MOVE_Landing);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            } else {
                move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_Idle);
            }
        }

        TelemetrySample s{};
        s.t = t;
        s.x = sim_pawn.position_uu.x;
        s.y = sim_pawn.position_uu.y;
        s.z = sim_pawn.position_uu.z;
        s.vx = sim_pawn.velocity_uu.x;
        s.vy = sim_pawn.velocity_uu.y;
        s.vz = sim_pawn.velocity_uu.z;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

std::vector<TelemetrySample> BuildRefSkillRoll(int hz) {
    const int rate = std::clamp(hz, 60, 120);
    const double dt = 1.0 / static_cast<double>(rate);
    const int steps = static_cast<int>(rate * 1.8);

    // Use aperture_runner::GuestBridge to time the crouch input and preserve horizontal speed.
    aperture_runner::GuestBridge guest_bridge;
    TelemetryPawnSimAdapter sim_pawn;
    sim_pawn.position_uu = {0.0f, 0.0f, 680.0f};
    sim_pawn.velocity_uu = {580.0f, 0.0f, 0.0f};

    std::vector<TelemetrySample> out;
    out.reserve(static_cast<std::size_t>(steps + 1));

    bool crouch_pressed = false;
    bool landed = false;
    double landed_time = 0.0;

    for (int i = 0; i <= steps; ++i) {
        const double t = i * dt;
        uint8_t move = static_cast<uint8_t>(MeMovementState::MOVE_Falling);
        uint8_t walk = static_cast<uint8_t>(MeWalkingState::WAS_None);

        if (!landed) {
            sim_pawn.position_uu.x += sim_pawn.velocity_uu.x * static_cast<float>(dt);
            sim_pawn.position_uu.z += sim_pawn.velocity_uu.z * static_cast<float>(dt);
            sim_pawn.velocity_uu.z -= me_reference::kFaithGravityUu * static_cast<float>(dt);

            // Press crouch within the Skill Roll window just before impact.
            if (!crouch_pressed && sim_pawn.position_uu.z <= 210.0f) {
                crouch_pressed = true;
                guest_bridge.OnCrouchPressed(t);
            }
            if (crouch_pressed) {
                move = static_cast<uint8_t>(MeMovementState::MOVE_Coil);
            }

            if (sim_pawn.position_uu.z <= 90.0f) {
                sim_pawn.position_uu.z = 90.0f;
                aperture_runner::ApertureEventPacket ev{};
                bool has_ev = false;
                guest_bridge.OnLanded(t, sim_pawn.position_uu, sim_pawn.velocity_uu,
                                      sim_pawn.camera, sim_pawn, &ev, &has_ev,
                                      static_cast<uint64_t>(i));
                sim_pawn.velocity_uu.z = 0.0f;
                landed = true;
                landed_time = t;
                move = static_cast<uint8_t>(MeMovementState::MOVE_SkillRoll);
            }
        } else {
            sim_pawn.position_uu.x += sim_pawn.velocity_uu.x * static_cast<float>(dt);
            if (t - landed_time < 0.65) {
                move = static_cast<uint8_t>(MeMovementState::MOVE_SkillRoll);
                walk = static_cast<uint8_t>(MeWalkingState::WAS_None);
            } else {
                move = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
                walk = ClassifyGroundWalkingState(sim_pawn.velocity_uu.x);
            }
        }

        TelemetrySample s{};
        s.t = t;
        s.x = sim_pawn.position_uu.x;
        s.y = sim_pawn.position_uu.y;
        s.z = sim_pawn.position_uu.z;
        s.vx = sim_pawn.velocity_uu.x;
        s.vy = sim_pawn.velocity_uu.y;
        s.vz = sim_pawn.velocity_uu.z;
        s.movement_state = move;
        s.walking_state = walk;
        out.push_back(s);
    }
    return out;
}

bool GenerateAllReferenceCsvs(const std::string& output_dir, int hz) {
    struct ClipSpec {
        const char* filename;
        std::vector<TelemetrySample> samples;
    };
    const ClipSpec clips[] = {
        {"sprint_start.csv", BuildRefSprintStart(hz)},
        {"jump.csv", BuildRefJump(hz)},
        {"wallrun_left.csv", BuildRefWallrun(true, hz)},
        {"wallrun_right.csv", BuildRefWallrun(false, hz)},
        {"wallclimb.csv", BuildRefWallclimb(hz)},
        {"ledge_grab.csv", BuildRefLedgeGrab(hz)},
        {"slide.csv", BuildRefSlide(hz)},
        {"hard_landing.csv", BuildRefHardLanding(hz)},
        {"skill_roll.csv", BuildRefSkillRoll(hz)},
    };

    for (const auto& c : clips) {
        const std::string full_path = output_dir + "/" + c.filename;
        if (!WriteCsvFile(full_path, c.samples)) {
            return false;
        }
    }
    return true;
}

}  // namespace ar::telemetry
