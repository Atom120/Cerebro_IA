/**
 * @file me_stormdrain_hook_v2.cpp
 * @brief Flujo de Trabajo #2 (Fase 3 y 4): Frontend Visual en Mirror's Edge (`Stormdrain_p` Esclusa Vertical)
 *
 * Se inyecta en MirrorsEdge.exe durante la carga de `Stormdrain_p` (Sección Única: Gran Esclusa Vertical).
 * Extiende las capacidades del Flujo #1 (`me_d3d9_bridge.cpp`) añadiendo:
 * 1. Sincronización de posición y orientación 3D del Cubo de Compañía (`companion_cube`) en coordenadas de UE3 (UU).
 * 2. Orientación de portales sobre rampas oblicuas a 45° (`sluice_angled_fling_ramp_45`).
 * 3. Disparo del evento Kismet `OpenSluiceExitGate_01` cuando `floor_button_pressed == 1`.
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../common/ipc_stormdrain_bridge_v2.h"

static HANDLE g_hMapFileV2 = NULL;
static PME2StormdrainSharedMemory* g_pBridgeV2 = NULL;

typedef struct PME2StormdrainFrameTarget {
    /* Estado del Jugador (Chell -> Faith Pawn) */
    PMEVec3     pawn_location_uu;
    PMEVec3     camera_location_uu;
    PMEVec3     pawn_velocity_uu;
    PMEURotator controller_rotation_uru;
    /* Portales Azul y Naranja (incluyendo normales inclinadas a 45 grados) */
    uint8_t     blue_portal_active;
    PMEVec3     blue_portal_pos_uu;
    PMEVec3     blue_portal_normal_lh;
    uint8_t     orange_portal_active;
    PMEVec3     orange_portal_pos_uu;
    PMEVec3     orange_portal_normal_lh;
    /* Cubo de Compañía sincronizado */
    uint8_t     cube_present;
    uint8_t     cube_held;
    PMEVec3     cube_location_uu;
    PMEURotator cube_rotation_uru;
    /* Estado del Puzzle y Compuerta Kismet */
    uint8_t     floor_button_pressed;
    uint8_t     sluice_exit_gate_open;
} PME2StormdrainFrameTarget;

bool PME2_Frontend_ConnectStormdrainBridge() {
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
    return (g_pBridgeV2 != NULL);
#else
    return false;
#endif
}

bool PME2_Frontend_ReadAndTransformStormdrain(PME2StormdrainFrameTarget* outTarget) {
    if (!g_pBridgeV2 || !outTarget || g_pBridgeV2->magic != PME2_MAGIC_HEADER) {
        return false;
    }

    PME2StormdrainSimChannel snapshot;
    uint32_t seqStart, seqEnd;

    do {
        seqStart = g_pBridgeV2->sim.seq;
        if (seqStart & 1u) continue;
#ifdef _WIN32
        MemoryBarrier();
#endif
        snapshot = g_pBridgeV2->sim;
#ifdef _WIN32
        MemoryBarrier();
#endif
        seqEnd = g_pBridgeV2->sim.seq;
    } while (seqStart != seqEnd);

    PMEVec3 anchor = g_pBridgeV2->anchor_me_uu;

    /* 1. Transformar Chell (HU, RH) -> Faith Pawn en Stormdrain_p (UU, LH) */
    outTarget->pawn_location_uu = PME_SourceToME_Position(snapshot.player_origin_hu, anchor);
    PMEVec3 eyeHu = {
        snapshot.player_origin_hu.x + snapshot.player_eye_offset_hu.x,
        snapshot.player_origin_hu.y + snapshot.player_eye_offset_hu.y,
        snapshot.player_origin_hu.z + snapshot.player_eye_offset_hu.z
    };
    outTarget->camera_location_uu = PME_SourceToME_Position(eyeHu, anchor);
    outTarget->pawn_velocity_uu = PME_SourceToME_Velocity(snapshot.player_velocity_hu);
    outTarget->controller_rotation_uru = PME_SourceToME_Rotation(snapshot.player_view_angles);

    /* 2. Transformar Portales (incluyendo rampa oblicua a 45 grados) */
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

    /* 3. Transformar el Weighted Companion Cube al espacio de Mirror's Edge */
    outTarget->cube_present = snapshot.companion_cube.is_present;
    outTarget->cube_held = snapshot.companion_cube.is_held_by_player;
    if (snapshot.companion_cube.is_present) {
        outTarget->cube_location_uu = PME_SourceToME_Position(snapshot.companion_cube.origin_hu, anchor);
        outTarget->cube_rotation_uru = PME_SourceToME_Rotation(snapshot.companion_cube.angles_deg);
    }

    /* 4. Estado del botón de piso y compuerta Kismet de salida */
    outTarget->floor_button_pressed = snapshot.puzzle_state.floor_button_pressed;
    outTarget->sluice_exit_gate_open = snapshot.puzzle_state.sluice_exit_gate_open;

    return true;
}
