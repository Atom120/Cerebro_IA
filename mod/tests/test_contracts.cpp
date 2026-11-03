// Checks the shared contracts in mod/include/ar compile, keep their layout and that the test
// helpers behave. Self-contained runner, same style as core/tests/test_core.cpp.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

#include "ar/faith_movement.h"
#include "ar/testing/box_world.h"
#include "ar/trace_provider.h"
#include "ar/units.h"

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

// Minimal host-side use of the interface: proves it can be implemented and driven as the
// plugin will (copy state, step, restore, step again -> same result).
class GravityOnlyMovement final : public IFaithMovement {
public:
    void Reset(FaithState& s, Vec3 origin, Vec3 velocity) const override {
        s = FaithState{};
        s.origin = origin;
        s.velocity = velocity;
    }
    FaithStepResult Step(const FaithInput& in, FaithState& s, const ITraceProvider& trace) const override {
        FaithStepResult r;
        const Vec3 next_vel = s.velocity + Vec3{0, 0, -UuToSu(me_reference::kFaithGravityUu)} * in.dt;
        const Vec3 end = s.origin + next_vel * in.dt;
        const TraceResult t = trace.TraceHull(s.origin, end, source_defaults::kHullMins, source_defaults::kHullMaxs);
        s.origin = t.end_pos;
        if (t.Hit() && t.normal.z > 0.7f) {
            if (!s.on_ground) {
                r.events |= kFaithEventLanded;
                r.impact_speed = -next_vel.z;
            }
            s.on_ground = true;
            s.velocity = {next_vel.x, next_vel.y, 0};
        } else {
            s.on_ground = false;
            s.velocity = next_vel;
        }
        s.anim = s.on_ground ? FaithAnimState::Idle : FaithAnimState::Fall;
        return r;
    }
};

void TestUnits() {
    CHECK_NEAR(UuToSu(254.0f), 100.0f, 1e-3);
    CHECK_NEAR(UuToSu(me_reference::kSprintMaxSpeedUu), 283.46, 0.01);
    CHECK_NEAR(UuToSu(me_reference::kJumpZUu), 248.03, 0.01);
    // Faith (2 x 90 uu tall) converts to about the Source player height (72 su).
    CHECK_NEAR(UuToSu(2.0f * me_reference::kCollisionHalfHeightUu), 70.87, 0.01);
    CHECK(kPortalBuildId == 19017868u);
}

void TestAnimNames() {
    std::set<std::string> names;
    for (int i = 0; i < static_cast<int>(FaithAnimState::Count); ++i) {
        const char* n = SequenceName(static_cast<FaithAnimState>(i));
        CHECK(n != nullptr);
        if (n) names.insert(n);
    }
    CHECK(names.size() == static_cast<size_t>(FaithAnimState::Count));
    CHECK(SequenceName(FaithAnimState::Count) == nullptr);
    CHECK(std::strcmp(SequenceName(FaithAnimState::WallRunLeft), "wallrun_left") == 0);
}

// docs/contratos/asset_manifest.json must list exactly the same sequences as SequenceName().
void TestAssetManifest() {
    std::ifstream f(AR_ASSET_MANIFEST, std::ios::binary);
    CHECK(f.good());
    if (!f) return;
    const std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    const size_t begin = json.find("\"sequences\"");
    const size_t open = json.find('[', begin);
    const size_t close = json.find(']', open);
    CHECK(begin != std::string::npos && open != std::string::npos && close != std::string::npos);
    if (close == std::string::npos) return;
    const std::string list = json.substr(open, close - open);

    size_t quoted = 0;
    for (char c : list) quoted += (c == '"');
    CHECK(quoted == 2u * static_cast<size_t>(FaithAnimState::Count));
    for (int i = 0; i < static_cast<int>(FaithAnimState::Count); ++i) {
        const std::string name = std::string("\"") + SequenceName(static_cast<FaithAnimState>(i)) + "\"";
        CHECK(list.find(name) != std::string::npos);
    }
}

void TestBoxWorld() {
    testing::BoxWorld w;
    w.AddFloor(0.0f).Add({100, -64, 0}, {116, 64, 200});  // floor + wall facing -X at x = 100

    // Dropping onto the floor stops at z = 0 with an up normal.
    TraceResult t = w.TraceHull({0, 0, 50}, {0, 0, -50}, source_defaults::kHullMins, source_defaults::kHullMaxs);
    CHECK(t.Hit());
    CHECK_NEAR(t.end_pos.z, 0.0, 1e-4);
    CHECK_NEAR(t.normal.z, 1.0, 1e-6);
    CHECK(t.kind == TraceHitKind::World);

    // Resting on the floor: moving sideways is free, moving down hits at once.
    t = w.TraceHull({0, 0, 0}, {50, 0, 0}, source_defaults::kHullMins, source_defaults::kHullMaxs);
    CHECK(!t.Hit());
    t = w.TraceHull({0, 0, 0}, {0, 0, -10}, source_defaults::kHullMins, source_defaults::kHullMaxs);
    CHECK(t.Hit());
    CHECK_NEAR(t.fraction, 0.0, 1e-6);

    // Running into the wall: the hull (16 su half-width) stops at x = 84 with a -X normal.
    t = w.TraceHull({0, 0, 1}, {200, 0, 1}, source_defaults::kHullMins, source_defaults::kHullMaxs);
    CHECK(t.Hit());
    CHECK_NEAR(t.end_pos.x, 84.0, 1e-3);
    CHECK_NEAR(t.normal.x, -1.0, 1e-6);

    // A ray over the top of the wall misses it.
    t = w.TraceRay({0, 0, 250}, {200, 0, 250});
    CHECK(!t.Hit());

    // Starting inside a solid is reported.
    t = w.TraceRay({108, 0, 100}, {108, 0, 120});
    CHECK(t.start_solid && t.all_solid);

    // Physics props are tagged so faith_move can refuse to wallrun on them.
    testing::BoxWorld cube;
    cube.Add({-16, -16, 0}, {16, 16, 32}, TraceHitKind::Physics);
    t = cube.TraceRay({-100, 0, 16}, {100, 0, 16});
    CHECK(t.kind == TraceHitKind::Physics);
    CHECK_NEAR(t.end_pos.x, -16.0, 1e-3);
}

void TestPredictionReplay() {
    testing::BoxWorld w;
    w.AddFloor(0.0f);
    GravityOnlyMovement m;
    FaithState s;
    m.Reset(s, {0, 0, 100}, {0, 0, 0});

    FaithInput in;
    const FaithState saved = s;
    FaithStepResult first{};
    for (int i = 0; i < 200; ++i) {
        const FaithStepResult r = m.Step(in, s, w);
        if (r.events & kFaithEventLanded) first = r;
    }
    CHECK(s.on_ground);
    CHECK_NEAR(s.origin.z, 0.0, 1e-4);
    CHECK(first.impact_speed > 0.0f);

    // Restore and replay: bit-identical, as Source prediction requires.
    FaithState replay = saved;
    for (int i = 0; i < 200; ++i) m.Step(in, replay, w);
    CHECK(std::memcmp(&replay, &s, sizeof(FaithState)) == 0);
}

}  // namespace

int main() {
    std::printf("ApertureRunner contract tests (%s)\n", sizeof(void*) == 8 ? "x64" : "x86");
    std::printf("contracts: faith_movement v%u, trace v%u\n", kFaithMovementContractVersion, kTraceContractVersion);
    TestUnits();
    TestAnimNames();
    TestAssetManifest();
    TestBoxWorld();
    TestPredictionReplay();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
