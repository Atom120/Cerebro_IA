/**
 * @file portal_me_vsp.cpp
 * @brief Fase 2: Backend de Simulación en Portal (Valve Server Plugin - IServerPluginCallbacks)
 *
 * Se carga dentro de hl2.exe (-game portal +map me_edge_rooftop_01) mediante portal/addons/portal_me_bridge.vdf.
 * En cada GameFrame:
 * 1. Lee el input capturado en Mirror's Edge desde PMEInputChannel (Memoria Compartida).
 * 2. Inyecta CUserCmd en CPortal_Player (movimiento + disparo de Portal Azul / Naranja).
 * 3. Lee posición, velocidad y ángulos de CPortal_Player + estado de prop_portal (Azul/Naranja).
 * 4. Publica PMESimulationChannel con bloqueo libre SeqLock hacia Mirror's Edge.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../common/ipc_bridge_protocol.h"

static HANDLE g_hMapFile = NULL;
static PMEBridgeSharedMemory* g_pBridge = NULL;

bool PME_Backend_InitSharedMemory() {
#ifdef _WIN32
    g_hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(PMEBridgeSharedMemory),
        PME_SHM_NAME
    );
    if (!g_hMapFile) return false;

    g_pBridge = (PMEBridgeSharedMemory*)MapViewOfFile(
        g_hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        sizeof(PMEBridgeSharedMemory)
    );
    if (!g_pBridge) return false;

    if (g_pBridge->magic != PME_MAGIC_HEADER) {
        ZeroMemory(g_pBridge, sizeof(PMEBridgeSharedMemory));
        g_pBridge->magic = PME_MAGIC_HEADER;
        g_pBridge->protocol_version = PME_PROTOCOL_VERSION;
        g_pBridge->anchor_me_uu.x = PME_ANCHOR_ME_X;
        g_pBridge->anchor_me_uu.y = PME_ANCHOR_ME_Y;
        g_pBridge->anchor_me_uu.z = PME_ANCHOR_ME_Z;
    }
    return true;
#else
    return false;
#endif
}

void PME_Backend_PublishSimulationTick(
    uint32_t tick,
    PMEVec3 playerOriginHU,
    PMEVec3 playerVelHU,
    PMEQAngle viewAnglesDeg,
    uint8_t isOnGround,
    uint8_t isDucked,
    const PMEPortalEntityState* bluePortal,
    const PMEPortalEntityState* orangePortal
) {
    if (!g_pBridge) return;

    /* SeqLock Write Begin (impar = escribiendo) */
    uint32_t seq = g_pBridge->sim.seq;
    g_pBridge->sim.seq = seq + 1;
#ifdef _WIN32
    MemoryBarrier();
#endif

    g_pBridge->sim.server_tick = tick;
    g_pBridge->sim.player_origin_hu = playerOriginHU;
    g_pBridge->sim.player_eye_offset_hu.x = 0.0f;
    g_pBridge->sim.player_eye_offset_hu.y = 0.0f;
    g_pBridge->sim.player_eye_offset_hu.z = isDucked ? 28.0f : 64.0f;
    g_pBridge->sim.player_velocity_hu = playerVelHU;
    g_pBridge->sim.player_view_angles = viewAnglesDeg;
    g_pBridge->sim.is_on_ground = isOnGround;
    g_pBridge->sim.is_ducked = isDucked;

    if (bluePortal)   g_pBridge->sim.blue_portal = *bluePortal;
    if (orangePortal) g_pBridge->sim.orange_portal = *orangePortal;

#ifdef _WIN32
    MemoryBarrier();
#endif
    /* SeqLock Write End (par = listo) */
    g_pBridge->sim.seq = seq + 2;
}
