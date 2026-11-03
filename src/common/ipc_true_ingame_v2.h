#ifndef ANTIGRAVITY_IPC_TRUE_INGAME_V2_H
#define ANTIGRAVITY_IPC_TRUE_INGAME_V2_H

/**
 * @file ipc_true_ingame_v2.h
 * @brief Contrato Binario Unificado para `portal-me-true-ingame-v2` (Rama `segundo-intento`)
 *
 * Implementa los 4 Módulos Técnicos de `ADR-005`:
 * - Módulo 1: Geometría 1:1 de la azotea real de `Edge_p` (`(19500.0, -3009.5, 8517.2)` / `(32208.5, -4811.3, 15180.0)`).
 * - Módulo 2: Inyección de inputs en memoria (ME -> Portal) sin cambiar de ventana + `snd_mute_losefocus 0`.
 * - Módulo 3: Lectura en tiempo real de las entidades `C_Prop_Portal` (Azul y Naranja) desde `client.dll`.
 * - Módulo 4: Hook nativo `d3d9.dll` (`IDirect3DDevice9::EndScene`) en `MirrorsEdge.exe` con ocultamiento
 *             de Faith (`DrawScale = 0`), Portales 3D con Z-Buffer + vista interior + Chell, y Viewmodel
 *             en 1ª persona de la Portal Gun + retícula oficial de Portal.
 */

#include <stdint.h>
#include <math.h>
#include "ipc_stormdrain_bridge_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMETV2_SHM_NAME                "Local\\Antigravity_Portal_ME_TrueInGame_v2"
#define PMETV2_MAGIC_HEADER            0x50545632u /* "PTV2" */
#define PMETV2_PROTOCOL_VERSION        2u

/* Dimensiones reales del portal en Source (32 x 54 HU de semiejes) convertidas a UE3 (UU) */
#define PMETV2_PORTAL_HALF_WIDTH_HU    32.0f
#define PMETV2_PORTAL_HALF_HEIGHT_HU   54.0f
#define PMETV2_PORTAL_HALF_WIDTH_UU    (32.0f * PME_HU_TO_UU)  /* 60.96 UU */
#define PMETV2_PORTAL_HALF_HEIGHT_UU   (54.0f * PME_HU_TO_UU)  /* 102.87 UU */

/* Colores ARGB oficiales para D3D9 */
#define PMETV2_COLOR_BLUE_PORTAL       0xFF00A2FFu
#define PMETV2_COLOR_BLUE_CORE         0xCC66D9FFu
#define PMETV2_COLOR_ORANGE_PORTAL     0xFFFF6A00u
#define PMETV2_COLOR_ORANGE_CORE       0xCCFFB366u
#define PMETV2_COLOR_APERTURE_WHITE    0xFFF2F5F8u
#define PMETV2_COLOR_APERTURE_DARK     0xFF22262Cu
#define PMETV2_COLOR_CHELL_JUMPSUIT    0xFFFF7315u

#ifndef PMETV2_PI
#define PMETV2_PI                      3.14159265358979323846f
#endif

#pragma pack(push, 8)

/**
 * Estado 3D completo de una entidad `C_Prop_Portal` leído de `client.dll` (Módulo 3)
 * y transformado al espacio 3D Left-Handed de Mirror's Edge para renderizado en D3D9 (Módulo 4).
 */
typedef struct PMETV2PortalRenderState {
    uint8_t   is_active;         /* m_bActivated: 1 si el portal está abierto en el mundo */
    uint8_t   is_linked;         /* 1 si m_hLinkedPortal != 0xFFFFFFFF (ambos portales conectados) */
    uint8_t   is_portal2;        /* m_bIsPortal2: 0 = Portal Azul, 1 = Portal Naranja */
    uint8_t   linkage_group_id;  /* m_iLinkageGroupID (0 para el jugador local) */
    uint32_t  entity_handle;     /* CBaseHandle en cl_entitylist */
    PMEVec3   origin_hu;         /* m_vecOrigin en Source (HU, Right-Handed) */
    PMEQAngle angles_deg;        /* m_angAngles en Source (grados Pitch, Yaw, Roll) */
    PMEVec3   origin_me_uu;      /* Centro del portal en Mirror's Edge (UU, Left-Handed) */
    PMEVec3   normal_me_lh;      /* Vector Forward (normal saliente de la superficie) en UE3 */
    PMEVec3   right_me_lh;       /* Eje semieje menor horizontal del óvalo en UE3 */
    PMEVec3   up_me_lh;          /* Eje semieje mayor vertical del óvalo en UE3 */
    float     half_width_uu;     /* Semieje menor (60.96 UU) */
    float     half_height_uu;    /* Semieje mayor (102.87 UU) */
} PMETV2PortalRenderState;

/**
 * Estado visual del Viewmodel en 1ª persona de la Portal Gun, haz de disparo y retícula (Módulo 4).
 */
typedef struct PMETV2ViewmodelState {
    uint8_t   weapon_visible;          /* 1 = dibujar Viewmodel 3D de la Portal Gun en 1ª persona */
    uint8_t   active_gun_color;        /* 0 = Núcleo Azul (#00A2FF), 1 = Núcleo Naranja (#FF6A00) */
    uint8_t   chell_visible_in_portal; /* 1 = dibujar figura 3D de Chell dentro del portal enlazado */
    uint8_t   crosshair_state;         /* Bit0 = Portal Azul abierto, Bit1 = Portal Naranja abierto */
    float     recoil_factor;           /* [0.0 .. 1.0] retroceso amortiguado tras disparar */
    float     sway_yaw;                /* Desplazamiento inercial horizontal del Viewmodel */
    float     sway_pitch;              /* Desplazamiento inercial vertical del Viewmodel */
    float     beam_alpha;              /* [0.0 .. 1.0] opacidad del rayo de energía al disparar */
    PMEVec3   beam_target_me_uu;       /* Punto 3D de impacto del disparo en Mirror's Edge (UU) */
} PMETV2ViewmodelState;

/**
 * Estado del canal de Inyección Directa de Inputs en Memoria (Módulo 2: ME -> Portal).
 */
typedef struct PMETV2MemoryInputState {
    uint8_t   me_window_focused;       /* 1 cuando MirrorsEdge.exe es la única ventana en foco */
    uint8_t   unmute_portal_bg_audio;  /* 1 = snd_mute_losefocus 0 activo en Portal */
    uint16_t  reserved;
    uint32_t  buttons_mask;            /* Bitmask PME_IN_* capturado en Mirror's Edge */
    float     forward_move;            /* [-450, +450] inyectado en Portal */
    float     side_move;               /* [-450, +450] inyectado en Portal */
    PMEQAngle injected_view_angles;    /* ViewAngles escritos en engine.dll de Portal */
} PMETV2MemoryInputState;

/**
 * Bloque maestro de Memoria Compartida a 120 Hz (`Local\Antigravity_Portal_ME_TrueInGame_v2`).
 */
typedef struct PMETV2SharedState {
    uint32_t                magic;                /* PMETV2_MAGIC_HEADER (0x50545632 "PTV2") */
    uint32_t                protocol_version;     /* PMETV2_PROTOCOL_VERSION (2) */
    volatile uint32_t       seq;                  /* SeqLock: impar = escribiendo, par = listo */
    uint32_t                tick_count;

    /* Ancla geométrica 1:1 en Edge_p */
    PMEVec3                 anchor_me_uu;

    /* Estado físico de Chell en Source (HU) */
    PMEVec3                 chell_origin_hu;
    PMEVec3                 chell_eye_hu;
    PMEVec3                 chell_velocity_hu;
    PMEQAngle               chell_angles_deg;

    /* Estado de cámara y ocultamiento de Faith en Mirror's Edge (UU / URU) */
    PMEVec3                 faith_location_uu;
    PMEVec3                 camera_location_uu;
    PMEURotator             camera_rotation_uru;
    float                   camera_fov_deg;       /* 90.0f para paridad de frustum con Source */
    float                   faith_draw_scale;     /* 0.0f para ocultar la malla de Faith en 1ª persona */

    /* Módulo 3: Entidades reales C_Prop_Portal (Azul y Naranja) */
    PMETV2PortalRenderState blue_portal;
    PMETV2PortalRenderState orange_portal;

    /* Módulo 4: Estado del Viewmodel de la Portal Gun y retícula */
    PMETV2ViewmodelState    viewmodel;

    /* Módulo 2: Estado de la inyección de inputs en memoria (ME -> Portal) */
    PMETV2MemoryInputState  input_injection;
} PMETV2SharedState;

#pragma pack(pop)

/**
 * Convierte un ángulo URU de Unreal Engine 3 (0..65535) a grados normalizados [-180, +180].
 */
static inline float PMETV2_URUToDegreesSigned(int32_t uru) {
    int32_t wrapped = uru & 0xFFFF;
    if (wrapped >= 32768) {
        wrapped -= 65536;
    }
    return (float)wrapped * PME_URU_TO_DEG;
}

/**
 * Convierte una rotación FRotator de Mirror's Edge (URU, Left-Handed) a QAngle de Source (grados, Right-Handed).
 * Se usa en el Módulo 2 para inyectar la rotación de cámara de Mirror's Edge hacia `engine.dll` de Portal.
 */
static inline PMEQAngle PME_METoSource_Rotation(PMEURotator rot_uru) {
    PMEQAngle q;
    q.pitch = -PMETV2_URUToDegreesSigned(rot_uru.pitch);
    q.yaw   = -PMETV2_URUToDegreesSigned(rot_uru.yaw);
    q.roll  =  PMETV2_URUToDegreesSigned(rot_uru.roll);
    if (q.pitch > 89.0f)  q.pitch = 89.0f;
    if (q.pitch < -89.0f) q.pitch = -89.0f;
    return q;
}

/**
 * Calcula los vectores ortonormales (`Forward/Normal`, `Right`, `Up`) en espacio Left-Handed de Mirror's Edge
 * a partir de los ángulos `m_angAngles` (Pitch, Yaw, Roll en grados) de una entidad `C_Prop_Portal` de Source.
 */
static inline void PMETV2_ComputePortalBasisME(
    PMEQAngle source_angles_deg,
    PMEVec3*  out_normal_lh,
    PMEVec3*  out_right_lh,
    PMEVec3*  out_up_lh
) {
    float pitchRad = source_angles_deg.pitch * (PMETV2_PI / 180.0f);
    float yawRad   = source_angles_deg.yaw   * (PMETV2_PI / 180.0f);
    float rollRad  = source_angles_deg.roll  * (PMETV2_PI / 180.0f);

    float sp = sinf(pitchRad), cp = cosf(pitchRad);
    float sy = sinf(yawRad),   cy = cosf(yawRad);
    float sr = sinf(rollRad),  cr = cosf(rollRad);

    /* Vectores base en Source Engine (Right-Handed Z-Up, +Pitch hacia abajo) */
    PMEVec3 fwd_rh = { cp * cy, cp * sy, -sp };
    PMEVec3 right_rh = {
        -1.0f * sr * sp * cy + -1.0f * cr * -sy,
        -1.0f * sr * sp * sy + -1.0f * cr * cy,
        -1.0f * sr * cp
    };
    PMEVec3 up_rh = {
        cr * sp * cy + -sr * -sy,
        cr * sp * sy + -sr * cy,
        cr * cp
    };

    /* Convertir de Right-Handed (Source) a Left-Handed (Mirror's Edge): invertir componente Y */
    if (out_normal_lh) {
        out_normal_lh->x = fwd_rh.x;
        out_normal_lh->y = -fwd_rh.y;
        out_normal_lh->z = fwd_rh.z;
    }
    if (out_right_lh) {
        out_right_lh->x = right_rh.x;
        out_right_lh->y = -right_rh.y;
        out_right_lh->z = right_rh.z;
    }
    if (out_up_lh) {
        out_up_lh->x = up_rh.x;
        out_up_lh->y = -up_rh.y;
        out_up_lh->z = up_rh.z;
    }
}

/**
 * Rellena un `PMETV2PortalRenderState` completo a partir de los datos leídos de `C_Prop_Portal` en `client.dll`.
 */
static inline void PMETV2_PopulatePortalRenderState(
    PMETV2PortalRenderState* out_portal,
    uint8_t                  is_active,
    uint8_t                  is_linked,
    uint8_t                  is_portal2,
    PMEVec3                  origin_hu,
    PMEQAngle                angles_deg,
    PMEVec3                  anchor_me_uu
) {
    if (!out_portal) return;
    out_portal->is_active        = is_active;
    out_portal->is_linked        = is_linked;
    out_portal->is_portal2       = is_portal2;
    out_portal->linkage_group_id = 0;
    out_portal->origin_hu        = origin_hu;
    out_portal->angles_deg       = angles_deg;
    out_portal->origin_me_uu     = PME_SourceToME_Position(origin_hu, anchor_me_uu);
    PMETV2_ComputePortalBasisME(
        angles_deg,
        &out_portal->normal_me_lh,
        &out_portal->right_me_lh,
        &out_portal->up_me_lh
    );
    out_portal->half_width_uu    = PMETV2_PORTAL_HALF_WIDTH_UU;
    out_portal->half_height_uu   = PMETV2_PORTAL_HALF_HEIGHT_UU;
}

#ifdef __cplusplus
}
#endif

#endif /* ANTIGRAVITY_IPC_TRUE_INGAME_V2_H */
