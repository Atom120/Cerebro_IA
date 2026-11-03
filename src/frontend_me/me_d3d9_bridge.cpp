/**
 * @file me_d3d9_bridge.cpp
 * @brief Fase 3 y 4: Frontend Visual en Mirror's Edge (UE3 ProcessEvent + D3D9 Hook)
 *
 * Se inyecta en MirrorsEdge.exe durante la carga de Edge_p (Sección 1: Azotea Inicial).
 * En cada frame:
 * 1. Captura el input del jugador en Mirror's Edge y lo publica en PMEInputChannel.
 * 2. Lee PMESimulationChannel con lectura consistente SeqLock.
 * 3. Transforma la posición (HU -> UU con inversión de eje Y) y rotación (QAngle -> FRotator)
 *    para sobrescribir TdPlayerPawn->Location, Velocity y TdPlayerController->Rotation.
 * 4. Transforma las coordenadas de blue_portal y orange_portal para renderizarlos en D3D9 EndScene.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../common/ipc_bridge_protocol.h"

static HANDLE g_hMapFile = NULL;
static PMEBridgeSharedMemory* g_pBridge = NULL;

typedef struct PMEMirrorsEdgeFrameTarget {
    PMEVec3     pawn_location_uu;
    PMEVec3     camera_location_uu;
    PMEVec3     pawn_velocity_uu;
    PMEURotator controller_rotation_uru;
    uint8_t     blue_portal_active;
    PMEVec3     blue_portal_pos_uu;
    PMEVec3     blue_portal_normal_lh;
    uint8_t     orange_portal_active;
    PMEVec3     orange_portal_pos_uu;
    PMEVec3     orange_portal_normal_lh;
} PMEMirrorsEdgeFrameTarget;

bool PME_Frontend_ConnectSharedMemory() {
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
    return (g_pBridge != NULL);
#else
    return false;
#endif
}

bool PME_Frontend_ReadAndTransformSimulation(PMEMirrorsEdgeFrameTarget* outTarget) {
    if (!g_pBridge || !outTarget || g_pBridge->magic != PME_MAGIC_HEADER) {
        return false;
    }

    PMESimulationChannel snapshot;
    uint32_t seqStart, seqEnd;

    /* Lectura SeqLock libre de bloqueos */
    do {
        seqStart = g_pBridge->sim.seq;
        if (seqStart & 1u) continue; /* El backend está escribiendo en este instante */
#ifdef _WIN32
        MemoryBarrier();
#endif
        snapshot = g_pBridge->sim;
#ifdef _WIN32
        MemoryBarrier();
#endif
        seqEnd = g_pBridge->sim.seq;
    } while (seqStart != seqEnd);

    PMEVec3 anchor = g_pBridge->anchor_me_uu;

    /* 1. Transformar pies y cámara de Chell (HU, RH) -> Faith Pawn (UU, LH) */
    outTarget->pawn_location_uu = PME_SourceToME_Position(snapshot.player_origin_hu, anchor);

    PMEVec3 eyeHu = {
        snapshot.player_origin_hu.x + snapshot.player_eye_offset_hu.x,
        snapshot.player_origin_hu.y + snapshot.player_eye_offset_hu.y,
        snapshot.player_origin_hu.z + snapshot.player_eye_offset_hu.z
    };
    outTarget->camera_location_uu = PME_SourceToME_Position(eyeHu, anchor);
    outTarget->pawn_velocity_uu = PME_SourceToME_Velocity(snapshot.player_velocity_hu);
    outTarget->controller_rotation_uru = PME_SourceToME_Rotation(snapshot.player_view_angles);

    /* 2. Transformar Portal Azul y Portal Naranja al espacio 3D de Mirror's Edge */
    outTarget->blue_portal_active = snapshot.blue_portal.is_active;
    if (snapshot.blue_portal.is_active) {
        outTarget->blue_portal_pos_uu = PME_SourceToME_Position(snapshot.blue_portal.origin_hu, anchor);
        outTarget->blue_portal_normal_lh = PME_SourceToME_Normal(snapshot.blue_portal.normal);
    }

    outTarget->orange_portal_active = snapshot.orange_portal.is_active;
    if (snapshot.orange_portal.is_active) {
        outTarget->orange_portal_pos_uu = PME_SourceToME_Position(snapshot.orange_portal.origin_hu, anchor);
        outTarget->orange_portal_normal_lh = PME_SourceToME_Normal(snapshot.orange_portal.normal);
    }

    return true;
}
