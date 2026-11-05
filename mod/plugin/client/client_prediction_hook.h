// Client-side prediction hook for client.dll!GameMovement001 and input mapping (Tasks G1 & G2).
// Owner: Gonzalo Robles (mod/plugin/client/).
#pragma once

#include <cstdint>

#include "ar/faith_movement.h"
#include "ar/trace_provider.h"
#include "client_interfaces.h"

namespace ar::client {

// Maps Source Engine button bitmasks (IN_JUMP, IN_DUCK, IN_RELOAD, IN_WALK) to ar::FaithButton.
// IN_USE stays untouched in the engine so buttons and cubes in testchmb_a_00 keep working.
constexpr uint32_t MapSourceButtonsToFaith(int32_t source_buttons) {
    uint32_t out = 0;
    if ((source_buttons & kInJump) != 0) out |= kFaithButtonUp;
    if ((source_buttons & kInDuck) != 0) out |= kFaithButtonDown;
    if ((source_buttons & kInReload) != 0) out |= kFaithButtonTurn180;
    if ((source_buttons & kInWalk) != 0) out |= kFaithButtonWalk;
    return out;
}

// Converts Source CMoveData into a deterministic ar::FaithInput for one prediction tick.
FaithInput BuildFaithInputFromMoveData(const CMoveData& move,
                                       float dt = source_defaults::kTickInterval,
                                       double curtime = 0.0);

// Synchronizes FaithState origin/velocity back into CMoveData after IFaithMovement::Step.
void WriteFaithStateToMoveData(const FaithState& state, CMoveData& move);

class ClientPredictionHook {
public:
    // Resolves client.dll in the current process via GetModuleHandleA("client.dll")
    // and hooks GameMovement001::ProcessMovement.
    static bool InstallFromLoadedClientDll();

    // Obtains GameMovement001 from a provided client.dll CreateInterface factory and hooks it.
    static bool InstallFromClientFactory(CreateInterfaceFn client_factory);

    // Installs the vtable hook on an explicit IClientGameMovement instance (used by unit tests).
    static bool Install(IClientGameMovement* movement);

    static void Uninstall();
    static bool IsHooked();

    static uint64_t GetInterceptedTicks();
    static void ResetInterceptedTicks();

    // Configures optional Faith movement backend for client prediction (Task G2).
    // When enabled is false or movement/trace is null, the hook operates in pure passthrough mode.
    static void SetFaithMovementBackend(const IFaithMovement* movement,
                                        const ITraceProvider* trace,
                                        bool enabled);

    static const FaithState& GetPredictedState();
    static void SetPredictedState(const FaithState& state);
    static const FaithStepResult& GetLastStepResult();

private:
    static void __fastcall Hooked_ProcessMovement(void* thisptr, int dummy_edx,
                                                  void* pPlayer, CMoveData* pMove);
};

}  // namespace ar::client
