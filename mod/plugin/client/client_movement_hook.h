#pragma once

// G1/G2 (Gonzalo's area, written by Sebas): hook of the client-side prediction copy of
// IGameMovement (client.dll "GameMovement001"). Portal is one process, so the server plugin DLL can
// reach client.dll directly. With ar_enable on, prediction runs faith_move too (G2), so the client
// predicts the same movement the server computes instead of Chell's.

#include <cstdint>

#include "source_interfaces.h"

namespace ar {

class ClientMovementHook {
public:
    static bool Install();  // finds client.dll and hooks slot 1 (ProcessMovement)
    static void Uninstall();
    static bool IsHooked();
    static uint64_t PredictedTicks();

private:
    static void __fastcall Hooked_ProcessMovement(void* thisptr, int dummy_edx, void* pPlayer, void* pMove);
};

}  // namespace ar
