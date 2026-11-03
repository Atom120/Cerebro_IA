// Deterministic unit tests for Mirror's Edge telemetry recorder & mapper (B2).
// Owner: Benja (mod/tools/me_telemetry/).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "me_telemetry.h"

using namespace ar;
using namespace ar::telemetry;

namespace {

int g_checks = 0;
int g_failures = 0;

void CheckImpl(bool cond, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL %s:%d  %s\n", file, line, expr);
    }
}

void CheckNearImpl(double a, double b, double eps, const char* expr_a, const char* file, int line) {
    ++g_checks;
    if (std::fabs(a - b) > eps) {
        ++g_failures;
        std::printf("  FAIL %s:%d  %s = %g, expected %g\n", file, line, expr_a, a, b);
    }
}

#define CHECK_TRUE(cond) CheckImpl((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR_VAL(a, b, eps) CheckNearImpl((a), (b), (eps), #a, __FILE__, __LINE__)

void TestGogOffsetsAndEnums() {
    CHECK_TRUE(gog_v101::kImageBase == 0x00400000u);
    CHECK_TRUE(gog_v101::kPeTimestamp == 0x4965FFF1u);
    CHECK_TRUE(gog_v101::kGEngineVa == 0x01FF8B20u);
    CHECK_TRUE(gog_v101::kOffsetGamePlayersData == 0x02BCu);
    CHECK_TRUE(gog_v101::kOffsetLocalPlayerActor == 0x0040u);
    CHECK_TRUE(gog_v101::kOffsetControllerPawn == 0x01C0u);
    CHECK_TRUE(gog_v101::kOffsetActorLocation == 0x00E8u);
    CHECK_TRUE(gog_v101::kOffsetActorVelocity == 0x0100u);
    CHECK_TRUE(gog_v101::kOffsetTdPawnMovementState == 0x04FEu);
    CHECK_TRUE(gog_v101::kOffsetTdPawnWalkingState == 0x0503u);

    CHECK_TRUE(std::strcmp(MeMovementStateName(0), "MOVE_None") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(1), "MOVE_Walking") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(4), "MOVE_WallRunningRight") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(5), "MOVE_WallRunningLeft") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(6), "MOVE_WallClimbing") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(16), "MOVE_Slide") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(61), "MOVE_Coil") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(91), "MOVE_SkillRoll") == 0);
    CHECK_TRUE(std::strcmp(MeMovementStateName(93), "MOVE_Cutscene") == 0);

    CHECK_TRUE(std::strcmp(MeWalkingStateName(0), "WAS_Idle") == 0);
    CHECK_TRUE(std::strcmp(MeWalkingStateName(2), "WAS_Walk") == 0);
    CHECK_TRUE(std::strcmp(MeWalkingStateName(4), "WAS_Run") == 0);
    CHECK_TRUE(std::strcmp(MeWalkingStateName(5), "WAS_Sprint") == 0);
}

void TestStateMappingAndUnitConversion() {
    TelemetrySample s{};
    s.t = 1.25;
    s.x = 254.0f;
    s.y = -508.0f;
    s.z = 180.0f;
    s.vx = 720.0f;
    s.vy = 0.0f;
    s.vz = 0.0f;
    s.movement_state = static_cast<uint8_t>(MeMovementState::MOVE_Walking);
    s.walking_state = static_cast<uint8_t>(MeWalkingState::WAS_Sprint);

    const ConvertedSourceSample conv = ConvertSampleToSource(s);
    CHECK_NEAR_VAL(conv.t, 1.25, 1e-6);
    // Source = uu / 2.54, with Y axis mirrored.
    CHECK_NEAR_VAL(conv.origin_su.x, 100.0, 1e-3);
    CHECK_NEAR_VAL(conv.origin_su.y, 200.0, 1e-3);
    CHECK_NEAR_VAL(conv.origin_su.z, 70.866, 1e-2);
    CHECK_NEAR_VAL(conv.velocity_su.x, 283.465, 1e-2);
    CHECK_NEAR_VAL(conv.speed_2d_su, 283.465, 1e-2);
    CHECK_TRUE(conv.td_state == aperture_runner::TdMovementState::Sprinting);
    CHECK_TRUE(conv.faith_anim == FaithAnimState::Sprint);

    // Check parkour state mappings
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_WallRunningLeft),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::WallRunLeft);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_WallRunningRight),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::WallRunRight);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_WallClimbing),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::WallClimb);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_Grabbing),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::LedgeHang);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_GrabPullUp),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::LedgeClimbUp);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_Slide),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::Slide);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_Landing),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::LandHard);
    CHECK_TRUE(MapMeToFaithAnimState(static_cast<uint8_t>(MeMovementState::MOVE_SkillRoll),
                                     static_cast<uint8_t>(MeWalkingState::WAS_None)) ==
               FaithAnimState::SkillRoll);
}

void TestCsvRoundTrip() {
    CHECK_TRUE(std::strcmp(CsvHeader(), "t,x,y,z,vx,vy,vz,movement_state,walking_state") == 0);

    TelemetrySample in{};
    in.t = 0.500000;
    in.x = 123.456f;
    in.y = -789.125f;
    in.z = 90.000f;
    in.vx = 720.000f;
    in.vy = -12.500f;
    in.vz = 630.000f;
    in.movement_state = static_cast<uint8_t>(MeMovementState::MOVE_Jump);
    in.walking_state = static_cast<uint8_t>(MeWalkingState::WAS_None);

    const std::string line = FormatCsvRow(in);
    TelemetrySample out{};
    CHECK_TRUE(ParseCsvRow(line, out));
    CHECK_NEAR_VAL(out.t, in.t, 1e-5);
    CHECK_NEAR_VAL(out.x, in.x, 1e-2);
    CHECK_NEAR_VAL(out.y, in.y, 1e-2);
    CHECK_NEAR_VAL(out.z, in.z, 1e-2);
    CHECK_NEAR_VAL(out.vx, in.vx, 1e-2);
    CHECK_NEAR_VAL(out.vy, in.vy, 1e-2);
    CHECK_NEAR_VAL(out.vz, in.vz, 1e-2);
    CHECK_TRUE(out.movement_state == in.movement_state);
    CHECK_TRUE(out.walking_state == in.walking_state);

    TelemetrySample dummy{};
    CHECK_TRUE(!ParseCsvRow(CsvHeader(), dummy));
    CHECK_TRUE(!ParseCsvRow("", dummy));
}

bool HasMovementState(const std::vector<TelemetrySample>& clip, MeMovementState target) {
    for (const auto& s : clip) {
        if (s.movement_state == static_cast<uint8_t>(target)) {
            return true;
        }
    }
    return false;
}

void TestReferenceTrajectories() {
    const auto sprint = BuildRefSprintStart(120);
    CHECK_TRUE(sprint.size() > 300u);
    CHECK_TRUE(sprint.front().walking_state == static_cast<uint8_t>(MeWalkingState::WAS_Idle));
    CHECK_TRUE(sprint.back().walking_state == static_cast<uint8_t>(MeWalkingState::WAS_Sprint));
    CHECK_NEAR_VAL(sprint.back().vx, me_reference::kSprintMaxSpeedUu, 1.0);

    const auto jump = BuildRefJump(120);
    CHECK_TRUE(HasMovementState(jump, MeMovementState::MOVE_Jump));
    CHECK_TRUE(HasMovementState(jump, MeMovementState::MOVE_Falling));
    CHECK_TRUE(HasMovementState(jump, MeMovementState::MOVE_SoftLanding));

    const auto wr_left = BuildRefWallrun(true, 120);
    CHECK_TRUE(HasMovementState(wr_left, MeMovementState::MOVE_WallRunningLeft));
    CHECK_TRUE(HasMovementState(wr_left, MeMovementState::MOVE_WallRunJump));

    const auto wr_right = BuildRefWallrun(false, 120);
    CHECK_TRUE(HasMovementState(wr_right, MeMovementState::MOVE_WallRunningRight));
    CHECK_TRUE(HasMovementState(wr_right, MeMovementState::MOVE_WallRunJump));

    const auto wc = BuildRefWallclimb(120);
    CHECK_TRUE(HasMovementState(wc, MeMovementState::MOVE_WallClimbing));
    CHECK_TRUE(HasMovementState(wc, MeMovementState::MOVE_WallClimb180TurnJump));

    const auto ledge = BuildRefLedgeGrab(120);
    CHECK_TRUE(HasMovementState(ledge, MeMovementState::MOVE_IntoGrab));
    CHECK_TRUE(HasMovementState(ledge, MeMovementState::MOVE_Grabbing));
    CHECK_TRUE(HasMovementState(ledge, MeMovementState::MOVE_GrabPullUp));

    const auto slide = BuildRefSlide(120);
    CHECK_TRUE(HasMovementState(slide, MeMovementState::MOVE_Slide));
    CHECK_TRUE(HasMovementState(slide, MeMovementState::MOVE_Crouch));

    const auto hard_land = BuildRefHardLanding(120);
    CHECK_TRUE(HasMovementState(hard_land, MeMovementState::MOVE_FallingUncontrolled));
    CHECK_TRUE(HasMovementState(hard_land, MeMovementState::MOVE_Landing));
    // GuestBridge::OnLanded zeroes horizontal velocity during hard landing stagger.
    for (const auto& s : hard_land) {
        if (s.movement_state == static_cast<uint8_t>(MeMovementState::MOVE_Landing)) {
            CHECK_NEAR_VAL(s.vx, 0.0, 1e-3);
            break;
        }
    }

    const auto skill_roll = BuildRefSkillRoll(120);
    CHECK_TRUE(HasMovementState(skill_roll, MeMovementState::MOVE_Coil));
    CHECK_TRUE(HasMovementState(skill_roll, MeMovementState::MOVE_SkillRoll));
    // GuestBridge::OnLanded preserves horizontal velocity when Skill Roll succeeds.
    for (const auto& s : skill_roll) {
        if (s.movement_state == static_cast<uint8_t>(MeMovementState::MOVE_SkillRoll)) {
            CHECK_TRUE(s.vx > 500.0f);
            break;
        }
    }
}

}  // namespace

int main() {
    std::printf("ApertureRunner me_telemetry tests (%s)\n", sizeof(void*) == 8 ? "x64" : "x86");
    TestGogOffsetsAndEnums();
    TestStateMappingAndUnitConversion();
    TestCsvRoundTrip();
    TestReferenceTrajectories();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
