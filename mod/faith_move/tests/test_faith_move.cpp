// Tests for faith_move P1/P2 (ground movement) against ar::testing::BoxWorld. Every value of the
// table in section 6 of PLAN_EQUIPO.md that P2 owns has a check here. Same runner style as
// mod/tests/test_contracts.cpp.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>

#include "ar/faith_movement.h"
#include "ar/testing/box_world.h"
#include "faith_move/faith_params.h"

using namespace ar;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                               \
    do {                                                                                    \
        ++g_checks;                                                                         \
        const double va = (a), vb = (b);                                                    \
        if (std::fabs(va - vb) > (eps)) {                                                   \
            ++g_failures;                                                                   \
            std::printf("  FAIL %s:%d  %s = %g, expected %g\n", __FILE__, __LINE__, #a, va, vb); \
        }                                                                                   \
    } while (0)

constexpr float kDt = source_defaults::kTickInterval;

int Ticks(float seconds) { return static_cast<int>(std::lround(seconds / kDt)); }

// Faith on a floor at z = 0, settled and standing still.
struct Rig {
    std::unique_ptr<IFaithMovement> move = CreateFaithMovement(FaithHostConfig{});
    testing::BoxWorld world;
    FaithState s;
    double time = 0.0;
    uint32_t events = 0;  // OR of every event since the last Clear()
    float last_impact = 0.0f;

    explicit Rig(bool floor = true, Vec3 spawn = {0, 0, 0}) {
        if (floor) world.AddFloor(0.0f);
        move->Reset(s, spawn, {0, 0, 0});
    }

    FaithStepResult Tick(float fwd = 0, float side = 0, uint32_t held = 0, uint32_t pressed = 0, float yaw = 0) {
        FaithInput in;
        in.dt = kDt;
        in.time = time;
        in.forward_move = fwd;
        in.side_move = side;
        in.view_angles = {0, yaw, 0};
        in.buttons = held | pressed;
        in.buttons_pressed = pressed;
        const FaithStepResult r = move->Step(in, s, world);
        time += kDt;
        events |= r.events;
        if (r.events & kFaithEventLanded) last_impact = r.impact_speed;
        return r;
    }

    void Run(int ticks, float fwd = 0, float side = 0, uint32_t held = 0, float yaw = 0) {
        for (int i = 0; i < ticks; ++i) Tick(fwd, side, held, 0, yaw);
    }

    void Settle() {
        Run(10);
        events = 0;
    }
};

void TestResetAndStand() {
    Rig r;
    CHECK(!r.s.on_ground);
    CHECK_NEAR(r.s.eye_height, source_defaults::kViewHeight, 1e-6);
    r.Settle();
    CHECK(r.s.on_ground);
    CHECK(r.s.anim == FaithAnimState::Idle);
    CHECK_NEAR(r.s.origin.z, 0.0, 0.05);
    CHECK_NEAR(r.s.speed_2d, 0.0, 1e-6);
    CHECK_NEAR(r.s.ground_normal.z, 1.0, 1e-6);
}

// Section 6: AccelRate 2419 up to the sprint start (157.5), then the sprint ramp up to 283.5.
void TestRunAndSprintInertia() {
    Rig r;
    r.Settle();
    r.Tick(1);
    CHECK_NEAR(r.s.speed_2d, faith::kGroundAccel * kDt, 0.01);  // one tick of AccelRate

    int t = 1;
    while (r.s.speed_2d < faith::kRunSpeed - 0.01f && t < 100) {
        r.Tick(1);
        ++t;
    }
    CHECK_NEAR(faith::kRunSpeed, 157.48, 0.01);
    CHECK(t * kDt <= 0.08f);  // 157.5 / 2419 = 0.065 s

    int sprint_ticks = 0;
    float at_half_second = 0.0f;
    while (r.s.speed_2d < faith::kSprintMaxSpeed - 0.01f && sprint_ticks < 400) {
        r.Tick(1);
        ++sprint_ticks;
        if (sprint_ticks == Ticks(0.5f)) at_half_second = r.s.speed_2d;
    }
    CHECK_NEAR(sprint_ticks * kDt, faith::kSprintRampSec, 0.05);
    // Inertia: half a second into the sprint Faith is faster than a run but far from full speed.
    CHECK(at_half_second > faith::kRunSpeed + 10.0f && at_half_second < faith::kSprintMaxSpeed - 40.0f);
    CHECK(faith::kSprintRampSec >= 1.0f && faith::kSprintRampSec <= 3.0f);
    r.Run(Ticks(2.0f), 1);
    CHECK_NEAR(r.s.speed_2d, 283.46, 0.01);  // GroundSpeed 720 uu
    CHECK(r.s.anim == FaithAnimState::Sprint);
    CHECK(r.s.on_ground);
    CHECK_NEAR(r.s.origin.z, 0.0, 0.05);
    CHECK_NEAR(r.s.velocity.y, 0.0, 1e-3);  // yaw 0 runs along +X
    CHECK(r.s.velocity.x > 0.0f);
}

void TestWalkStrafeAndStop() {
    Rig walk;
    walk.Settle();
    walk.Run(Ticks(2.0f), 1, 0, kFaithButtonWalk);
    CHECK_NEAR(walk.s.speed_2d, faith::kWalkSpeed, 0.01);
    CHECK(walk.s.anim == FaithAnimState::Walk);

    Rig strafe;  // sideways never sprints
    strafe.Settle();
    strafe.Run(Ticks(3.0f), 0, 1);
    CHECK_NEAR(strafe.s.speed_2d, faith::kRunSpeed, 0.01);
    CHECK(strafe.s.anim == FaithAnimState::Run);
    CHECK(strafe.s.velocity.y < 0.0f);  // +side = right = -Y at yaw 0

    Rig stop;
    stop.Settle();
    stop.Run(Ticks(3.0f), 1);
    stop.Run(Ticks(0.15f));
    CHECK_NEAR(stop.s.speed_2d, 0.0, 1e-3);  // 283.5 / 2419 = 0.12 s
    CHECK(stop.s.anim == FaithAnimState::Idle);
}

void TestTurnsAndInertia() {
    Rig gentle;  // turning 90 deg/s at full sprint keeps the sprint
    gentle.Settle();
    gentle.Run(Ticks(3.0f), 1);
    for (int i = 0; i < Ticks(1.0f); ++i) gentle.Tick(1, 0, 0, 0, 90.0f * static_cast<float>(i + 1) * kDt);
    CHECK(gentle.s.speed_2d > 280.0f);
    CHECK(gentle.s.anim == FaithAnimState::Sprint);

    Rig sharp;  // a 180 loses it
    sharp.Settle();
    sharp.Run(Ticks(3.0f), 1);
    sharp.Run(Ticks(0.5f), 1, 0, 0, 180.0f);
    CHECK(sharp.s.velocity.x < 0.0f);
    CHECK(sharp.s.speed_2d < 200.0f);
}

// Section 6: BaseJumpZ 248, gravity 646 -> apex v^2 / 2g = 47.6 su above the floor.
void TestJumpAndGravity() {
    Rig r;
    r.Settle();
    const FaithStepResult jump = r.Tick(0, 0, 0, kFaithButtonUp);
    CHECK(jump.events & kFaithEventJumped);
    CHECK_NEAR(r.s.velocity.z, 248.03, 0.01);
    CHECK(!r.s.on_ground);
    CHECK(r.s.anim == FaithAnimState::Jump);

    float apex = 0.0f;
    const float vz0 = r.s.velocity.z;
    r.Tick();
    CHECK_NEAR(vz0 - r.s.velocity.z, faith::kGravity * kDt, 1e-3);  // 646 su/s^2
    CHECK_NEAR(faith::kGravity, 646.06, 0.01);
    for (int i = 0; i < 200 && !r.s.on_ground; ++i) {
        apex = std::fmax(apex, r.s.origin.z);
        if (r.s.velocity.z <= 0.0f && !r.s.on_ground) CHECK(r.s.anim == FaithAnimState::Fall);
        r.Tick();
    }
    CHECK_NEAR(apex, 248.03 * 248.03 / (2.0 * 646.06), 2.0);
    CHECK(r.s.on_ground);
    CHECK(r.events & kFaithEventLanded);
    CHECK(!(r.events & kFaithEventHardLanding));
    CHECK(r.s.anim == FaithAnimState::LandSoft);
    CHECK_NEAR(r.last_impact, 248.0, 12.0);  // lands about as fast as it left

    // Holding Up does not bunny-hop: only a fresh press jumps.
    r.Run(Ticks(0.5f), 0, 0, kFaithButtonUp);
    CHECK(r.s.on_ground);
}

void TestSprintJumpAddsSpeed() {
    Rig r;
    r.Settle();
    r.Run(Ticks(0.5f), 1);
    const float before = r.s.speed_2d;
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK_NEAR(r.s.speed_2d, before + faith::kJumpAddXY, 0.5);  // JumpAddXY 100 uu = 39.4 su

    Rig fast;
    fast.Settle();
    fast.Run(Ticks(3.0f), 1);
    fast.Tick(1, 0, 0, kFaithButtonUp);
    CHECK_NEAR(fast.s.speed_2d, faith::kSprintMaxSpeed + faith::kJumpAddXY, 0.5);
    fast.Run(Ticks(0.3f), 1);  // air control does not add more
    CHECK(fast.s.speed_2d <= faith::kSprintMaxSpeed + faith::kJumpAddXY + 0.01f);
}

// Section 6: HardLandingHeight 530 uu = 208.7 su.
void TestLandings() {
    Rig soft(true, {0, 0, 100});
    soft.Run(Ticks(2.0f));
    CHECK(soft.s.on_ground);
    CHECK(soft.events & kFaithEventLanded);
    CHECK(!(soft.events & kFaithEventHardLanding));

    Rig hard(true, {0, 0, 0});
    hard.world.Add({-64, -64, 0}, {64, 64, 300});  // tower 300 su high
    hard.move->Reset(hard.s, {0, 0, 300}, {0, 0, 0});
    hard.Settle();
    CHECK(hard.s.on_ground);
    CHECK_NEAR(hard.s.origin.z, 300.0, 0.05);
    // Sprint off the edge of the tower.
    for (int i = 0; i < Ticks(4.0f) && !(hard.events & kFaithEventLanded); ++i) hard.Tick(1);
    CHECK(hard.events & kFaithEventLanded);
    CHECK(hard.events & kFaithEventHardLanding);
    CHECK(hard.s.anim == FaithAnimState::LandHard);
    CHECK(hard.s.speed_2d <= faith::kHardLandingMaxSpeed + 0.01f);
    CHECK_NEAR(hard.s.origin.z, 0.0, 0.05);
    hard.Tick(1, 0, 0, kFaithButtonUp);  // locked: no jump, no acceleration
    CHECK(hard.s.on_ground);
    hard.Run(Ticks(faith::kHardLandingLockSec), 1);
    CHECK(hard.s.anim != FaithAnimState::LandHard);

    Rig edge(true, {0, 0, 0});  // just under the threshold stays soft
    edge.world.Add({-64, -64, 0}, {64, 64, 200});
    edge.move->Reset(edge.s, {0, 0, 200}, {0, 0, 0});
    edge.Settle();
    for (int i = 0; i < Ticks(4.0f) && !(edge.events & kFaithEventLanded); ++i) edge.Tick(1);
    CHECK(edge.events & kFaithEventLanded);
    CHECK(!(edge.events & kFaithEventHardLanding));

    Rig high(true, {0, 0, 500});  // FallingUncontrolledHeight 1000 uu = 393.7 su
    bool uncontrolled = false;
    for (int i = 0; i < Ticks(3.0f) && !high.s.on_ground; ++i) {
        high.Tick();
        uncontrolled |= high.s.anim == FaithAnimState::FallUncontrolled;
    }
    CHECK(uncontrolled);
    CHECK(high.events & kFaithEventHardLanding);
}

void TestWallsStepsAndCube() {
    Rig wall;
    wall.world.Add({100, -256, 0}, {132, 256, 200});
    wall.Settle();
    wall.Run(Ticks(2.0f), 1);
    CHECK(wall.s.origin.x <= 84.0f && wall.s.origin.x > 83.5f);  // hull half-width 16
    CHECK_NEAR(wall.s.velocity.x, 0.0, 1e-3);
    CHECK(wall.s.on_ground);

    Rig slide;  // running into a wall at 45 degrees slides along it
    slide.world.Add({100, -1024, 0}, {132, 1024, 200});
    slide.Settle();
    slide.Run(Ticks(2.0f), 1, 0, 0, 45.0f);
    CHECK(slide.s.origin.x <= 84.0f);
    CHECK(slide.s.origin.y > 100.0f);

    Rig step;  // 16 su step: climbs it
    step.world.Add({64, -256, 0}, {512, 256, 16});
    step.Settle();
    step.Run(Ticks(1.0f), 1);
    CHECK(step.s.origin.x > 100.0f);
    CHECK_NEAR(step.s.origin.z, 16.0, 0.05);
    CHECK(step.s.on_ground);
    step.Run(Ticks(3.0f), 1, 0, 0, 180.0f);  // and walks back down it without leaving the floor
    CHECK(step.s.origin.x < 0.0f);
    CHECK_NEAR(step.s.origin.z, 0.0, 0.05);
    CHECK(!(step.events & kFaithEventHardLanding));

    Rig tall;  // 24 su is more than step_height (18): blocked
    tall.world.Add({64, -256, 0}, {512, 256, 24});
    tall.Settle();
    tall.Run(Ticks(1.0f), 1);
    CHECK(tall.s.origin.x <= 48.0f);
    CHECK_NEAR(tall.s.origin.z, 0.0, 0.05);

    Rig cube;  // the cube is solid too (it is only excluded from wallruns, P3)
    cube.world.Add({64, -16, 0}, {96, 16, 32}, TraceHitKind::Physics);
    cube.world.Add({64, -16, 32}, {96, 16, 200});  // stacked high so it cannot be stepped onto
    cube.Settle();
    cube.Run(Ticks(1.0f), 1);
    CHECK(cube.s.origin.x <= 48.0f);
}

void TestCoyoteTime() {
    auto off_ledge = [](float delay) {
        Rig r(false, {0, 0, 100});
        r.world.Add({-512, -256, 84}, {64, 256, 100});
        r.Settle();
        int t = 0;
        while (r.s.on_ground && t < 200) {
            r.Tick(1, 0, kFaithButtonWalk);
            ++t;
        }
        r.Run(Ticks(delay), 1, 0, kFaithButtonWalk);
        return (r.Tick(1, 0, kFaithButtonWalk, kFaithButtonUp).events & kFaithEventJumped) != 0;
    };
    CHECK(off_ledge(0.045f));
    CHECK(!off_ledge(0.2f));
}

// Prediction: save, run, restore, run again -> bit-identical, as Source requires.
void TestPredictionReplay() {
    Rig r;
    r.world.Add({300, -64, 0}, {400, 64, 16}).Add({600, -256, 0}, {640, 256, 300});
    r.Settle();
    auto script = [](Rig& rig, int from, int to) {
        for (int i = from; i < to; ++i) {
            const uint32_t pressed = (i % 90 == 45) ? kFaithButtonUp : 0u;
            rig.Tick(1.0f, (i / 60) % 2 ? 0.3f : -0.3f, 0, pressed, static_cast<float>(i) * 0.4f);
        }
    };
    script(r, 0, 100);
    const FaithState saved = r.s;
    script(r, 100, 400);
    const FaithState first = r.s;
    r.s = saved;
    script(r, 100, 400);
    CHECK(std::memcmp(&r.s, &first, sizeof(FaithState)) == 0);
}

}  // namespace

int main() {
    std::printf("faith_move tests (%s), contract v%u\n", sizeof(void*) == 8 ? "x64" : "x86",
                kFaithMovementContractVersion);
    TestResetAndStand();
    TestRunAndSprintInertia();
    TestWalkStrafeAndStop();
    TestTurnsAndInertia();
    TestJumpAndGravity();
    TestSprintJumpAddsSpeed();
    TestLandings();
    TestWallsStepsAndCube();
    TestCoyoteTime();
    TestPredictionReplay();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
