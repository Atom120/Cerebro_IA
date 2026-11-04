// Tests for the G3 camera module. Same runner style as mod/tests/test_contracts.cpp.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "camera/faith_camera.h"

using namespace ar;
using namespace ar::camera;

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
const float kRun = UuToSu(me_reference::kSprintStartSpeedUu);
const float kSprint = UuToSu(me_reference::kSprintMaxSpeedUu);

FaithState Ground(float speed, FaithAnimState anim) {
    FaithState f;
    f.on_ground = true;
    f.speed_2d = speed;
    f.anim = anim;
    return f;
}

void TestRollFollowsWallrunLean() {
    CameraState cam;
    FaithState f;
    f.anim = FaithAnimState::WallRunLeft;
    f.view_roll = 15.0f;
    const CameraParams p;
    UpdateCamera(cam, f, 0, 0, kDt, p);
    CHECK_NEAR(cam.roll, 15.0 * (1.0 - std::exp(-p.roll_lambda * kDt)), 1e-4);  // exact exp(-lambda*dt)
    for (int i = 0; i < 200; ++i) UpdateCamera(cam, f, 0, 0, kDt, p);
    CHECK_NEAR(cam.roll, 15.0, 1e-3);
    f.view_roll = 0.0f;  // wallrun over: straightens back
    for (int i = 0; i < 200; ++i) UpdateCamera(cam, f, 0, 0, kDt, p);
    CHECK_NEAR(cam.roll, 0.0, 1e-3);
}

void TestFovWithSpeed() {
    const CameraParams p;
    CameraState run;
    CameraOutput o;
    for (int i = 0; i < 400; ++i) o = UpdateCamera(run, Ground(kRun, FaithAnimState::Run), 0, 0, kDt, p);
    CHECK_NEAR(o.fov, p.fov_base, 1e-3);  // no change up to the sprint start
    CameraState sprint;
    for (int i = 0; i < 400; ++i) o = UpdateCamera(sprint, Ground(kSprint, FaithAnimState::Sprint), 0, 0, kDt, p);
    CHECK_NEAR(o.fov, p.fov_base + p.fov_sprint_extra, 1e-2);
    CameraState half;
    for (int i = 0; i < 400; ++i) o = UpdateCamera(half, Ground((kRun + kSprint) / 2, FaithAnimState::Sprint), 0, 0, kDt, p);
    CHECK_NEAR(o.fov, p.fov_base + p.fov_sprint_extra / 2, 1e-2);
}

void TestHeadBob() {
    const CameraParams p;
    CameraState idle;
    CameraOutput o;
    float max_idle = 0.0f;
    for (int i = 0; i < 300; ++i) {
        o = UpdateCamera(idle, Ground(0, FaithAnimState::Idle), 0, 0, kDt, p);
        max_idle = std::fmax(max_idle, std::fabs(o.eye_offset.z));
    }
    CHECK_NEAR(max_idle, 0.0, 1e-6);

    auto peak = [&](float speed, FaithAnimState anim) {
        CameraState cam;
        float m = 0.0f;
        for (int i = 0; i < 600; ++i) {
            const CameraOutput r = UpdateCamera(cam, Ground(speed, anim), 0, 0, kDt, p);
            if (i > 300) m = std::fmax(m, std::fabs(r.eye_offset.z));
        }
        return m;
    };
    const float sprint_bob = peak(kSprint, FaithAnimState::Sprint);
    const float run_bob = peak(kRun, FaithAnimState::Run);
    CHECK_NEAR(sprint_bob, p.bob_vertical, 0.05);
    CHECK(run_bob < sprint_bob && run_bob > 0.3f * sprint_bob);

    // Two vertical bobs (one per foot) for each lateral sway.
    CameraState steps;
    int z_flips = 0, x_flips = 0;
    float last_z = 0.0f, last_x = 0.0f;
    for (int i = 0; i < 900; ++i) {
        const CameraOutput r = UpdateCamera(steps, Ground(kSprint, FaithAnimState::Sprint), 0, 0, kDt, p);
        if (i > 300) {
            z_flips += (r.eye_offset.z > 0) != (last_z > 0);
            x_flips += (r.eye_offset.x > 0) != (last_x > 0);
        }
        last_z = r.eye_offset.z;
        last_x = r.eye_offset.x;
    }
    CHECK(x_flips > 4);
    CHECK(std::abs(z_flips - 2 * x_flips) <= 2);

    FaithState air = Ground(kSprint, FaithAnimState::Fall);  // in the air the bob fades out
    air.on_ground = false;
    CameraState cam;
    for (int i = 0; i < 300; ++i) UpdateCamera(cam, Ground(kSprint, FaithAnimState::Sprint), 0, 0, kDt, p);
    for (int i = 0; i < 200; ++i) o = UpdateCamera(cam, air, 0, 0, kDt, p);
    CHECK(std::fabs(o.eye_offset.z) < 0.01f);
}

void TestLandingShake() {
    const CameraParams p;
    auto peak = [&](uint32_t events, float impact) {
        CameraState cam;
        float m = 0.0f;
        CameraOutput o = UpdateCamera(cam, Ground(0, FaithAnimState::LandSoft), events, impact, kDt, p);
        m = std::fabs(o.pitch_offset);
        int t = 1;
        for (; t < 100; ++t) {
            o = UpdateCamera(cam, Ground(0, FaithAnimState::Idle), 0, 0, kDt, p);
            m = std::fmax(m, std::fabs(o.pitch_offset));
            if (cam.shake_left == 0.0f) break;
        }
        CHECK(t * kDt <= p.land_shake_sec + 2 * kDt);  // dies out in time
        o = UpdateCamera(cam, Ground(0, FaithAnimState::Idle), 0, 0, kDt, p);
        CHECK_NEAR(o.pitch_offset, 0.0, 1e-6);
        return m;
    };
    const float soft = peak(kFaithEventLanded, 150.0f);
    const float hard = peak(kFaithEventLanded | kFaithEventHardLanding, 400.0f);
    CHECK(soft > 0.0f);
    CHECK(hard > 2.0f * soft);
    // At the same impact speed, a hard landing shakes 1.5 times more.
    const float same_soft = peak(kFaithEventLanded, 300.0f);
    const float same_hard = peak(kFaithEventLanded | kFaithEventHardLanding, 300.0f);
    CHECK_NEAR(same_hard / same_soft, 1.5, 0.05);
    CHECK(hard <= p.land_shake_pitch * 1.5f * 1.5f + 1e-3f);
}

void TestSkillRollSomersault() {
    CameraState cam;
    CameraOutput o = UpdateCamera(cam, Ground(200, FaithAnimState::SkillRoll), kFaithEventLanded | kFaithEventSkillRoll, 300, kDt);
    float max_pitch = o.pitch_offset;
    for (int i = 0; i < 60; ++i) {
        o = UpdateCamera(cam, Ground(200, FaithAnimState::SkillRoll), 0, 0, kDt);
        if (cam.roll_left > 0.0f) max_pitch = std::fmax(max_pitch, o.pitch_offset);
    }
    CHECK(max_pitch > 300.0f && max_pitch <= 360.0f + 10.0f);  // a full forward somersault (+ the landing shake)
    CHECK(std::fabs(o.pitch_offset) < 1e-3f);                  // and back to level
}

void TestTurn180() {
    CameraState cam;
    const CameraParams p;
    float total = 0.0f;
    int ticks = 0;
    CameraOutput o = UpdateCamera(cam, Ground(0, FaithAnimState::Turn180), kFaithEventTurn180, 0, kDt, p);
    total += o.yaw_delta;
    ++ticks;
    while (cam.turn_left > 0.0f && ticks < 100) {
        o = UpdateCamera(cam, Ground(0, FaithAnimState::Turn180), 0, 0, kDt, p);
        total += o.yaw_delta;
        ++ticks;
    }
    CHECK_NEAR(total, 180.0, 1e-3);
    CHECK_NEAR(ticks * kDt, p.turn180_sec, kDt + 1e-4);
    o = UpdateCamera(cam, Ground(0, FaithAnimState::Idle), 0, 0, kDt, p);
    CHECK_NEAR(o.yaw_delta, 0.0, 1e-6);
}

void TestDeterministic() {
    CameraState a, b;
    FaithState f = Ground(kSprint, FaithAnimState::Sprint);
    for (int i = 0; i < 100; ++i) UpdateCamera(a, f, i == 50 ? kFaithEventLanded : 0u, 300, kDt);
    const CameraState saved = a;
    for (int i = 0; i < 100; ++i) UpdateCamera(a, f, 0, 0, kDt);
    b = saved;
    for (int i = 0; i < 100; ++i) UpdateCamera(b, f, 0, 0, kDt);
    CHECK(std::memcmp(&a, &b, sizeof(CameraState)) == 0);
}

}  // namespace

int main() {
    std::printf("camera tests (%s)\n", sizeof(void*) == 8 ? "x64" : "x86");
    TestRollFollowsWallrunLean();
    TestFovWithSpeed();
    TestHeadBob();
    TestLandingShake();
    TestSkillRollSomersault();
    TestTurn180();
    TestDeterministic();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
