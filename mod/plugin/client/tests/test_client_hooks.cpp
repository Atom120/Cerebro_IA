// Autonomous unit tests for client.dll prediction hook, input mapping and 1P render selector
// (Tasks G1, G2 prep, G4 provisional).
// Owner: Gonzalo Robles (mod/plugin/client/).
#include "client_prediction_hook.h"
#include "render/faith_render.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "ar/testing/box_world.h"

using namespace ar;
using namespace ar::client;
using namespace ar::client::render;

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
        std::printf("  FAIL %s:%d  %s = %.6f, expected %.6f\n", file, line, expr, actual, expected);
    }
}

#define CHECK(cond) CheckImpl((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) CheckNearImpl(static_cast<double>(a), static_cast<double>(b), \
                                            static_cast<double>(eps), #a, __FILE__, __LINE__)

class MockClientGameMovement : public IClientGameMovement {
public:
    int original_calls = 0;

    void ProcessMovement(void* /*pPlayer*/, CMoveData* pMove) override {
        ++original_calls;
        if (pMove != nullptr) {
            pMove->m_vecAbsOrigin.x += pMove->m_vecVelocity.x * source_defaults::kTickInterval;
        }
    }
    void StartTrackPredictionErrors(void* /*pPlayer*/) override {}
    void FinishTrackPredictionErrors(void* /*pPlayer*/) override {}
    void DiffPrint(const char* /*fmt*/, ...) override {}
};

class DeterministicTestFaithMovement final : public IFaithMovement {
public:
    void Reset(FaithState& state, Vec3 origin, Vec3 velocity) const override {
        state = FaithState{};
        state.origin = origin;
        state.velocity = velocity;
    }

    FaithStepResult Step(const FaithInput& input, FaithState& state,
                         const ITraceProvider& trace) const override {
        FaithStepResult res{};
        const float accel = UuToSu(me_reference::kAccelRateUu);
        state.velocity.x += input.forward_move * accel * input.dt;
        if ((input.buttons_pressed & kFaithButtonUp) != 0u && state.on_ground) {
            state.velocity.z = UuToSu(me_reference::kJumpZUu);
            state.on_ground = false;
            res.events |= kFaithEventJumped;
        }
        const Vec3 end = state.origin + state.velocity * input.dt;
        const TraceResult tr = trace.TraceHull(
            state.origin, end, source_defaults::kHullMins, source_defaults::kHullMaxs);
        state.origin = tr.end_pos;
        state.speed_2d = std::sqrt(state.velocity.x * state.velocity.x +
                                   state.velocity.y * state.velocity.y);
        state.anim = (state.speed_2d > UuToSu(me_reference::kSprintStartSpeedUu))
                         ? FaithAnimState::Sprint
                         : FaithAnimState::Run;
        return res;
    }
};

void TestButtonMappingAndMoveDataTranslation() {
    // Verify individual semantic buttons and that IN_USE is left to the engine.
    CHECK(MapSourceButtonsToFaith(kInJump) == kFaithButtonUp);
    CHECK(MapSourceButtonsToFaith(kInDuck) == kFaithButtonDown);
    CHECK(MapSourceButtonsToFaith(kInReload) == kFaithButtonTurn180);
    CHECK(MapSourceButtonsToFaith(kInWalk) == kFaithButtonWalk);
    CHECK(MapSourceButtonsToFaith(kInUse) == 0u);

    CMoveData move{};
    move.m_flMaxSpeed = 450.0f;
    move.m_flForwardMove = 450.0f;
    move.m_flSideMove = -225.0f;
    move.m_vecViewAngles = {10.0f, 90.0f, 0.0f};
    move.m_nOldButtons = kInDuck;
    move.m_nButtons = kInDuck | kInJump | kInUse;

    const FaithInput in = BuildFaithInputFromMoveData(move, 0.015f, 1.5);
    CHECK_NEAR(in.forward_move, 1.0f, 1e-5f);
    CHECK_NEAR(in.side_move, -0.5f, 1e-5f);
    CHECK_NEAR(in.view_angles.y, 90.0f, 1e-5f);
    CHECK(in.buttons == (kFaithButtonDown | kFaithButtonUp));
    // kInDuck was already held in m_nOldButtons; only kInJump was newly pressed this tick.
    CHECK(in.buttons_pressed == kFaithButtonUp);
}

void TestClientVTableHookPassthroughAndPredictionRewind() {
    MockClientGameMovement mock{};
    IClientGameMovement* volatile vptr = &mock;

    ClientPredictionHook::ResetInterceptedTicks();
    ClientPredictionHook::SetFaithMovementBackend(nullptr, nullptr, false);

    CHECK(ClientPredictionHook::Install(vptr));
    CHECK(ClientPredictionHook::IsHooked());

    CMoveData move{};
    move.m_vecAbsOrigin = {0.0f, 0.0f, 64.0f};
    move.m_vecVelocity = {200.0f, 0.0f, 0.0f};

    // Call through volatile interface pointer so MSVC emits an indirect vtable call (slot 1).
    for (int i = 0; i < 5; ++i) {
        vptr->ProcessMovement(nullptr, &move);
    }
    CHECK(ClientPredictionHook::GetInterceptedTicks() == 5u);
    CHECK(mock.original_calls == 5);
    CHECK_NEAR(move.m_vecAbsOrigin.x, 5.0f * 200.0f * source_defaults::kTickInterval, 1e-4f);

    // Switch to FaithMovement prediction mode and verify rewind/replay bit-identically.
    testing::BoxWorld world{};
    world.AddFloor(0.0f);
    DeterministicTestFaithMovement faith_move{};
    FaithState initial_state{};
    faith_move.Reset(initial_state, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
    initial_state.on_ground = true;

    ClientPredictionHook::SetPredictedState(initial_state);
    ClientPredictionHook::SetFaithMovementBackend(&faith_move, &world, true);

    move.m_vecAbsOrigin = {0.0f, 0.0f, 0.0f};
    move.m_vecVelocity = {0.0f, 0.0f, 0.0f};
    move.m_flMaxSpeed = 450.0f;
    move.m_flForwardMove = 450.0f;

    for (int i = 0; i < 10; ++i) {
        vptr->ProcessMovement(nullptr, &move);
    }
    const FaithState snapshot = ClientPredictionHook::GetPredictedState();
    const CMoveData move_snapshot = move;

    for (int i = 0; i < 8; ++i) {
        vptr->ProcessMovement(nullptr, &move);
    }
    const FaithState first_end = ClientPredictionHook::GetPredictedState();
    const Vec3 first_origin = move.m_vecAbsOrigin;

    // Rewind to tick 10 and replay the 8 ticks.
    ClientPredictionHook::SetPredictedState(snapshot);
    move = move_snapshot;
    for (int i = 0; i < 8; ++i) {
        vptr->ProcessMovement(nullptr, &move);
    }
    const FaithState replay_end = ClientPredictionHook::GetPredictedState();

    CHECK(std::memcmp(&first_end, &replay_end, sizeof(FaithState)) == 0);
    CHECK_NEAR(move.m_vecAbsOrigin.x, first_origin.x, 1e-6f);
    // Original movement was not called while Faith backend was active.
    CHECK(mock.original_calls == 5);

    ClientPredictionHook::SetFaithMovementBackend(nullptr, nullptr, false);
    ClientPredictionHook::Uninstall();
    CHECK(!ClientPredictionHook::IsHooked());
}

int g_warning_count = 0;
void CaptureWarning(const char* msg) {
    if (msg != nullptr && std::strlen(msg) > 0) {
        ++g_warning_count;
    }
}

void TestProvisionalRenderAndSequenceMapping() {
    FaithFirstPersonRenderer renderer{};
    g_warning_count = 0;

    // Non-existent path -> selects provisional Portal models and logs warning exactly once.
    const RenderModelSelection sel1 =
        renderer.ResolveModels("C:/non_existent_portal_dir_for_test", &CaptureWarning);
    CHECK(sel1.using_provisional_models);
    CHECK(std::strcmp(sel1.arms_model_path, kProvisionalArmsModelPath) == 0);
    CHECK(std::strcmp(sel1.body_model_path, kProvisionalBodyModelPath) == 0);
    CHECK(g_warning_count == 1);

    // Calling ResolveModels again does not spam the console warning.
    renderer.ResolveModels("C:/non_existent_portal_dir_for_test", &CaptureWarning);
    CHECK(g_warning_count == 1);

    // Verify all 24 FaithAnimState values produce valid sequence names matching SequenceName().
    for (int i = 0; i < static_cast<int>(FaithAnimState::Count); ++i) {
        FaithState st{};
        st.anim = static_cast<FaithAnimState>(i);
        st.speed_2d = 250.0f;
        const FaithRenderFrame frame = renderer.BuildFrame(st, 45.0f);
        CHECK(frame.sequence_name != nullptr);
        CHECK(std::strcmp(frame.sequence_name, SequenceName(st.anim)) == 0);
        CHECK(frame.draw_arms_1p);
        CHECK(frame.draw_body_1p);
    }
}

}  // namespace

int main() {
    TestButtonMappingAndMoveDataTranslation();
    TestClientVTableHookPassthroughAndPredictionRewind();
    TestProvisionalRenderAndSequenceMapping();

    std::printf("test_client_hooks: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
