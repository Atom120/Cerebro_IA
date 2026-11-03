#pragma once

#include "source_interfaces.h"
#include <cstdint>

namespace ar {

class MovementHook {
public:
    static bool Install(source::IGameMovement* pMovement);
    static void Uninstall();
    static bool IsHooked();
    static uint64_t GetInterceptedTicks();
    static void ResetInterceptedTicks();

private:
    static void __fastcall Hooked_ProcessMovement(void* thisptr, int dummy_edx, void* pPlayer, void* pMove);
};

}  // namespace ar
