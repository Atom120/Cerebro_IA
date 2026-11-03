#ifndef ANTIGRAVITY_IPC_STORMDRAIN_BRIDGE_V2_H
#define ANTIGRAVITY_IPC_STORMDRAIN_BRIDGE_V2_H

/**
 * @file ipc_stormdrain_bridge_v2.h
 * @brief Contrato Binario de Transporte Dual (SharedMemory + UDP Loopback) para el Flujo de Trabajo #2
 *        (Portal <-> Mirror's Edge en Cap. 2: Stormdrain_p — Gran Esclusa Vertical `stormdrain_p_sluice_01`)
 *
 * Este contrato coexiste con `ipc_bridge_protocol.h` (Flujo #1: Edge_p Rooftop) sin modificarlo.
 * Añade soporte para:
 * 1. Superficies y portales oblicuos a 45 grados (Angled Flings / Catapultas parabólicas).
 * 2. Sincronización de objetos físicos interactivos (Weighted Companion Cube `prop_weighted_cube`).
 * 3. Sincronización de eventos de puzzle (Botón de piso `prop_floor_button` -> Kismet `OpenSluiceExitGate_01`
 *    y barreras Emancipation Grill / Fizzlers `prop_portal_cleanser`).
 * 4. Transporte híbrido: Memoria Compartida (`Local\Antigravity_Portal_ME_Stormdrain_v2`) o Socket UDP (`127.0.0.1:27099`).
 */

#include <stdint.h>
#include <math.h>
#include "ipc_bridge_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PME2_SHM_NAME              "Local\\Antigravity_Portal_ME_Stormdrain_v2"
#define PME2_UDP_LOOPBACK_PORT     27099u
#define PME2_MAGIC_HEADER          0x504D4532u /* "PME2" */
#define PME2_PROTOCOL_VERSION      2u

/* Anclaje de la Sección Única de Stormdrain_p (Gran Esclusa Vertical) en Mirror's Edge (UU) */
#define PME2_ANCHOR_ME_X           (-4500.0f)
#define PME2_ANCHOR_ME_Y           (12800.0f)
#define PME2_ANCHOR_ME_Z           (-1600.0f)

#pragma pack(push, 8)

/**
 * Estado sincronizado de un objeto físico interactivo (ej. Weighted Companion Cube)
 */
typedef struct PME2PhysicsPropState {
    uint8_t     is_present;       /* 1 si el cubo existe en la escena y no ha sido desintegrado */
    uint8_t     is_held_by_player;/* 1 si Chell lo lleva sujetado con +USE (E) */
    uint8_t     just_fizzled;     /* 1 si acaba de tocar el Emancipation Grill (Fizzler) */
    uint8_t     reserved;
    PMEVec3     origin_hu;        /* Posición del centro del cubo en Source (HU) */
    PMEVec3     velocity_hu;      /* Velocidad lineal en Source (HU/s) */
    PMEQAngle   angles_deg;       /* Rotación 3D del cubo en grados de Source */
} PME2PhysicsPropState;

/**
 * Estado de los elementos de lógica de cámara de pruebas (Botón pesado, Compuerta Kismet, Fizzlers)
 */
typedef struct PME2PuzzleTriggerState {
    uint8_t     floor_button_pressed;  /* 1 cuando el Cubo o Chell están sobre prop_floor_button */
    uint8_t     sluice_exit_gate_open; /* 1 cuando Mirror's Edge debe disparar Kismet OpenSluiceExitGate_01 */
    uint8_t     fizzler_triggered_tick;/* 1 cuando el jugador cruza un Emancipation Grill (borra portales) */
    uint8_t     reserved;
    PMEVec3     button_origin_hu;      /* Posición del botón en Source (HU) */
} PME2PuzzleTriggerState;

/**
 * Canal Backend -> Frontend v2 (Incluye Chell, Portales Oblicuos, Cubo de Compañía y Triggers)
 */
typedef struct PME2StormdrainSimChannel {
    volatile uint32_t      seq;                  /* SeqLock: impar = escribiendo, par = listo */
    uint32_t               server_tick;
    PMEVec3                player_origin_hu;
    PMEVec3                player_eye_offset_hu;
    PMEVec3                player_velocity_hu;
    PMEQAngle              player_view_angles;
    uint8_t                is_on_ground;
    uint8_t                is_ducked;
    uint8_t                in_portal_env;
    uint8_t                teleported_this_tick;
    PMEPortalEntityState   blue_portal;          /* Soporta normales oblicuas ej. (0.7071, 0, 0.7071) */
    PMEPortalEntityState   orange_portal;
    PME2PhysicsPropState   companion_cube;       /* Estado del Weighted Companion Cube */
    PME2PuzzleTriggerState puzzle_state;         /* Estado del botón pesado y barreras Fizzler */
} PME2StormdrainSimChannel;

/**
 * Bloque raíz de Memoria Compartida / Paquete UDP para el Flujo #2 (Stormdrain_p)
 */
typedef struct PME2StormdrainSharedMemory {
    uint32_t                 magic;            /* PME2_MAGIC_HEADER (0x504D4532) */
    uint32_t                 protocol_version; /* PME2_PROTOCOL_VERSION (2) */
    PMEVec3                  anchor_me_uu;     /* (-4500, 12800, -1600) */
    uint32_t                 transport_mode;   /* 0 = SharedMemory, 1 = UDP Loopback */
    PMEInputChannel          input;            /* Reutiliza PMEInputChannel (WASD + Mouse + E + Clicks) */
    PME2StormdrainSimChannel sim;              /* Simulación extendida con cubo y rampas a 45° */
} PME2StormdrainSharedMemory;

#pragma pack(pop)

/**
 * Calcula la magnitud de un vector 3D.
 */
static inline float PME2_Vec3Length(PMEVec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/**
 * Calcula analíticamente el vector de velocidad de salida al cruzar desde un portal de entrada
 * (con normal `in_normal`) hacia un portal de salida (con normal `out_normal`, ej. rampa a 45 grados),
 * conservando íntegramente la rapidez escalar ("Speedy thing goes in, speedy thing comes out").
 *
 * La componente de velocidad que entra contra `-in_normal` se reorienta a lo largo de `+out_normal`.
 */
static inline PMEVec3 PME2_ComputePortalExitVelocity(
    PMEVec3 in_velocity_hu,
    PMEVec3 in_normal_rh,
    PMEVec3 out_normal_rh
) {
    /* Rapidez de entrada proyectada hacia el interior del portal (-in_normal) */
    float entry_speed = -(
        in_velocity_hu.x * in_normal_rh.x +
        in_velocity_hu.y * in_normal_rh.y +
        in_velocity_hu.z * in_normal_rh.z
    );
    if (entry_speed < 0.0f) {
        entry_speed = PME2_Vec3Length(in_velocity_hu);
    }

    PMEVec3 out_vel;
    out_vel.x = out_normal_rh.x * entry_speed;
    out_vel.y = out_normal_rh.y * entry_speed;
    out_vel.z = out_normal_rh.z * entry_speed;
    return out_vel;
}

#ifdef __cplusplus
}
#endif

#endif /* ANTIGRAVITY_IPC_STORMDRAIN_BRIDGE_V2_H */
