// Mirror's Edge read-only telemetry recorder and movement-state mapper (B2).
// Owner: Benja (mod/tools/me_telemetry/).
//
// Strictly read-only (PROCESS_VM_READ): never writes to MirrorsEdge.exe memory.
// Calibrated and verified against Mirror's Edge GOG v1.01 (DRM-Free, PE Timestamp 0x4965FFF1,
// fixed ImageBase 0x00400000) and TdGame/CookedPC/TdGame.u.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aperture_runner/bridge.h"
#include "aperture_runner/math.h"
#include "aperture_runner/protocol.h"
#include "ar/faith_movement.h"
#include "ar/units.h"

namespace ar::telemetry {

// Verified offsets for Mirror's Edge GOG v1.01 (MirrorsEdge.exe, PE Timestamp 0x4965FFF1).
namespace gog_v101 {
inline constexpr uint32_t kImageBase = 0x00400000u;
inline constexpr uint32_t kPeTimestamp = 0x4965FFF1u;
inline constexpr uint32_t kGEngineVa = 0x01FF8B20u;
inline constexpr uint32_t kGWorldVa = 0x01FFBCA4u;
inline constexpr uint32_t kRdataStartVa = 0x01A8B000u;
inline constexpr uint32_t kRdataEndVa = 0x01F53000u;
inline constexpr uint32_t kTdPawnVtableVa = 0x01D05268u;

// Pointer chain: GEngine -> GamePlayers.Data[0] -> Actor (ATdPlayerController) -> Pawn (ATdPlayerPawn)
inline constexpr uint32_t kOffsetGamePlayersData = 0x02BCu;
inline constexpr uint32_t kOffsetLocalPlayerActor = 0x0040u;
inline constexpr uint32_t kOffsetControllerPawn = 0x01C0u;

// AActor / APawn / ATdPawn fields (verified via ATdPawn::GetMove @ 0x012B1750,
// ATdPawn::IsInMove @ 0x012B1730, and ATdPawn::UpdateWalkingState @ 0x012B1660).
inline constexpr uint32_t kOffsetActorPhysics = 0x006Bu;        // uint8_t EPhysics (1=Walking, 2=Falling)
inline constexpr uint32_t kOffsetActorLocation = 0x00E8u;       // FVector (float x, y, z) in uu
inline constexpr uint32_t kOffsetActorRotation = 0x00F4u;       // FRotator (int32 pitch, yaw, roll) in URU
inline constexpr uint32_t kOffsetActorVelocity = 0x0100u;       // FVector (float vx, vy, vz) in uu/s
inline constexpr uint32_t kOffsetTdPawnSpeed2D = 0x04BCu;       // float cached 2D speed in uu/s
inline constexpr uint32_t kOffsetTdPawnMovementState = 0x04FEu; // uint8_t EMovement (0..93)
inline constexpr uint32_t kOffsetTdPawnWalkingState = 0x0503u;  // uint8_t WalkingState (0..6)
}  // namespace gog_v101

// Exact EMovement enum values extracted from TdGame/CookedPC/TdGame.u (0..94).
enum class MeMovementState : uint8_t {
    MOVE_None = 0,
    MOVE_Walking = 1,
    MOVE_Falling = 2,
    MOVE_Grabbing = 3,
    MOVE_WallRunningRight = 4,
    MOVE_WallRunningLeft = 5,
    MOVE_WallClimbing = 6,
    MOVE_SpringBoarding = 7,
    MOVE_SpeedVaulting = 8,
    MOVE_VaultOver = 9,
    MOVE_GrabPullUp = 10,
    MOVE_Jump = 11,
    MOVE_WallRunJump = 12,
    MOVE_GrabJump = 13,
    MOVE_IntoGrab = 14,
    MOVE_Crouch = 15,
    MOVE_Slide = 16,
    MOVE_Melee = 17,
    MOVE_Snatch = 18,
    MOVE_Barge = 19,
    MOVE_Landing = 20,
    MOVE_Climb = 21,
    MOVE_IntoClimb = 22,
    MOVE_WallKick = 23,
    MOVE_180Turn = 24,
    MOVE_180TurnInAir = 25,
    MOVE_LayOnGround = 26,
    MOVE_IntoZipLine = 27,
    MOVE_ZipLine = 28,
    MOVE_Balance = 29,
    MOVE_LedgeWalk = 30,
    MOVE_GrabTransfer = 31,
    MOVE_MeleeAir = 32,
    MOVE_DodgeJump = 33,
    MOVE_WallRunDodgeJump = 34,
    MOVE_Stumble = 35,
    MOVE_Snatched = 36,
    MOVE_StepUp = 37,
    MOVE_RumpSlide = 38,
    MOVE_Interact = 39,
    MOVE_WallRun = 40,
    MOVE_BotStop = 41,
    MOVE_BotStartWalking = 42,
    MOVE_BotStartRunning = 43,
    MOVE_BotTurnRunning = 44,
    MOVE_BotTurnStanding = 45,
    MOVE_ExitCover = 46,
    MOVE_Vertigo = 47,
    MOVE_MeleeSlide = 48,
    MOVE_WallClimbDodgeJump = 49,
    MOVE_WallClimb180TurnJump = 50,
    MOVE_WallClimbDodgeJumpLeft = 51,
    MOVE_WallClimbDodgeJumpRight = 52,
    MOVE_MeleeVault = 53,
    MOVE_BotMeleeSecondSwing = 54,
    MOVE_StumbleHard = 55,
    MOVE_BotRoll = 56,
    MOVE_BotFlip = 57,
    MOVE_Backflip_OBSOLETE = 58,
    MOVE_BackflipToRun_OBSOLETE = 59,
    MOVE_Swing = 60,
    MOVE_Coil = 61,
    MOVE_MeleeWallrun = 62,
    MOVE_MeleeCrouch = 63,
    MOVE_BotJumpShort = 64,
    MOVE_BotJumpMedium = 65,
    MOVE_BotJumpLong = 66,
    MOVE_JumpIntoGrab = 67,
    MOVE_StandGrabHeaveBot = 68,
    MOVE_BotMeleeDodge = 69,
    MOVE_FinishAttack = 70,
    MOVE_MeleeBarge = 71,
    MOVE_FallingUncontrolled = 72,
    MOVE_SwingJump = 73,
    MOVE_AnimationPlayback = 74,
    MOVE_EnterCover = 75,
    MOVE_Cover = 76,
    MOVE_StumbleFalling = 77,
    MOVE_SoftLanding = 78,
    MOVE_HeadButtedByCeleste = 79,
    MOVE_MeleeOriginalCeleste_OBSOLETE = 80,
    MOVE_AutoStepUp = 81,
    MOVE_MeleeAirAbove = 82,
    MOVE_MeleeCounterAttack_OBSOLETE = 83,
    MOVE_Block = 84,
    MOVE_AirBarge = 85,
    MOVE_RB_Bullrush_OBSOLETE = 86,
    MOVE_RB_Bullrush_End_OBSOLETE = 87,
    MOVE_RB_HitWall_OBSOLETE = 88,
    MOVE_RB_HitFence_OBSOLETE = 89,
    MOVE_RB_Ledge_OBSOLETE = 90,
    MOVE_SkillRoll = 91,
    MOVE_BotGetDistance = 92,
    MOVE_Cutscene = 93,
    MOVE_MAX = 94
};

// Exact WalkingState (WAS_*) enum values extracted from TdGame/CookedPC/TdGame.u (0..7).
enum class MeWalkingState : uint8_t {
    WAS_Idle = 0,
    WAS_Sneak = 1,
    WAS_Walk = 2,
    WAS_Jog = 3,
    WAS_Run = 4,
    WAS_Sprint = 5,
    WAS_None = 6,
    WAS_MAX = 7
};

// One telemetry sample in Mirror's Edge world units (uu, uu/s, seconds).
// Matches the CSV contract: t, x, y, z, vx, vy, vz, movement_state, walking_state
struct TelemetrySample {
    double t = 0.0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    uint8_t movement_state = 0;  // MeMovementState (ATdPawn + 0x04FE)
    uint8_t walking_state = 0;   // MeWalkingState  (ATdPawn + 0x0503)
};

// Converted sample in Source units (su = uu / 2.54, Y mirrored) for tuning-analyst (P5).
struct ConvertedSourceSample {
    double t = 0.0;
    Vec3 origin_su{};
    Vec3 velocity_su{};
    float speed_2d_su = 0.0f;
    aperture_runner::TdMovementState td_state = aperture_runner::TdMovementState::Other;
    FaithAnimState faith_anim = FaithAnimState::Idle;
};

// Live pointer chain snapshot from MirrorsEdge.exe (read-only verification).
struct LiveProbeResult {
    bool process_found = false;
    bool handle_opened_readonly = false;
    uint32_t pid = 0;
    uint32_t pe_timestamp = 0;
    uint32_t gengine_ptr = 0;
    uint32_t game_players_data_ptr = 0;
    uint32_t local_player_ptr = 0;
    uint32_t controller_ptr = 0;
    uint32_t pawn_ptr = 0;
    uint32_t pawn_vtable = 0;
    uint8_t physics_mode = 0;
    TelemetrySample sample{};

    bool IsChainComplete() const {
        return process_found && handle_opened_readonly && gengine_ptr != 0 &&
               local_player_ptr != 0 && controller_ptr != 0 && pawn_ptr != 0;
    }
};

const char* MeMovementStateName(uint8_t state);
const char* MeWalkingStateName(uint8_t state);

aperture_runner::TdMovementState MapMeToTdMovementState(uint8_t movement_state,
                                                        uint8_t walking_state);
FaithAnimState MapMeToFaithAnimState(uint8_t movement_state, uint8_t walking_state,
                                     float speed_2d_uu = 0.0f);

ConvertedSourceSample ConvertSampleToSource(const TelemetrySample& sample);

const char* CsvHeader();
std::string FormatCsvRow(const TelemetrySample& sample);
bool ParseCsvRow(const std::string& line, TelemetrySample& out_sample);

bool WriteCsvFile(const std::string& path, const std::vector<TelemetrySample>& samples);
bool ReadCsvFile(const std::string& path, std::vector<TelemetrySample>& out_samples);

// Read-only live probe and recording from MirrorsEdge.exe (Windows).
LiveProbeResult ProbeLiveMirrorsEdge();
bool RecordLiveMirrorsEdge(const std::string& csv_path, double duration_sec, int hz,
                           std::size_t* out_rows_written = nullptr);

// Reference trace generators (120 Hz) for the 9 required B2 maneuvers, using Mirror's Edge
// reference constants (ar::me_reference) and aperture_runner::GuestBridge.
std::vector<TelemetrySample> BuildRefSprintStart(int hz = 120);
std::vector<TelemetrySample> BuildRefJump(int hz = 120);
std::vector<TelemetrySample> BuildRefWallrun(bool left_wall, int hz = 120);
std::vector<TelemetrySample> BuildRefWallclimb(int hz = 120);
std::vector<TelemetrySample> BuildRefLedgeGrab(int hz = 120);
std::vector<TelemetrySample> BuildRefSlide(int hz = 120);
std::vector<TelemetrySample> BuildRefHardLanding(int hz = 120);
std::vector<TelemetrySample> BuildRefSkillRoll(int hz = 120);

bool GenerateAllReferenceCsvs(const std::string& output_dir, int hz = 120);

}  // namespace ar::telemetry
