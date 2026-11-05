// Implementation of client.dll!GameMovement001 vtable hook and input translation (Tasks G1 & G2).
// Owner: Gonzalo Robles (mod/plugin/client/).
#define _CRT_SECURE_NO_WARNINGS 1
#include "client_prediction_hook.h"

#include <algorithm>
#include <cmath>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace ar::client {

namespace {

using ProcessMovementFn = void(__thiscall*)(void* thisptr, void* pPlayer, CMoveData* pMove);

IClientGameMovement* g_client_movement = nullptr;
void** g_client_vtable = nullptr;
ProcessMovementFn g_original_process_movement = nullptr;
uint64_t g_intercepted_ticks = 0;

const IFaithMovement* g_faith_movement = nullptr;
const ITraceProvider* g_trace_provider = nullptr;
bool g_faith_enabled = false;
FaithState g_predicted_state{};
FaithStepResult g_last_step_result{};

float NormalizeMoveAxis(float move_value, float max_speed) {
    const float ref = (max_speed > 1.0f) ? max_speed : 450.0f;
    return std::clamp(move_value / ref, -1.0f, 1.0f);
}

}  // namespace

FaithInput BuildFaithInputFromMoveData(const CMoveData& move, float dt, double curtime) {
    FaithInput input{};
    input.dt = (dt > 0.0f) ? dt : source_defaults::kTickInterval;
    input.time = curtime;
    input.forward_move = NormalizeMoveAxis(move.m_flForwardMove, move.m_flMaxSpeed);
    input.side_move = NormalizeMoveAxis(move.m_flSideMove, move.m_flMaxSpeed);
    input.view_angles = move.m_vecViewAngles;

    const uint32_t cur_buttons = MapSourceButtonsToFaith(move.m_nButtons);
    const uint32_t old_buttons = MapSourceButtonsToFaith(move.m_nOldButtons);
    input.buttons = cur_buttons;
    input.buttons_pressed = cur_buttons & ~old_buttons;
    return input;
}

void WriteFaithStateToMoveData(const FaithState& state, CMoveData& move) {
    move.m_vecAbsOrigin = state.origin;
    move.m_vecVelocity = state.velocity;
}

void __fastcall ClientPredictionHook::Hooked_ProcessMovement(void* thisptr, int /*dummy_edx*/,
                                                             void* pPlayer, CMoveData* pMove) {
    ++g_intercepted_ticks;

    if (g_faith_enabled && g_faith_movement != nullptr && g_trace_provider != nullptr &&
        pMove != nullptr) {
        g_predicted_state.origin = pMove->m_vecAbsOrigin;
        g_predicted_state.velocity = pMove->m_vecVelocity;

        const double curtime =
            static_cast<double>(g_intercepted_ticks) * source_defaults::kTickInterval;
        const FaithInput input =
            BuildFaithInputFromMoveData(*pMove, source_defaults::kTickInterval, curtime);
        g_last_step_result =
            g_faith_movement->Step(input, g_predicted_state, *g_trace_provider);
        WriteFaithStateToMoveData(g_predicted_state, *pMove);
        return;
    }

    if (g_original_process_movement != nullptr) {
        g_original_process_movement(thisptr, pPlayer, pMove);
    }
}

bool ClientPredictionHook::InstallFromLoadedClientDll() {
#ifdef _WIN32
    HMODULE hClient = GetModuleHandleA("client.dll");
    if (hClient == nullptr) {
        return false;
    }
    auto factory =
        reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hClient, "CreateInterface"));
    return InstallFromClientFactory(factory);
#else
    return false;
#endif
}

bool ClientPredictionHook::InstallFromClientFactory(CreateInterfaceFn client_factory) {
    if (client_factory == nullptr) {
        return false;
    }
    int return_code = 0;
    auto* movement = static_cast<IClientGameMovement*>(
        client_factory(kClientGameMovementVersion, &return_code));
    if (movement == nullptr) {
        return false;
    }
    return Install(movement);
}

bool ClientPredictionHook::Install(IClientGameMovement* movement) {
    if (movement == nullptr || g_client_vtable != nullptr) {
        return false;
    }

#ifdef _WIN32
    void** vtable = *reinterpret_cast<void***>(movement);
    if (vtable == nullptr) {
        return false;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(&vtable[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    g_client_movement = movement;
    g_client_vtable = vtable;
    g_original_process_movement = reinterpret_cast<ProcessMovementFn>(vtable[1]);
    vtable[1] = reinterpret_cast<void*>(&Hooked_ProcessMovement);

    VirtualProtect(&vtable[1], sizeof(void*), old_protect, &old_protect);
    return true;
#else
    return false;
#endif
}

void ClientPredictionHook::Uninstall() {
#ifdef _WIN32
    if (g_client_vtable != nullptr && g_original_process_movement != nullptr) {
        DWORD old_protect = 0;
        if (VirtualProtect(&g_client_vtable[1], sizeof(void*), PAGE_EXECUTE_READWRITE,
                           &old_protect)) {
            g_client_vtable[1] = reinterpret_cast<void*>(g_original_process_movement);
            VirtualProtect(&g_client_vtable[1], sizeof(void*), old_protect, &old_protect);
        }
    }
    g_client_movement = nullptr;
    g_client_vtable = nullptr;
    g_original_process_movement = nullptr;
#endif
}

bool ClientPredictionHook::IsHooked() {
    return g_client_vtable != nullptr;
}

uint64_t ClientPredictionHook::GetInterceptedTicks() {
    return g_intercepted_ticks;
}

void ClientPredictionHook::ResetInterceptedTicks() {
    g_intercepted_ticks = 0;
}

void ClientPredictionHook::SetFaithMovementBackend(const IFaithMovement* movement,
                                                   const ITraceProvider* trace,
                                                   bool enabled) {
    g_faith_movement = movement;
    g_trace_provider = trace;
    g_faith_enabled = enabled;
}

const FaithState& ClientPredictionHook::GetPredictedState() {
    return g_predicted_state;
}

void ClientPredictionHook::SetPredictedState(const FaithState& state) {
    g_predicted_state = state;
}

const FaithStepResult& ClientPredictionHook::GetLastStepResult() {
    return g_last_step_result;
}

}  // namespace ar::client
