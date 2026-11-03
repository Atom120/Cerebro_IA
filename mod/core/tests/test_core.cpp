// Self-contained test runner for ApertureRunner.Core (no framework dependency).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "aperture_runner/bridge.h"
#include "aperture_runner/math.h"
#include "aperture_runner/shared_arena.h"

using namespace aperture_runner;

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
            std::printf("  FAIL %s:%d  %s = %.6f, expected %.6f\n", __FILE__, __LINE__, #a, \
                        va, vb);                                                            \
        }                                                                                   \
    } while (0)

void CheckVec(Vec3 a, Vec3 b, float eps, int line) {
    ++g_checks;
    if (std::fabs(a.x - b.x) > eps || std::fabs(a.y - b.y) > eps || std::fabs(a.z - b.z) > eps) {
        ++g_failures;
        std::printf("  FAIL line %d  (%.4f, %.4f, %.4f) != (%.4f, %.4f, %.4f)\n", line, a.x, a.y,
                    a.z, b.x, b.y, b.z);
    }
}
#define CHECK_VEC(a, b, eps) CheckVec((a), (b), (eps), __LINE__)

// Portal frame from outward normal (column 0), up (column 2) and origin.
Mat4 PortalFrame(Vec3 normal, Vec3 up, Vec3 origin) {
    const Vec3 x = Normalized(normal);
    const Vec3 z = Normalized(up);
    const Vec3 y = Cross(z, x);
    return Mat4::FromRotationTranslation(Mat3::FromColumns(x, y, z), origin);
}

void TestConversions() {
    std::printf("conversions\n");
    const WorldCalibration c{{100, 200, 50}, {1000, -500, 0}};
    const Vec3 p_uu{1254, -300, 175};
    const Vec3 p_su = Ue3ToSourcePoint(p_uu, c);
    // 254 uu forward = 100 su, 200 uu right = 78.74 su LEFT-negative, 175 uu up = 68.9 su
    CHECK_VEC(p_su, Vec3(100 + 254 * 0.393700787f, 200 - 200 * 0.393700787f, 50 + 175 * 0.393700787f), 1e-3f);
    CHECK_VEC(SourceToUe3Point(p_su, c), p_uu, 1e-2f);
    CHECK_NEAR(kScaleUe3ToSource * kScaleSourceToUe3, 1.0, 1e-6);
    CHECK_NEAR(kUuPerMeter * kScaleUe3ToSource, kSuPerMeter, 1e-4);

    // Quaternion mirror must equal S R S on matrices.
    const Quat q = Normalized(Quat{0.2f, -0.4f, 0.7f, 0.5f});
    const Mat3 expected = MirrorYRotation(QuatToMat3(q));
    const Mat3 got = QuatToMat3(MirrorYQuat(q));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) CHECK_NEAR(got.m[i][j], expected.m[i][j], 1e-5);

    // Rotating then converting == converting then rotating with the mirrored quaternion.
    const Vec3 v{3, -2, 5};
    CHECK_VEC(MirrorY(Rotate(q, v)), Rotate(MirrorYQuat(q), MirrorY(v)), 1e-4f);

    // UE3 rotator: yaw 16384 (90 deg) turns +X to +Y; pitch 16384 turns +X to +Z.
    CHECK_VEC(Rotate(Ue3RotatorToQuat(0, 16384, 0), {1, 0, 0}), Vec3(0, 1, 0), 1e-4f);
    CHECK_VEC(Rotate(Ue3RotatorToQuat(16384, 0, 0), {1, 0, 0}), Vec3(0, 0, 1), 1e-4f);
    const Quat back = Mat3ToQuat(QuatToMat3(q));
    CHECK_NEAR(std::fabs(back.x * q.x + back.y * q.y + back.z * q.z + back.w * q.w), 1.0, 1e-5);
}

void TestPortalTransform() {
    std::printf("portal transform\n");
    // Floor portal facing up at origin; wall portal at (500, 0, 64) facing +X.
    const Mat4 floor = PortalFrame({0, 0, 1}, {1, 0, 0}, {0, 0, 0});
    const Mat4 wall = PortalFrame({1, 0, 0}, {0, 0, 1}, {500, 0, 64});
    const PortalTransform pt = ComputePortalTransform(floor, wall);

    // Falling straight down into the floor portal exits flying out of the wall along +X.
    const Vec3 v_in{0, 0, -1000};
    const Vec3 v_out = pt.rotation * v_in;
    CHECK_VEC(v_out, Vec3(1000, 0, 0), 1e-3f);
    CHECK_NEAR(Length(v_out), Length(v_in), 1e-3);  // momentum magnitude conserved

    // The point just behind the entry plane maps just in front of the exit plane.
    const Vec3 p_out = TransformPoint(pt.transform, {0, 0, -5});
    CHECK_VEC(p_out, Vec3(505, 0, 64), 1e-3f);

    // Omega is a proper rotation.
    const Mat3 r = pt.rotation;
    const Mat3 rrt = r * r.Transposed();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) CHECK_NEAR(rrt.m[i][j], i == j ? 1.0 : 0.0, 1e-5);

    // Round trip in -> out -> in is identity.
    const PortalTransform back = ComputePortalTransform(wall, floor);
    CHECK_VEC(TransformPoint(back.transform, p_out), Vec3(0, 0, -5), 1e-3f);

    // Two facing wall portals: walking into one keeps the run direction mirrored correctly.
    const Mat4 a = PortalFrame({1, 0, 0}, {0, 0, 1}, {0, 0, 0});
    const Mat4 b = PortalFrame({0, 1, 0}, {0, 0, 1}, {0, 300, 0});
    const PortalTransform ab = ComputePortalTransform(a, b);
    CHECK_VEC(ab.rotation * Vec3(-1, 0, 0), Vec3(0, 1, 0), 1e-5f);
}

void TestCrossing() {
    std::printf("crossing detection\n");
    const Mat4 wall = PortalFrame({1, 0, 0}, {0, 0, 1}, {0, 0, 0});
    CHECK(DetectPortalCrossing({10, 0, 0}, {-10, 0, 0}, wall).crossed);
    CHECK_NEAR(DetectPortalCrossing({10, 0, 0}, {-10, 0, 0}, wall).fraction, 0.5, 1e-6);
    CHECK(!DetectPortalCrossing({-10, 0, 0}, {10, 0, 0}, wall).crossed);         // exiting
    CHECK(!DetectPortalCrossing({10, 31.9f, 53.9f}, {-10, 31.9f, 53.9f}, wall).crossed);  // corner, outside ellipse
    CHECK(DetectPortalCrossing({10, 31.0f, 0}, {-10, 31.0f, 0}, wall).crossed);
    CHECK(DetectPortalCrossing({10, 0, 53.0f}, {-10, 0, 53.0f}, wall).crossed);
    CHECK(!DetectPortalCrossing({10, 33.0f, 0}, {-10, 33.0f, 0}, wall).crossed);

    PortalCarve carve{};
    carve.ue3_matrix = wall.ToMatrix4();
    carve.half_width = kPortalHalfWidthSu * kScaleSourceToUe3;
    carve.half_height = kPortalHalfHeightSu * kScaleSourceToUe3;
    carve.depth = 40.0f;
    carve.active = 1;
    CHECK(IsInsidePortalCarve(carve, {0, 0, 0}));
    CHECK(IsInsidePortalCarve(carve, {-20, 50, 100}));
    CHECK(!IsInsidePortalCarve(carve, {-20, 90, 0}));
    CHECK(!IsInsidePortalCarve(carve, {-60, 0, 0}));
    carve.active = 0;
    CHECK(!IsInsidePortalCarve(carve, {0, 0, 0}));
}

void TestWallrunContinuation() {
    std::printf("wallrun continuation\n");
    // Faith runs along +X with the wall on her right. UE3 Y points right, so that wall's
    // normal is -Y in UE3 (+Y in Source). Ahead of her, portal A faces -X (Source); the exit
    // portal B faces -Y. Hand-derived: Omega maps the wall normal +Y(src) to +X(src).
    const Mat4 a = PortalFrame({-1, 0, 0}, {0, 0, 1}, {0, 0, 0});
    const Mat4 b = PortalFrame({0, -1, 0}, {0, 0, 1}, {800, 0, 0});
    const PortalTransform pt = ComputePortalTransform(a, b);
    Vec3 expected;
    const Vec3 wall_normal_uu{0, -1, 0};
    CHECK(ExpectedExitWallNormal(pt, wall_normal_uu, 20.0f, &expected));
    CHECK_VEC(expected, Vec3(1, 0, 0), 1e-5f);
    CHECK_VEC(pt.rotation * Vec3(1, 0, 0), Vec3(0, -1, 0), 1e-5f);  // run direction after exit
    CHECK(WallMatchesExpected(Normalized(expected + Vec3(0, 0.2f, 0.1f)), expected, 20.0f));
    // 25 degrees off -> rejected.
    const Vec3 off = Rotate(QuatFromAxisAngle({0, 0, 1}, 25.0f * kDegToRad), expected);
    CHECK(!WallMatchesExpected(off, expected, 20.0f));

    // Entry portal on the floor oriented so the wall normal lines up with its local up: the
    // transported normal becomes vertical at an upright exit, which is not a runnable wall.
    const Mat4 floor_entry = PortalFrame({0, 0, 1}, {0, 1, 0}, {0, 0, 0});
    const Mat4 wall_exit = PortalFrame({-1, 0, 0}, {0, 0, 1}, {800, 0, 0});
    const PortalTransform vertical = ComputePortalTransform(floor_entry, wall_exit);
    CHECK(!ExpectedExitWallNormal(vertical, wall_normal_uu, 20.0f, &expected));
    CHECK_VEC(expected, Vec3(0, 0, 1), 1e-5f);
}

void TestRollAndLanding() {
    std::printf("roll decay and skill roll\n");
    const Quat rolled = QuatFromAxisAngle({1, 0, 0}, 0.6f);
    CHECK_NEAR(CameraRollRad(rolled), 0.6, 1e-4);
    const Quat decayed = DecayCameraRoll(rolled, 0.1f, 8.5f);
    CHECK_NEAR(CameraRollRad(decayed), 0.6 * std::exp(-0.85), 1e-4);
    // Forward direction is unchanged by roll correction.
    CHECK_VEC(Rotate(decayed, {1, 0, 0}), Rotate(rolled, {1, 0, 0}), 1e-5f);
    // Looking straight down: roll undefined, camera untouched.
    const Quat down = QuatFromAxisAngle({0, 1, 0}, kPi / 2);
    CHECK_NEAR(CameraRollRad(down), 0.0, 1e-6);

    const ApertureTuning t;
    SkillRollResolver r;
    // 12 m/s impact, crouch 0.15 s before touchdown -> roll, 0.65 * 1200 uu/s added forward.
    r.OnCrouchPressed(10.00);
    LandingResult res = r.OnLanded(10.15, {300, 0, -1200}, {1, 0, 0}, t, kUuPerMeter);
    CHECK(res.outcome == LandingOutcome::SkillRoll);
    CHECK_VEC(res.velocity, Vec3(300 + 0.65f * 1200, 0, 0), 1e-2f);
    CHECK_NEAR(res.impact_speed_mps, 12.0, 1e-4);

    // Press 0.30 s early -> outside the 0.22 s window -> hard landing.
    r.OnCrouchPressed(20.00);
    res = r.OnLanded(20.30, {300, 0, -1200}, {1, 0, 0}, t, kUuPerMeter);
    CHECK(res.outcome == LandingOutcome::HardLanding);
    CHECK_NEAR(res.stagger_sec, 1.4, 1e-6);

    // Press after touchdown does not count.
    r.OnCrouchPressed(30.10);
    res = r.OnLanded(30.00, {0, 0, -1200}, {1, 0, 0}, t, kUuPerMeter);
    CHECK(res.outcome == LandingOutcome::HardLanding);

    // Below 9.5 m/s nothing happens regardless of input.
    res = r.OnLanded(40.0, {100, 0, -900}, {1, 0, 0}, t, kUuPerMeter);
    CHECK(res.outcome == LandingOutcome::Soft);
    CHECK_VEC(res.velocity, Vec3(100, 0, 0), 1e-4f);
}

void TestGels() {
    std::printf("gels\n");
    const ApertureTuning t;
    const GelModifiers orange = GetGelModifiers(SurfaceGelType::OrangePropulsion, t);
    CHECK_NEAR(orange.max_speed_mult, 2.4, 1e-6);
    CHECK_NEAR(orange.friction_mult, 0.15, 1e-6);
    CHECK_NEAR(orange.max_wallrun_sec, 3.5, 1e-6);
    CHECK_NEAR(t.sprint_max_speed_mps * orange.max_speed_mult, 18.672, 1e-3);
    const GelModifiers blue = GetGelModifiers(SurfaceGelType::BlueRepulsion, t);
    CHECK_VEC(ApplyRepulsionBounce({200, 0, -500}, {0, 0, 1}, blue.bounce_beta), Vec3(200, 0, 975), 1e-2f);
    CHECK_VEC(ApplyRepulsionBounce({200, 0, 500}, {0, 0, 1}, blue.bounce_beta), Vec3(200, 0, 500), 1e-4f);
    CHECK_NEAR(SprintSpeedMps(0.0f, t), 3.5, 1e-6);
    CHECK_NEAR(SprintSpeedMps(2.8f, t), 7.78, 1e-5);
    CHECK_NEAR(SprintSpeedMps(10.0f, t), 7.78, 1e-5);

    ApertureTuning wild;
    wild.propulsion_speed_mult = 9.0f;
    wild.skill_roll_window_sec = 0.01f;
    wild.portal_stencil_recursion = 20;
    const ApertureTuning s = wild.Sanitized();
    CHECK_NEAR(s.propulsion_speed_mult, 3.2, 1e-6);
    CHECK_NEAR(s.skill_roll_window_sec, 0.10, 1e-6);
    CHECK(s.portal_stencil_recursion == 8);
    const Vec3 lin = RunnerVisionLinear(t);
    CHECK_NEAR(lin.x, 0.7913, 1e-3);
    CHECK_NEAR(lin.z, 0.0060, 1e-3);
}

struct FakePawn final : IGuestPawnAdapter {
    Vec3 pos, vel;
    Quat cam;
    bool wall_present = true;
    Vec3 wall_normal;
    bool forced = false, cancelled = false, forced_right = false;
    float stagger = 0;
    void TeleportPawn(Vec3 p, Vec3 v, Quat c) override { pos = p; vel = v; cam = c; }
    bool TraceWall(Vec3, Vec3, float, Vec3* n) override {
        if (!wall_present) return false;
        *n = wall_normal;
        return true;
    }
    void ForceWallrun(Vec3, bool right) override { forced = true; forced_right = right; }
    void CancelWallrun() override { cancelled = true; }
    void SetCameraRotation(Quat c) override { cam = c; }
    void SetVelocity(Vec3 v) override { vel = v; }
    void BeginHardLanding(float s, float) override { stagger = s; }
};

void TestBridges() {
    std::printf("host/guest bridge\n");
    PortalPairState portals{};
    portals.calibration = WorldCalibration{{0, 0, 0}, {0, 0, 0}}.ToData();
    portals.blue.world_matrix = PortalFrame({-1, 0, 0}, {0, 0, 1}, {0, 0, 0}).ToMatrix4();
    portals.orange.world_matrix = PortalFrame({0, -1, 0}, {0, 0, 1}, {800, 0, 0}).ToMatrix4();
    portals.blue.active = portals.blue.linked = 1;
    portals.orange.active = portals.orange.linked = 1;

    FaithKinematicState faith{};
    faith.movement_state = static_cast<uint8_t>(TdMovementState::WallRunningRight);
    faith.wall_normal = Float3{0, -1, 0};  // UE3: wall on the right while running +X
    faith.camera_rotation = Quat::Identity().ToFloat4();
    // Blue faces -X; Faith approaches from x = -25 uu (in front of it) running +X.
    faith.position = Float3{-25, 0, 0};
    faith.velocity = Float3{500, 0, 0};

    HostBridge host;
    HostTickResult r = host.Tick(faith, portals, 1);
    CHECK(!r.teleport_issued);  // first tick only records position
    faith.position = Float3{15, 0, 0};  // now behind the plane
    r = host.Tick(faith, portals, 2);
    CHECK(r.teleport_issued);
    CHECK(r.teleport.sequence_id == 1);
    CHECK(r.teleport.preserve_wallrun == 1);
    CHECK_NEAR(Length(Vec3(r.teleport.new_velocity)), 500.0, 1e-2);
    bool saw_wallrun_event = false;
    for (uint32_t i = 0; i < r.event_count; ++i)
        saw_wallrun_event |= r.events[i].event_type == uint16_t(ApertureEventType::WallrunThroughPortal);
    CHECK(saw_wallrun_event);

    // Until the guest acks, nothing else is issued even if positions look like a crossing.
    r = host.Tick(faith, portals, 3);
    CHECK(r.waiting_for_ack && !r.teleport_issued);

    FakePawn pawn;
    pawn.wall_normal = Vec3(r.teleport.expected_wall_normal);
    GuestBridge guest;
    const TeleportCommand cmd = host.Tick(faith, portals, 4).teleport;  // still pending: empty
    CHECK(cmd.sequence_id == 0);
    HostBridge host2;
    faith.position = Float3{-25, 0, 0};
    host2.Tick(faith, portals, 1);
    faith.position = Float3{15, 0, 0};
    const TeleportCommand real = host2.Tick(faith, portals, 2).teleport;
    pawn.wall_normal = Vec3(real.expected_wall_normal);
    GuestTeleportResult g = guest.ApplyTeleport(real, TdMovementState::WallRunningRight, pawn);
    CHECK(g.acked_sequence == 1 && g.wallrun_continued && pawn.forced && pawn.forced_right);
    CHECK_VEC(pawn.pos, Vec3(real.new_position), 1e-4f);

    pawn = FakePawn{};
    pawn.wall_present = false;
    g = guest.ApplyTeleport(real, TdMovementState::WallRunningRight, pawn);
    CHECK(!g.wallrun_continued && pawn.cancelled);

    // Ack releases the host; the next tick re-seeds prev and does not teleport.
    faith.last_applied_teleport_seq = 1;
    faith.position = real.new_position;
    r = host2.Tick(faith, portals, 3);
    CHECK(!r.waiting_for_ack && !r.teleport_issued);

    // Landing via the guest bridge produces a director event.
    ApertureEventPacket ev{};
    bool has_ev = false;
    guest.OnCrouchPressed(1.0);
    guest.OnLanded(1.1, {5, 5, 0}, {0, 0, -1500}, Quat::Identity(), pawn, &ev, &has_ev, 9);
    CHECK(has_ev && ev.event_type == uint16_t(ApertureEventType::SkillRollSucceeded));
    CHECK_VEC(Vec3(ev.event_position), Vec3(5, 5, 0), 1e-6f);
}

void TestArena() {
    std::printf("shared arena\n");
    const std::wstring name = L"Local\\ApertureRunner_SHM_test_" + std::to_wstring(GetCurrentProcessId());
    ArenaOpenError err;
    auto host = SharedArena::Open(ArenaRole::Host, &err, name);
    CHECK(host != nullptr);
    if (!host) {
        std::printf("  open failed: %s\n", ToString(err));
        return;
    }
    auto guest = SharedArena::Open(ArenaRole::Guest, &err, name);
    CHECK(guest != nullptr);
    if (!guest) return;
    CHECK(host->header().magic == kShmMagic);
    CHECK(host->IsPeerAlive(ArenaRole::Guest, 5000));
    CHECK(!host->IsPeerAlive(ArenaRole::Director, 5000));

    FaithKinematicState s{};
    CHECK(!host->ReadFaithState(&s));  // nothing written yet
    s.tick_counter = 77;
    s.position = Float3{1, 2, 3};
    s.bone_count = 82;
    s.bones[81].position = Float3{9, 9, 9};
    guest->WriteFaithState(s);
    FaithKinematicState got{};
    CHECK(host->ReadFaithState(&got));
    CHECK(got.tick_counter == 77 && got.bone_count == 82 && got.bones[81].position.z == 9.0f);

    for (uint32_t i = 0; i < kTeleportRingCapacity; ++i) {
        TeleportCommand c{};
        c.sequence_id = i + 1;
        CHECK(host->PushTeleport(c));
    }
    TeleportCommand overflow{};
    CHECK(!host->PushTeleport(overflow));
    TeleportCommand out{};
    CHECK(guest->PopTeleport(&out) && out.sequence_id == 1);
    CHECK(host->PushTeleport(overflow));

    ApertureEventPacket e{};
    e.event_type = uint16_t(ApertureEventType::TurretDisarmed);
    std::snprintf(e.details, sizeof(e.details), "turret_07");
    CHECK(guest->PushEvent(e));
    ApertureEventPacket eo{};
    CHECK(!host->PopEvent(EventSource::Host, &eo));
    CHECK(host->PopEvent(EventSource::Guest, &eo));
    CHECK(eo.source == uint8_t(EventSource::Guest) && std::strcmp(eo.details, "turret_07") == 0);

    auto view = host->BeginBrushSnapshot();
    view.header->brush_count = 2;
    view.brushes[1].plane_count = 6;
    view.brushes[1].flags = kBrushFlagWallrunnable;
    host->CommitBrushSnapshot(view);
    BrushSnapshotHeader bh{};
    std::vector<ConvexBrush> brushes(kMaxBrushes);
    CHECK(guest->ReadBrushSnapshot(&bh, brushes.data()));
    CHECK(bh.brush_count == 2 && brushes[1].plane_count == 6);
    auto view2 = host->BeginBrushSnapshot();
    view2.header->brush_count = 5;
    host->CommitBrushSnapshot(view2);
    CHECK(guest->ReadBrushSnapshot(&bh, brushes.data()) && bh.brush_count == 5);

    auto ov = guest->BeginOverlayFrame(4, 2);
    ov.pixels[0] = 0xAB;
    guest->CommitOverlayFrame(ov);
    uint32_t w = 0;
    uint8_t first = 0;
    CHECK(host->ReadOverlayFrame([&](const OverlayFrameHeader& h, const uint8_t* px) {
        w = h.width;
        first = px[0];
    }));
    CHECK(w == 4 && first == 0xAB);
}

}  // namespace

int main() {
    std::printf("ApertureRunner.Core tests (%s)\n", sizeof(void*) == 8 ? "x64" : "x86");
    TestConversions();
    TestPortalTransform();
    TestCrossing();
    TestWallrunContinuation();
    TestRollAndLanding();
    TestGels();
    TestBridges();
    TestArena();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
