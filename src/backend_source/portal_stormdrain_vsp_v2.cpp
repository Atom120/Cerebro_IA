/**
 * @file portal_stormdrain_vsp_v2.cpp
 * @brief Flujo de Trabajo #2 (Fase 2): Backend de Simulación Extendido en Portal para `Stormdrain_p`
 *
 * Se carga dentro de hl2.exe (-game portal +map me_stormdrain_sluice_01) mediante VSP.
 * A diferencia del Flujo #1 (`portal_me_vsp.cpp`), este backend sincroniza:
 * 1. Posición, velocidad y ángulos de Chell incluyendo catapultas en paneles a 45° (`wedge_45_plus_x`).
 * 2. Estado de ambos portales (`prop_portal`) con vectores normales oblicuos 3D.
 * 3. El Cubo de Compañía (`prop_weighted_cube`), permitiendo recogerlo con +USE (`PME_IN_USE`) y lanzarlo por portales.
 * 4. El estado del Super-Botón de piso (`prop_floor_button`) y las barreras Emancipation Grill (`prop_portal_cleanser`).
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../common/ipc_stormdrain_bridge_v2.h"

static HANDLE g_hMapFileV2 = NULL;
static PME2StormdrainSharedMemory* g_pBridgeV2 = NULL;

bool PME2_Backend_InitStormdrainBridge() {
#ifdef _WIN32
    g_hMapFileV2 = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(PME2StormdrainSharedMemory),
        PME2_SHM_NAME
    );
    if (!g_hMapFileV2) return false;

    g_pBridgeV2 = (PME2StormdrainSharedMemory*)MapViewOfFile(
        g_hMapFileV2,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        sizeof(PME2StormdrainSharedMemory)
    );
    if (!g_pBridgeV2) return false;

    if (g_pBridgeV2->magic != PME2_MAGIC_HEADER) {
        ZeroMemory(g_pBridgeV2, sizeof(PME2StormdrainSharedMemory));
        g_pBridgeV2->magic = PME2_MAGIC_HEADER;
        g_pBridgeV2->protocol_version = PME2_PROTOCOL_VERSION;
        g_pBridgeV2->anchor_me_uu.x = PME2_ANCHOR_ME_X;
        g_pBridgeV2->anchor_me_uu.y = PME2_ANCHOR_ME_Y;
        g_pBridgeV2->anchor_me_uu.z = PME2_ANCHOR_ME_Z;
        g_pBridgeV2->transport_mode = 0; /* 0 = SharedMemory activa */
    }
    return true;
#else
    return false;
#endif
}

void PME2_Backend_PublishStormdrainTick(
    uint32_t tick,
    PMEVec3 playerOriginHU,
    PMEVec3 playerVelHU,
    PMEQAngle viewAnglesDeg,
    uint8_t isOnGround,
    uint8_t isDucked,
    const PMEPortalEntityState* bluePortal,
    const PMEPortalEntityState* orangePortal,
    const PME2PhysicsPropState* companionCube,
    const PME2PuzzleTriggerState* puzzleState
) {
    if (!g_pBridgeV2) return;

    /* SeqLock Write Begin (impar = escribiendo) */
    uint32_t seq = g_pBridgeV2->sim.seq;
    g_pBridgeV2->sim.seq = seq + 1;
#ifdef _WIN32
    MemoryBarrier();
#endif

    g_pBridgeV2->sim.server_tick = tick;
    g_pBridgeV2->sim.player_origin_hu = playerOriginHU;
    g_pBridgeV2->sim.player_eye_offset_hu.x = 0.0f;
    g_pBridgeV2->sim.player_eye_offset_hu.y = 0.0f;
    g_pBridgeV2->sim.player_eye_offset_hu.z = isDucked ? 28.0f : 64.0f;
    g_pBridgeV2->sim.player_velocity_hu = playerVelHU;
    g_pBridgeV2->sim.player_view_angles = viewAnglesDeg;
    g_pBridgeV2->sim.is_on_ground = isOnGround;
    g_pBridgeV2->sim.is_ducked = isDucked;

    if (bluePortal)    g_pBridgeV2->sim.blue_portal = *bluePortal;
    if (orangePortal)  g_pBridgeV2->sim.orange_portal = *orangePortal;
    if (companionCube) g_pBridgeV2->sim.companion_cube = *companionCube;
    if (puzzleState)   g_pBridgeV2->sim.puzzle_state = *puzzleState;

#ifdef _WIN32
    MemoryBarrier();
#endif
    /* SeqLock Write End (par = listo) */
    g_pBridgeV2->sim.seq = seq + 2;
}
