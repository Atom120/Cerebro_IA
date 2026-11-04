#define _CRT_SECURE_NO_WARNINGS 1
#include "client_movement_hook.h"

#include "server/server_plugin.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

typedef void(__thiscall* ProcessMovementFn)(void* thisptr, void* pPlayer, void* pMove);

static void** s_vtable = nullptr;
static ProcessMovementFn s_original = nullptr;
static uint64_t s_ticks = 0;

void __fastcall ClientMovementHook::Hooked_ProcessMovement(void* thisptr, int /*dummy_edx*/, void* pPlayer, void* pMove) {
    ++s_ticks;
    if (s_ticks % 500 == 0) {
        ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] Prediccion del cliente activa (%llu ticks predichos)\n", s_ticks);
    }
    if (s_original) s_original(thisptr, pPlayer, pMove);
}

bool ClientMovementHook::Install() {
#ifdef _WIN32
    if (s_vtable) return true;
    HMODULE client = GetModuleHandleA("client.dll");
    if (!client) return false;
    auto factory = reinterpret_cast<source::CreateInterfaceFn>(GetProcAddress(client, "CreateInterface"));
    if (!factory) return false;
    int rc = 0;
    void* movement = factory(source::kGameMovementVersion, &rc);
    if (!movement || rc != 0) return false;

    void** vtable = *reinterpret_cast<void***>(movement);
    DWORD old = 0;
    if (!VirtualProtect(&vtable[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return false;
    s_original = reinterpret_cast<ProcessMovementFn>(vtable[1]);
    vtable[1] = reinterpret_cast<void*>(&Hooked_ProcessMovement);
    VirtualProtect(&vtable[1], sizeof(void*), old, &old);
    s_vtable = vtable;
    return true;
#else
    return false;
#endif
}

void ClientMovementHook::Uninstall() {
#ifdef _WIN32
    if (!s_vtable) return;
    DWORD old = 0;
    if (VirtualProtect(&s_vtable[1], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
        s_vtable[1] = reinterpret_cast<void*>(s_original);
        VirtualProtect(&s_vtable[1], sizeof(void*), old, &old);
    }
    s_vtable = nullptr;
    s_original = nullptr;
#endif
}

bool ClientMovementHook::IsHooked() { return s_vtable != nullptr; }
uint64_t ClientMovementHook::PredictedTicks() { return s_ticks; }

}  // namespace ar
