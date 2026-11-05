// Deterministic numerical unit tests for the Mirror's Edge camera module (Task G3).
// Owner: Gonzalo Robles (mod/camera/).
#include "camera_feel.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace ar;
using namespace ar::camera;

namespace {

int g_failures = 0;
int g_checks = 0;

void CheckImpl(bool cond, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL %s:%d  %s\n", file, line, expr);
    }
}

void CheckNearImpl(double actual, double expected, double eps, const char* expr,
                   const char* file, int line) {
    ++g_checks;
    if (std::fabs(actual - expected) > eps) {
        ++g_failures;
        std::printf("  FAIL %s:%d  %s = %.6f, expected %.6f (eps=%.6f)\n",
                    file, line, expr, actual, expected, eps);
    }
}

#define CHECK(cond) CheckImpl((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) CheckNearImpl(static_cast<double>(a), static_cast<double>(b), \
                                            static_cast<double>(eps), #a, __FILE__, __LINE__)

void TestRollExponentialDecay() {
    CameraConfig cfg{};
    cfg.roll_decay_lambda = 8.5f;
    CameraController ctrl(cfg);
    CameraState cam{};

    // Start with a 15-degree roll angle and let it decay with no wallrun active.
    cam.current_roll_deg = 15.0f;
    FaithState faith{};
    faith.anim = FaithAnimState::Idle;
    faith.view_roll = 0.0f;

    const float dt = source_defaults::kTickInterval;  // 0.015 s (66.67 Hz)
    const CameraOutput out1 = ctrl.Update(faith, dt, cam);
    const float expected1 = 15.0f * std::exp(-8.5f * dt);
    CHECK_NEAR(out1.angle_offset.z, expected1, 1e-5f);
    CHECK_NEAR(cam.current_roll_deg, expected1, 1e-5f);

    // Run 39 more ticks (total 40 ticks = 0.60 s): exact semigroup property exp(-lambda * N * dt).
    for (int i = 1; i < 40; ++i) {
        ctrl.Update(faith, dt, cam);
    }
    const float expected40 = 15.0f * std::exp(-8.5f * 40.0f * dt);
    CHECK_NEAR(cam.current_roll_deg, expected40, 1e-4f);
}

void TestWallrunRollAndVerticalPitchGuard() {
    CameraController ctrl{};
    CameraState cam{};
    FaithState faith{};
    faith.anim = FaithAnimState::WallRunLeft;
    faith.view_roll = 14.0f;

    const float dt = source_defaults::kTickInterval;
    for (int i = 0; i < 60; ++i) {
        ctrl.Update(faith, dt, cam, {}, 0.0f);
    }
    // After 0.9s of wallrun left, roll should have converged close to +14 deg.
    CHECK(cam.current_roll_deg > 13.9f);
    CHECK(cam.current_roll_deg <= 14.0f);

    // Looking straight up (-88 deg) or straight down (+88 deg) must immediately zero roll.
    const CameraOutput up_out = ctrl.Update(faith, dt, cam, {}, -88.0f);
    CHECK_NEAR(up_out.angle_offset.z, 0.0f, 1e-6f);
    CHECK_NEAR(cam.current_roll_deg, 0.0f, 1e-6f);

    cam.current_roll_deg = -12.0f;
    const CameraOutput down_out = ctrl.Update(faith, dt, cam, {}, 86.0f);
    CHECK_NEAR(down_out.angle_offset.z, 0.0f, 1e-6f);
    CHECK_NEAR(DecayRoll(10.0f, dt, 8.5f, 89.0f), 0.0f, 1e-6f);
    CHECK_NEAR(DecayRoll(10.0f, dt, 8.5f, -89.0f), 0.0f, 1e-6f);
}

void TestViewBobbingRunAndAirborneDecay() {
    CameraController ctrl{};
    CameraState cam{};
    FaithState faith{};
    const float dt = source_defaults::kTickInterval;

    // Stationary on ground -> zero bobbing.
    faith.on_ground = true;
    faith.anim = FaithAnimState::Idle;
    faith.speed_2d = 0.0f;
    CameraOutput idle_out = ctrl.Update(faith, dt, cam);
    CHECK_NEAR(idle_out.position_offset.z, 0.0f, 1e-6f);
    CHECK_NEAR(idle_out.position_offset.y, 0.0f, 1e-6f);

    // Sprinting on ground at 283.5 su/s -> bobbing envelope builds up and oscillates.
    faith.anim = FaithAnimState::Sprint;
    faith.speed_2d = UuToSu(me_reference::kSprintMaxSpeedUu);
    float max_abs_z = 0.0f;
    float max_abs_y = 0.0f;
    for (int i = 0; i < 60; ++i) {
        CameraOutput sprint_out = ctrl.Update(faith, dt, cam);
        max_abs_z = std::fmax(max_abs_z, std::fabs(sprint_out.position_offset.z));
        max_abs_y = std::fmax(max_abs_y, std::fabs(sprint_out.position_offset.y));
    }
    CHECK(max_abs_z > 0.8f);
    CHECK(max_abs_y > 0.5f);
    CHECK(cam.bob_weight > 0.9f);

    // Jump into the air -> bobbing decays smoothly to zero.
    faith.on_ground = false;
    faith.anim = FaithAnimState::Jump;
    for (int i = 0; i < 80; ++i) {
        ctrl.Update(faith, dt, cam);
    }
    CHECK(cam.bob_weight < 0.01f);
}

void TestDynamicFovScaling() {
    CameraController ctrl{};
    CameraState cam{};
    FaithState faith{};
    const float dt = source_defaults::kTickInterval;

    // Below sprint start speed (157.48 su/s) -> FOV offset remains 0.
    faith.on_ground = true;
    faith.anim = FaithAnimState::Walk;
    faith.speed_2d = 120.0f;
    for (int i = 0; i < 30; ++i) {
        ctrl.Update(faith, dt, cam);
    }
    CHECK_NEAR(cam.current_fov_offset_deg, 0.0f, 1e-5f);

    // Full sprint (283.46 su/s) -> FOV approaches fov_max_sprint_offset_deg (+8 deg).
    faith.anim = FaithAnimState::Sprint;
    faith.speed_2d = UuToSu(me_reference::kSprintMaxSpeedUu);
    float prev_fov = 0.0f;
    for (int i = 0; i < 90; ++i) {
        const CameraOutput out = ctrl.Update(faith, dt, cam);
        CHECK(out.fov_offset_deg >= prev_fov);
        prev_fov = out.fov_offset_deg;
    }
    CHECK_NEAR(cam.current_fov_offset_deg, ctrl.Config().fov_max_sprint_offset_deg, 0.1f);

    // High-speed portal fling (600 su/s) -> clamped at fov_max_fling_offset_deg (+12 deg).
    faith.on_ground = false;
    faith.anim = FaithAnimState::Fall;
    faith.speed_2d = 600.0f;
    for (int i = 0; i < 100; ++i) {
        ctrl.Update(faith, dt, cam);
    }
    CHECK_NEAR(cam.current_fov_offset_deg, ctrl.Config().fov_max_fling_offset_deg, 0.1f);
}

void TestLandingShakeAndSkillRoll() {
    CameraController ctrl{};
    const float dt = source_defaults::kTickInterval;

    // Compare soft vs hard landing peak dip and pitch kick.
    CameraState cam_soft{};
    CameraState cam_hard{};
    FaithState faith{};
    faith.on_ground = true;

    FaithStepResult soft_ev{};
    soft_ev.events = kFaithEventLanded;
    soft_ev.impact_speed = 250.0f;

    FaithStepResult hard_ev{};
    hard_ev.events = kFaithEventLanded | kFaithEventHardLanding;
    hard_ev.impact_speed = 420.0f;

    ctrl.Update(faith, dt, cam_soft, soft_ev);
    ctrl.Update(faith, dt, cam_hard, hard_ev);

    float max_soft_dip = 0.0f;
    float max_hard_dip = 0.0f;
    for (int i = 0; i < 40; ++i) {
        const CameraOutput so = ctrl.Update(faith, dt, cam_soft);
        const CameraOutput ho = ctrl.Update(faith, dt, cam_hard);
        max_soft_dip = std::fmax(max_soft_dip, -so.position_offset.z);
        max_hard_dip = std::fmax(max_hard_dip, -ho.position_offset.z);
    }
    CHECK(max_soft_dip > 1.0f);
    CHECK(max_hard_dip > max_soft_dip);

    // Skill Roll somersault: 0 -> 360 deg pitch progression and mid-roll vertical dip.
    CameraState cam_roll{};
    FaithState roll_state{};
    roll_state.on_ground = true;
    roll_state.anim = FaithAnimState::SkillRoll;
    FaithStepResult roll_ev{};
    roll_ev.events = kFaithEventSkillRoll;

    const CameraOutput start_out = ctrl.Update(roll_state, dt, cam_roll, roll_ev);
    CHECK(cam_roll.skill_roll_active);
    CHECK(start_out.angle_offset.x > 0.0f);

    // Advance to approximately half of skill_roll_duration_sec (0.325 s ~ 21 more ticks).
    CameraOutput mid_out{};
    for (int i = 0; i < 20; ++i) {
        mid_out = ctrl.Update(roll_state, dt, cam_roll);
    }
    // Near midpoint: somersault pitch is around 180 deg and vertical dip is near -18 su.
    CHECK_NEAR(mid_out.angle_offset.x, 180.0f, 25.0f);
    CHECK(mid_out.position_offset.z < -15.0f);

    // Finish the roll: camera somersault ends cleanly.
    for (int i = 0; i < 30; ++i) {
        ctrl.Update(roll_state, dt, cam_roll);
    }
    CHECK(!cam_roll.skill_roll_active);
}

void TestPredictionRewindDeterminism() {
    CameraController ctrl{};
    CameraState cam{};
    FaithState faith{};
    faith.on_ground = true;
    faith.anim = FaithAnimState::Sprint;
    faith.speed_2d = 260.0f;
    faith.view_roll = 8.0f;
    const float dt = source_defaults::kTickInterval;

    for (int i = 0; i < 15; ++i) {
        ctrl.Update(faith, dt, cam);
    }

    // Snapshot state at tick 15.
    const CameraState snapshot = cam;

    CameraOutput first_run[10]{};
    for (int i = 0; i < 10; ++i) {
        first_run[i] = ctrl.Update(faith, dt, cam);
    }
    const CameraState end_first = cam;

    // Rewind and replay the same 10 ticks.
    cam = snapshot;
    for (int i = 0; i < 10; ++i) {
        const CameraOutput replay = ctrl.Update(faith, dt, cam);
        CHECK(std::memcmp(&replay, &first_run[i], sizeof(CameraOutput)) == 0);
    }
    CHECK(std::memcmp(&cam, &end_first, sizeof(CameraState)) == 0);
}

}  // namespace

int main() {
    TestRollExponentialDecay();
    TestWallrunRollAndVerticalPitchGuard();
    TestViewBobbingRunAndAirborneDecay();
    TestDynamicFovScaling();
    TestLandingShakeAndSkillRoll();
    TestPredictionRewindDeterminism();

    std::printf("test_camera: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
