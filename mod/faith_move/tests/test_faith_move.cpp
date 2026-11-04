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

// ------------------------------------------------------------------ P3: parkour

// Runs until `done` or `max_ticks`; returns the ticks taken.
int RunUntil(Rig& r, int max_ticks, const std::function<bool()>& done, float fwd = 1, uint32_t held = 0) {
    int t = 0;
    while (!done() && t < max_ticks) {
        r.Tick(fwd, 0, held);
        ++t;
    }
    return t;
}

void TestWallRun() {
    Rig r(true, {0, -80, 0});
    r.world.Add({-512, -140, 0}, {8192, -100, 300});  // wall on the right of a run along +X
    r.Settle();
    r.Run(Ticks(1.5f), 1);
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(r.events & kFaithEventWallRunStart);
    CHECK(!(r.events & kFaithEventJumped));
    CHECK(r.s.anim == FaithAnimState::WallRunRight);
    CHECK(!r.s.on_ground);
    CHECK_NEAR(r.s.wall_normal.y, 1.0, 1e-3);
    CHECK(r.s.view_roll < 0.0f);

    float max_z = 0.0f;
    const int ticks = 1 + RunUntil(r, 300, [&] {
        max_z = std::fmax(max_z, r.s.origin.z);
        return r.s.anim != FaithAnimState::WallRunRight;
    });
    CHECK_NEAR(ticks * kDt, faith::kWallrunDurationSec, 0.05);  // forward held: it ends by time, not speed
    CHECK(max_z > 40.0f && max_z < 80.0f);
    CHECK(r.events & kFaithEventWallRunEnd);
    CHECK(r.s.speed_2d > 250.0f);

    // Holding Up does not run the same wall again before landing.
    r.events = 0;
    RunUntil(r, 300, [&] { return r.s.on_ground; }, 1, kFaithButtonUp);
    CHECK(r.s.on_ground);
    CHECK(!(r.events & kFaithEventWallRunStart));
    CHECK(!(r.events & kFaithEventHardLanding));

    // Without forward held the run slows down and ends earlier.
    Rig slow(true, {0, -80, 0});
    slow.world.Add({-512, -140, 0}, {8192, -100, 300});
    slow.Settle();
    slow.Run(Ticks(0.3f), 1);
    slow.Tick(1, 0, 0, kFaithButtonUp);
    const int slow_ticks = 1 + RunUntil(slow, 300, [&] { return slow.s.anim != FaithAnimState::WallRunRight; }, 0);
    CHECK(slow_ticks * kDt < faith::kWallrunDurationSec - 0.2f);
}

void TestWallRunLeftAndWallJump() {
    Rig r(true, {0, 80, 0});
    r.world.Add({-512, 100, 0}, {8192, 140, 300});  // wall on the left
    r.Settle();
    r.Run(Ticks(1.0f), 1);
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(r.s.anim == FaithAnimState::WallRunLeft);
    CHECK(r.s.view_roll > 0.0f);
    r.Run(Ticks(0.3f), 1);
    CHECK(r.s.anim == FaithAnimState::WallRunLeft);
    r.events = 0;
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(r.events & kFaithEventJumped);
    CHECK(r.events & kFaithEventWallRunEnd);
    CHECK(r.s.anim == FaithAnimState::WallRunJump);
    CHECK(r.s.velocity.y < -100.0f);  // pushed away from the wall (normal -Y)
    CHECK(r.s.velocity.z > 200.0f);
}

void TestWallRunRefused() {
    Rig cube(true, {0, -80, 0});  // physics props are never wallrunnable
    cube.world.Add({-512, -140, 0}, {8192, -100, 300}, TraceHitKind::Physics);
    cube.Settle();
    cube.Run(Ticks(1.0f), 1);
    cube.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(!(cube.events & kFaithEventWallRunStart));
    CHECK(cube.events & kFaithEventJumped);

    Rig low(true, {0, -80, 0});  // a wall lower than kWallrunMinWallHeight (75.6) is not
    low.world.Add({-512, -140, 0}, {8192, -100, 60});
    low.Settle();
    low.Run(Ticks(1.0f), 1);
    low.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(!(low.events & kFaithEventWallRunStart));

    Rig slow(true, {0, -80, 0});  // walking (59 su/s) is under kWallrunMinSpeed (78.7)
    slow.world.Add({-512, -140, 0}, {8192, -100, 300});
    slow.Settle();
    slow.Run(Ticks(1.0f), 1, 0, kFaithButtonWalk);
    slow.Tick(1, 0, kFaithButtonWalk, kFaithButtonUp);
    CHECK(!(slow.events & kFaithEventWallRunStart));
}

void TestWallClimb() {
    Rig r;
    r.world.Add({100, -256, 0}, {400, 256, 400});
    r.Settle();
    r.Run(Ticks(0.6f), 1);  // touching the wall
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(r.events & kFaithEventWallClimbStart);
    CHECK(!(r.events & kFaithEventJumped));
    CHECK(r.s.anim == FaithAnimState::WallClimb);
    CHECK_NEAR(r.s.wall_normal.x, -1.0, 1e-3);

    float max_z = 0.0f;
    RunUntil(r, 300, [&] {
        max_z = std::fmax(max_z, r.s.origin.z);
        return r.s.anim != FaithAnimState::WallClimb;
    });
    const double v = faith::kWallClimbSpeed;
    CHECK_NEAR(max_z, v * v / (2.0 * faith::kGravity * faith::kWallClimbGravityScale), 3.0);
    CHECK(r.s.anim == FaithAnimState::Fall);

    r.events = 0;  // only one wallclimb per airtime
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(!(r.events & kFaithEventWallClimbStart));
    RunUntil(r, 300, [&] { return r.s.on_ground; });
    r.Run(5, 1);
    r.Tick(1, 0, 0, kFaithButtonUp);  // landed: allowed again
    CHECK(r.events & kFaithEventWallClimbStart);

    Rig turn;  // climb, turn away from the wall, jump off it
    turn.world.Add({100, -256, 0}, {400, 256, 400});
    turn.Settle();
    turn.Run(Ticks(0.6f), 1);
    turn.Tick(1, 0, 0, kFaithButtonUp);
    turn.Run(Ticks(0.2f), 1);
    turn.events = 0;
    turn.Tick(1, 0, 0, kFaithButtonTurn180);
    CHECK(turn.events & kFaithEventTurn180);
    CHECK(turn.s.anim == FaithAnimState::WallClimbTurn);
    const float held_z = turn.s.origin.z;
    turn.Run(Ticks(0.3f));
    CHECK_NEAR(turn.s.origin.z, held_z, 1e-4);  // hangs on the wall
    turn.Tick(0, 0, 0, kFaithButtonUp);
    CHECK(turn.events & kFaithEventJumped);
    CHECK(turn.s.anim == FaithAnimState::WallClimbJump);
    CHECK(turn.s.velocity.x < -150.0f);
    CHECK_NEAR(turn.s.velocity.z, faith::kJumpSpeed, 0.01);

    Rig expire;  // the turn hold runs out and she drops
    expire.world.Add({100, -256, 0}, {400, 256, 400});
    expire.Settle();
    expire.Run(Ticks(0.6f), 1);
    expire.Tick(1, 0, 0, kFaithButtonUp);
    expire.Run(Ticks(0.2f), 1);
    expire.Tick(0, 0, 0, kFaithButtonTurn180);
    expire.Run(Ticks(faith::kWallClimbTurnHoldSec + 0.1f));
    CHECK(expire.s.anim == FaithAnimState::Fall);
}

void TestLedges() {
    // Jump at a 100 su box from 30 su away (out of wallclimb reach): grabs the ledge near the apex.
    Rig r(true, {54, 0, 0});
    r.world.Add({100, -256, 0}, {400, 256, 100});
    r.Settle();
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(r.events & kFaithEventJumped);
    RunUntil(r, 100, [&] { return r.s.anim == FaithAnimState::LedgeHang; });
    CHECK(r.s.anim == FaithAnimState::LedgeHang);
    CHECK(r.events & kFaithEventLedgeGrab);
    CHECK(!(r.events & kFaithEventWallClimbStart));
    const float hang_z = r.s.origin.z;
    r.Run(Ticks(0.5f));
    CHECK_NEAR(r.s.origin.z, hang_z, 1e-4);
    CHECK_NEAR(r.s.speed_2d, 0.0, 1e-6);

    r.Tick(0, 0, 0, kFaithButtonUp);
    CHECK(r.s.anim == FaithAnimState::LedgeClimbUp);
    RunUntil(r, 100, [&] { return r.s.on_ground; }, 0);
    CHECK(r.s.on_ground);
    CHECK_NEAR(r.s.origin.z, 100.0, 0.05);
    CHECK(r.s.origin.x > 100.0f);
    r.Run(Ticks(1.0f), 1);  // and can run on top
    CHECK(r.s.origin.x > 200.0f);
    CHECK_NEAR(r.s.origin.z, 100.0, 0.05);

    // Reached through a wallclimb on a taller box.
    Rig climb;
    climb.world.Add({100, -256, 0}, {400, 256, 150});
    climb.Settle();
    climb.Run(Ticks(0.6f), 1);
    climb.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(climb.events & kFaithEventWallClimbStart);
    RunUntil(climb, 100, [&] { return climb.s.anim == FaithAnimState::LedgeHang; });
    CHECK(climb.s.anim == FaithAnimState::LedgeHang);
    climb.Tick(0, 0, 0, kFaithButtonUp);
    RunUntil(climb, 100, [&] { return climb.s.on_ground; }, 0);
    CHECK_NEAR(climb.s.origin.z, 150.0, 0.05);

    // A wall too tall for the climb: no ledge, she falls back down.
    Rig tall;
    tall.world.Add({100, -256, 0}, {400, 256, 400});
    tall.Settle();
    tall.Run(Ticks(0.6f), 1);
    tall.Tick(1, 0, 0, kFaithButtonUp);
    RunUntil(tall, 300, [&] { return tall.s.on_ground; });
    CHECK(!(tall.events & kFaithEventLedgeGrab));

    // Down drops from the ledge without grabbing it again at once.
    Rig drop(true, {54, 0, 0});
    drop.world.Add({100, -256, 0}, {400, 256, 100});
    drop.Settle();
    drop.Tick(1, 0, 0, kFaithButtonUp);
    RunUntil(drop, 100, [&] { return drop.s.anim == FaithAnimState::LedgeHang; });
    drop.events = 0;
    drop.Tick(0, 0, 0, kFaithButtonDown);
    RunUntil(drop, 200, [&] { return drop.s.on_ground; }, 1);  // even pushing toward the wall
    CHECK(drop.s.on_ground);
    CHECK(!(drop.events & kFaithEventLedgeGrab));
}

void TestVault() {
    Rig r;
    r.world.Add({100, -256, 0}, {116, 256, 36});  // waist-high, thin obstacle
    r.Settle();
    RunUntil(r, 200, [&] { return r.s.origin.x >= 62.0f; });
    const float speed = r.s.speed_2d;
    r.Tick(1, 0, 0, kFaithButtonUp);
    CHECK(!(r.events & kFaithEventJumped));
    CHECK(r.s.anim == FaithAnimState::Vault);
    const int ticks = RunUntil(r, 100, [&] { return r.s.anim != FaithAnimState::Vault; });
    CHECK_NEAR(ticks * kDt, faith::kVaultSec, 0.05);
    CHECK(r.s.speed_2d >= speed - 1.0f);  // keeps the run
    r.Run(Ticks(1.0f), 1);
    CHECK(r.s.origin.x > 200.0f);
    CHECK(r.s.on_ground);
    CHECK_NEAR(r.s.origin.z, 0.0, 0.05);
}

void TestSlideAndCrouch() {
    Rig r(true, {-300, 0, 0});
    r.world.Add({200, -256, 40}, {260, 256, 200});  // bar: 40 su of room under it
    r.Settle();
    r.Run(Ticks(1.0f), 1);
    r.Tick(1, 0, kFaithButtonDown, kFaithButtonDown);
    CHECK(r.events & kFaithEventSlideStart);
    CHECK(r.s.anim == FaithAnimState::Slide);
    CHECK(r.s.ducked);
    CHECK_NEAR(r.s.eye_height, source_defaults::kDuckViewHeight, 1e-6);

    const int slide_ticks = 1 + RunUntil(r, 300, [&] { return r.s.anim != FaithAnimState::Slide; }, 1, kFaithButtonDown);
    CHECK(slide_ticks * kDt > 0.8f && slide_ticks * kDt < 1.6f);  // until under kSlideAbortSpeed (98.4)
    CHECK(r.s.speed_2d < faith::kSlideAbortSpeed + 1.0f);

    RunUntil(r, 600, [&] { return r.s.origin.x > 230.0f; }, 1, kFaithButtonDown);  // crouch-walk under the bar
    CHECK(r.s.anim == FaithAnimState::CrouchWalk);
    CHECK(r.s.speed_2d <= faith::kCrouchSpeed + 0.01f);
    r.Run(5, 0);  // Down released under the bar: no room to stand
    CHECK(r.s.ducked);
    RunUntil(r, 600, [&] { return r.s.origin.x > 300.0f; }, 1, kFaithButtonDown);
    r.Run(5, 1);  // past the bar: stands up
    CHECK(!r.s.ducked);
    CHECK_NEAR(r.s.eye_height, source_defaults::kViewHeight, 1e-6);

    Rig slow;  // no slide from a walk: crouch instead
    slow.Settle();
    slow.Run(Ticks(0.5f), 1, 0, kFaithButtonWalk);
    slow.Tick(1, 0, kFaithButtonDown, kFaithButtonDown);
    CHECK(!(slow.events & kFaithEventSlideStart));
    CHECK(slow.s.ducked);
    slow.Run(Ticks(0.5f), 0, 0, kFaithButtonDown);
    CHECK(slow.s.anim == FaithAnimState::Crouch);
}

void TestCoilAndSkillRoll() {
    auto tower_fall = [](float press_height, bool hold) {
        Rig r;
        r.world.Add({-64, -64, 0}, {64, 64, 300});
        r.move->Reset(r.s, {0, 0, 300}, {0, 0, 0});
        r.Settle();
        RunUntil(r, 400, [&] { return !r.s.on_ground; });
        RunUntil(r, 400, [&] { return r.s.origin.z < press_height; });
        const float before = r.s.origin.z;
        r.Tick(1, 0, kFaithButtonDown, kFaithButtonDown);
        CHECK(r.s.anim == FaithAnimState::Coil);
        CHECK(r.s.ducked);
        CHECK(r.s.origin.z > before + 25.0f);  // feet tuck up, head stays
        float air_speed = 0.0f;
        RunUntil(r, 400, [&] {
            if (!r.s.on_ground) air_speed = r.s.speed_2d;
            return r.s.on_ground;
        }, 1, hold ? kFaithButtonDown : 0u);
        r.last_impact = air_speed;  // reuse: horizontal speed just before landing
        return r;
    };

    Rig roll = tower_fall(70.0f, true);  // Down just before landing from 300 su: Skill Roll
    CHECK(roll.events & kFaithEventSkillRoll);
    CHECK(!(roll.events & kFaithEventHardLanding));
    CHECK(roll.s.anim == FaithAnimState::SkillRoll);
    CHECK(roll.s.speed_2d >= roll.last_impact - 0.5f);  // keeps the speed instead of the hard-landing stop
    CHECK(roll.s.speed_2d > faith::kHardLandingMaxSpeed + 50.0f);
    roll.Run(Ticks(faith::kSkillRollSec), 1);
    CHECK(roll.s.anim == FaithAnimState::Sprint);
    CHECK(!roll.s.ducked);

    Rig early = tower_fall(280.0f, false);  // Down long before landing: too early, hard landing
    CHECK(!(early.events & kFaithEventSkillRoll));
    CHECK(early.events & kFaithEventHardLanding);
    CHECK(!early.s.ducked);

    Rig hop;  // a jump (47.6 su) is under kSkillRollHeight (78.7): no roll even coiling just before landing
    hop.Settle();
    hop.Tick(0, 0, 0, kFaithButtonUp);
    hop.Run(Ticks(0.45f));  // coil near the apex: the tuck must not count as height gained
    hop.Tick(0, 0, kFaithButtonDown, kFaithButtonDown);
    CHECK(hop.s.anim == FaithAnimState::Coil);
    hop.Run(Ticks(0.25f), 0, 0, kFaithButtonDown);
    hop.Tick(0, 0, kFaithButtonDown, kFaithButtonDown);  // tap again so the landing is inside the roll window
    CHECK(!hop.s.on_ground);
    RunUntil(hop, 100, [&] { return hop.s.on_ground; }, 0, kFaithButtonDown);
    CHECK(hop.s.on_ground);
    CHECK(!(hop.events & kFaithEventSkillRoll));
}

void TestTurn180() {
    Rig r;
    r.Settle();
    r.Run(Ticks(2.0f), 1);
    r.Tick(1, 0, 0, kFaithButtonTurn180);
    CHECK(r.events & kFaithEventTurn180);
    CHECK(r.s.anim == FaithAnimState::Turn180);
    CHECK(r.s.velocity.x < 0.0f);
    CHECK(r.s.speed_2d <= faith::kRunSpeed + 0.01f);
    r.Run(Ticks(faith::kTurn180Sec), 0, 0, 0, 180.0f);  // the host turned the view
    CHECK(r.s.anim != FaithAnimState::Turn180);
}

void TestParkourReplay() {
    Rig r(true, {0, -80, 0});
    r.world.Add({-512, -140, 0}, {900, -100, 300})   // wall to run on
        .Add({1100, -512, 0}, {1400, 512, 100})      // box to grab
        .Add({600, 20, 0}, {616, 400, 36});          // obstacle to vault
    r.Settle();
    auto script = [](Rig& rig, int from, int to) {
        for (int i = from; i < to; ++i) {
            uint32_t pressed = 0;
            if (i % 70 == 20) pressed |= kFaithButtonUp;
            if (i % 110 == 60) pressed |= kFaithButtonDown;
            if (i % 230 == 200) pressed |= kFaithButtonTurn180;
            const uint32_t held = (i % 110 >= 60 && i % 110 < 80) ? kFaithButtonDown : 0u;
            rig.Tick(1.0f, (i / 50) % 2 ? 0.2f : -0.2f, held, pressed, static_cast<float>(i % 400) * 0.1f);
        }
    };
    script(r, 0, 150);
    const FaithState saved = r.s;
    script(r, 150, 700);
    const FaithState first = r.s;
    const uint32_t events = r.events;
    r.s = saved;
    script(r, 150, 700);
    CHECK(std::memcmp(&r.s, &first, sizeof(FaithState)) == 0);
    CHECK(events & (kFaithEventWallRunStart | kFaithEventWallClimbStart | kFaithEventLedgeGrab | kFaithEventSlideStart));
}

// ------------------------------------------------------------------ P4: portals

// What the host does when the engine teleports the player through a portal: move the origin,
// rotate the velocity (the engine did it) and report the rotation in FaithInput::portal.
FaithStepResult TeleportTick(Rig& r, Vec3 exit_origin, const Mat3& rot, float fwd, float yaw, uint32_t held = 0) {
    r.s.origin = exit_origin;
    r.s.velocity = rot * r.s.velocity;
    FaithInput in;
    in.dt = kDt;
    in.forward_move = fwd;
    in.buttons = held;
    in.view_angles = {0, yaw, 0};
    in.portal.teleported = true;
    in.portal.rotation = rot;
    const FaithStepResult res = r.move->Step(in, r.s, r.world);
    r.events |= res.events;
    return res;
}

Mat3 YawRotation(float degrees) {
    const float a = degrees * aperture_runner::kDegToRad;
    return Mat3::FromColumns({std::cos(a), std::sin(a), 0}, {-std::sin(a), std::cos(a), 0}, {0, 0, 1});
}

void TestPortalKeepsWallRun() {
    // Wallrun along wall A (running +X, wall on the right), then a portal turns her 90 degrees left
    // (+X -> +Y) next to wall B, which is on her right again: the wallrun keeps going.
    Rig r(true, {0, -80, 0});
    r.world.Add({-512, -140, 0}, {8192, -100, 300})   // wall A, normal +Y
        .Add({1020, -512, 0}, {1060, 8192, 300});      // wall B, normal -X
    r.Settle();
    r.Run(Ticks(1.0f), 1);
    r.Tick(1, 0, 0, kFaithButtonUp);
    r.Run(Ticks(0.2f), 1);
    CHECK(r.s.anim == FaithAnimState::WallRunRight);
    const float speed = r.s.speed_2d;
    const float z = r.s.origin.z;

    const FaithStepResult res = TeleportTick(r, {1000, 0, z}, YawRotation(90.0f), 1, 90.0f);
    CHECK(res.events & kFaithEventPortalContinued);
    CHECK(r.s.anim == FaithAnimState::WallRunRight);
    CHECK_NEAR(r.s.wall_normal.x, -1.0, 1e-3);
    CHECK(r.s.velocity.y > speed - 10.0f);  // keeps going along +Y with its speed
    CHECK_NEAR(r.s.velocity.x, 0.0, 6.0);    // only the small push into the wall
    r.Run(Ticks(0.2f), 1, 0, 0, 90.0f);
    CHECK(r.s.anim == FaithAnimState::WallRunRight);
    CHECK(r.s.origin.y > 20.0f);

    // Same portal but no wall at the exit: the wallrun ends and she flies on.
    Rig open(true, {0, -80, 0});
    open.world.Add({-512, -140, 0}, {8192, -100, 300});
    open.Settle();
    open.Run(Ticks(1.0f), 1);
    open.Tick(1, 0, 0, kFaithButtonUp);
    open.Run(Ticks(0.2f), 1);
    const FaithStepResult gone = TeleportTick(open, {3000, 3000, open.s.origin.z}, YawRotation(90.0f), 1, 90.0f);
    CHECK(!(gone.events & kFaithEventPortalContinued));
    CHECK(gone.events & kFaithEventWallRunEnd);
    CHECK(open.s.anim != FaithAnimState::WallRunRight);
}

void TestPortalKeepsSlideAndResetsFall() {
    Rig r(true, {-300, 0, 0});
    r.Settle();
    r.Run(Ticks(1.0f), 1);
    r.Tick(1, 0, kFaithButtonDown, kFaithButtonDown);
    CHECK(r.s.anim == FaithAnimState::Slide);
    const FaithStepResult res =
        TeleportTick(r, {2000, 2000, r.s.origin.z}, YawRotation(180.0f), 1, 180.0f, kFaithButtonDown);
    CHECK(res.events & kFaithEventPortalContinued);
    CHECK(r.s.anim == FaithAnimState::Slide);
    CHECK(r.s.ducked);
    CHECK(r.s.velocity.x < -100.0f);  // the engine's rotated velocity is kept, not rotated again

    // Falling 500 su into a floor portal and coming out 50 su above another floor: the landing
    // counts from the exit, not from where the fall started.
    Rig fall(true, {0, 0, 500});
    fall.world.Add({2900, 2900, 0}, {3100, 3100, 10});
    RunUntil(fall, 200, [&] { return fall.s.origin.z < 60.0f; }, 0);
    TeleportTick(fall, {3000, 3000, 60.0f}, YawRotation(0.0f), 0, 0.0f);
    RunUntil(fall, 200, [&] { return fall.s.on_ground; }, 0);
    CHECK(fall.s.on_ground);
    CHECK(fall.events & kFaithEventLanded);
    CHECK(!(fall.events & kFaithEventHardLanding));
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
    TestWallRun();
    TestWallRunLeftAndWallJump();
    TestWallRunRefused();
    TestWallClimb();
    TestLedges();
    TestVault();
    TestSlideAndCrouch();
    TestCoilAndSkillRoll();
    TestTurn180();
    TestParkourReplay();
    TestPortalKeepsWallRun();
    TestPortalKeepsSlideAndResetsFall();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
