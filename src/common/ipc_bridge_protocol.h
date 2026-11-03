#ifndef ANTIGRAVITY_IPC_BRIDGE_PROTOCOL_H
#define ANTIGRAVITY_IPC_BRIDGE_PROTOCOL_H

/**
 * @file ipc_bridge_protocol.h
 * @brief Contrato Binario de Memoria Compartida (Pass-Through Mod: Portal <-> Mirror's Edge)
 *
 * Alcance restringido a: Edge_p (Sección 1: Azotea Inicial de Entrenamiento - edge_p_rooftop_01)
 *
 * Reglas de Sincronización entre Motores:
 * - Backend de Simulación: Portal (Source Engine 2007) -> Right-Handed Z-Up, 1 HU = 1.905 cm
 * - Frontend Visual: Mirror's Edge (Unreal Engine 3) -> Left-Handed Z-Up, 1 UU = 1.0 cm
 */

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PME_SHM_NAME          "Local\\Antigravity_Portal_ME_Bridge_v1"
#define PME_MAGIC_HEADER      0x504D4531u /* "PME1" */
#define PME_PROTOCOL_VERSION  1u

/* Constantes de conversión geométrica entre Source (HU) y Mirror's Edge (UU) */
#define PME_HU_TO_UU          1.905f
#define PME_UU_TO_HU          0.5249343832f
#define PME_DEG_TO_URU        182.04444444f
#define PME_URU_TO_DEG        0.0054931641f

/* Anclaje de la Sección 1 del nivel Edge_p (Azotea de entrenamiento) */
#define PME_ANCHOR_ME_X       (-14200.0f)
#define PME_ANCHOR_ME_Y       (8400.0f)
#define PME_ANCHOR_ME_Z       (3200.0f)

/* Bitmask de botones de entrada (compatibles con Source Engine IN_* flags) */
#define PME_IN_ATTACK         (1u << 0)  /* Disparo Portal Azul (Click Izquierdo) */
#define PME_IN_JUMP           (1u << 1)  /* Salto (Espacio) */
#define PME_IN_DUCK           (1u << 2)  /* Agacharse (Ctrl) */
#define PME_IN_FORWARD        (1u << 3)  /* W */
#define PME_IN_BACK           (1u << 4)  /* S */
#define PME_IN_USE            (1u << 5)  /* E (Interactuar / Recoger objeto) */
#define PME_IN_MOVELEFT       (1u << 9)  /* A */
#define PME_IN_MOVERIGHT      (1u << 10) /* D */
#define PME_IN_ATTACK2        (1u << 11) /* Disparo Portal Naranja (Click Derecho) */
#define PME_IN_RESET_SLICE    (1u << 31) /* Reiniciar jugador al spawn de la azotea (Tecla R) */

#pragma pack(push, 8)

typedef struct PMEVec3 {
    float x;
    float y;
    float z;
} PMEVec3;

typedef struct PMEQAngle {
    float pitch; /* Grados [-90, 90], positivo hacia abajo en Source */
    float yaw;   /* Grados [-180, 180], antihorario visto desde +Z en Source */
    float roll;  /* Grados [-180, 180] */
} PMEQAngle;

typedef struct PMEURotator {
    int32_t pitch; /* Unidades de Rotación Unreal (URU): 65536 = 360 grados */
    int32_t yaw;
    int32_t roll;
} PMEURotator;

typedef struct PMEPortalEntityState {
    uint8_t   is_active;       /* 1 si el portal está abierto y válido en la superficie */
    uint8_t   is_linked;       /* 1 si ambos portales (Azul y Naranja) están conectados */
    uint16_t  reserved;
    PMEVec3   origin_hu;       /* Posición del centro del portal en Source (HU) */
    PMEQAngle angles_deg;      /* Orientación del plano del portal en Source (grados) */
    PMEVec3   normal;          /* Vector normal unitario saliendo de la pared en Source */
} PMEPortalEntityState;

/**
 * Canal Frontend -> Backend (Mirror's Edge envía input del usuario hacia Portal)
 */
typedef struct PMEInputChannel {
    volatile uint32_t seq;          /* SeqLock: impar = escribiendo, par = listo */
    uint32_t          frame_number;
    float             forward_move; /* [-450.0f, 450.0f] velocidad deseada en Source */
    float             side_move;    /* [-450.0f, 450.0f] */
    float             mouse_dx;     /* Delta de yaw acumulado desde el último tick */
    float             mouse_dy;     /* Delta de pitch acumulado desde el último tick */
    uint32_t          buttons;      /* Bitmask PME_IN_* */
    uint32_t          reserved;
} PMEInputChannel;

/**
 * Canal Backend -> Frontend (Portal envía física de Chell y estado de portales a Mirror's Edge)
 */
typedef struct PMESimulationChannel {
    volatile uint32_t    seq;                  /* SeqLock: impar = escribiendo, par = listo */
    uint32_t             server_tick;
    PMEVec3              player_origin_hu;     /* Posición de los pies de Chell en Source (HU) */
    PMEVec3              player_eye_offset_hu; /* Offset de cámara (0,0,64 de pie / 0,0,28 agachada) */
    PMEVec3              player_velocity_hu;   /* Velocidad en HU/s (incluye impulso de portales) */
    PMEQAngle            player_view_angles;   /* Ángulos de cámara en grados de Source */
    uint8_t              is_on_ground;
    uint8_t              is_ducked;
    uint8_t              in_portal_env;        /* 1 si Chell está cruzando el umbral de un portal */
    uint8_t              teleported_this_tick; /* 1 en el frame exacto en que cruza un portal */
    PMEPortalEntityState blue_portal;          /* Portal 1 (Azul) */
    PMEPortalEntityState orange_portal;        /* Portal 2 (Naranja) */
} PMESimulationChannel;

/**
 * Bloque raíz de Memoria Compartida (Local\Antigravity_Portal_ME_Bridge_v1)
 */
typedef struct PMEBridgeSharedMemory {
    uint32_t             magic;            /* PME_MAGIC_HEADER (0x504D4531) */
    uint32_t             protocol_version; /* PME_PROTOCOL_VERSION (1) */
    PMEVec3              anchor_me_uu;     /* Origen del anclaje en Mirror's Edge (-14200, 8400, 3200) */
    uint32_t             flags;
    PMEInputChannel      input;            /* Escrito por Mirror's Edge, leído por Portal */
    PMESimulationChannel sim;              /* Escrito por Portal, leído por Mirror's Edge */
} PMEBridgeSharedMemory;

#pragma pack(pop)

/**
 * Convierte una posición local de Source Engine (HU, Right-Handed)
 * a coordenadas de mundo de Mirror's Edge (UU, Left-Handed) relativas al anclaje del nivel.
 */
static inline PMEVec3 PME_SourceToME_Position(PMEVec3 pos_hu, PMEVec3 anchor_me_uu) {
    PMEVec3 out_uu;
    out_uu.x = anchor_me_uu.x + (pos_hu.x * PME_HU_TO_UU);
    out_uu.y = anchor_me_uu.y - (pos_hu.y * PME_HU_TO_UU); /* Inversión de eje Y (RH -> LH) */
    out_uu.z = anchor_me_uu.z + (pos_hu.z * PME_HU_TO_UU);
    return out_uu;
}

/**
 * Convierte una posición de mundo de Mirror's Edge (UU, Left-Handed)
 * a coordenadas locales de Source Engine (HU, Right-Handed) para generar geometría proxy.
 */
static inline PMEVec3 PME_METoSource_Position(PMEVec3 pos_me_uu, PMEVec3 anchor_me_uu) {
    PMEVec3 out_hu;
    out_hu.x = (pos_me_uu.x - anchor_me_uu.x) * PME_UU_TO_HU;
    out_hu.y = -(pos_me_uu.y - anchor_me_uu.y) * PME_UU_TO_HU; /* Inversión de eje Y (LH -> RH) */
    out_hu.z = (pos_me_uu.z - anchor_me_uu.z) * PME_UU_TO_HU;
    return out_hu;
}

/**
 * Convierte el vector de velocidad de Source (HU/s, RH) al de Mirror's Edge (UU/s, LH).
 */
static inline PMEVec3 PME_SourceToME_Velocity(PMEVec3 vel_hu) {
    PMEVec3 out_uu;
    out_uu.x = vel_hu.x * PME_HU_TO_UU;
    out_uu.y = -vel_hu.y * PME_HU_TO_UU;
    out_uu.z = vel_hu.z * PME_HU_TO_UU;
    return out_uu;
}

/**
 * Convierte un vector normal unitario de Source (RH) a Mirror's Edge (LH).
 */
static inline PMEVec3 PME_SourceToME_Normal(PMEVec3 normal_rh) {
    PMEVec3 out_lh;
    out_lh.x = normal_rh.x;
    out_lh.y = -normal_rh.y;
    out_lh.z = normal_rh.z;
    return out_lh;
}

/**
 * Convierte ángulos de vista de Source (QAngle en grados) a FRotator de UE3 (URU 0..65535).
 * Nota: En Source +Pitch mira hacia abajo; en UE3 +Pitch mira hacia arriba.
 *       En Source +Yaw gira a la izquierda (CCW); en UE3 +Yaw gira a la derecha (CW).
 */
static inline PMEURotator PME_SourceToME_Rotation(PMEQAngle q) {
    PMEURotator rot;
    rot.pitch = ((int32_t)lroundf(-q.pitch * PME_DEG_TO_URU)) & 0xFFFF;
    rot.yaw   = ((int32_t)lroundf(-q.yaw   * PME_DEG_TO_URU)) & 0xFFFF;
    rot.roll  = ((int32_t)lroundf( q.roll  * PME_DEG_TO_URU)) & 0xFFFF;
    return rot;
}

#ifdef __cplusplus
}
#endif

#endif /* ANTIGRAVITY_IPC_BRIDGE_PROTOCOL_H */
