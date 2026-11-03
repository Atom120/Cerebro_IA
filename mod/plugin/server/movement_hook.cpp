#define _CRT_SECURE_NO_WARNINGS 1
#include "movement_hook.h"
#include "server_plugin.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

typedef void(__thiscall* ProcessMovementFn)(void* thisptr, void* pPlayer, void* pMove);

static source::IGameMovement* s_pGameMovement = nullptr;
static void** s_pVTable = nullptr;
static ProcessMovementFn s_pOriginalProcessMovement = nullptr;
static uint64_t s_intercepted_ticks = 0;

void __fastcall MovementHook::Hooked_ProcessMovement(void* thisptr, int /*dummy_edx*/, void* pPlayer, void* pMove) {
    ++s_intercepted_ticks;

    // Log discreto periodico cada 500 ticks (~7.5 segundos en Source a 66.67 Hz)
    if (s_intercepted_ticks % 500 == 0) {
        ApertureRunnerServerPlugin::LogMsg(
            "[ApertureRunner] ProcessMovement passthrough activo (%llu ticks interceptados)\n",
            s_intercepted_ticks
        );
    }

    // Modo passthrough puro (Z3): delega la ejecucion integra a CPortalGameMovement
    if (s_pOriginalProcessMovement) {
        s_pOriginalProcessMovement(thisptr, pPlayer, pMove);
    }
}

bool MovementHook::Install(source::IGameMovement* pMovement) {
    if (!pMovement || s_pVTable) {
        return false;
    }

#ifdef _WIN32
    void** pVTable = *reinterpret_cast<void***>(pMovement);
    if (!pVTable) {
        return false;
    }

    DWORD oldProtect = 0;
    // Ranura 1 de la vtable de IGameMovement es ProcessMovement
    if (!VirtualProtect(&pVTable[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    s_pGameMovement = pMovement;
    s_pVTable = pVTable;
    s_pOriginalProcessMovement = reinterpret_cast<ProcessMovementFn>(pVTable[1]);
    pVTable[1] = reinterpret_cast<void*>(&Hooked_ProcessMovement);

    VirtualProtect(&pVTable[1], sizeof(void*), oldProtect, &oldProtect);
    return true;
#else
    return false;
#endif
}

void MovementHook::Uninstall() {
#ifdef _WIN32
    if (s_pVTable && s_pOriginalProcessMovement) {
        DWORD oldProtect = 0;
        if (VirtualProtect(&s_pVTable[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            s_pVTable[1] = reinterpret_cast<void*>(s_pOriginalProcessMovement);
            VirtualProtect(&s_pVTable[1], sizeof(void*), oldProtect, &oldProtect);
        }
    }
    s_pGameMovement = nullptr;
    s_pVTable = nullptr;
    s_pOriginalProcessMovement = nullptr;
#endif
}

bool MovementHook::IsHooked() {
    return s_pVTable != nullptr;
}

uint64_t MovementHook::GetInterceptedTicks() {
    return s_intercepted_ticks;
}

void MovementHook::ResetInterceptedTicks() {
    s_intercepted_ticks = 0;
}

}  // namespace ar
