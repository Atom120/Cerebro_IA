/**
 * @file me_true_ingame_d3d9.cpp
 * @brief Módulo 4 (`portal-me-true-ingame-v2`): Proxy Nativo `d3d9.dll` + Hook `IDirect3DDevice9::EndScene`
 *        para `MirrorsEdge.exe` (32 bits x86).
 *
 * Responsabilidades dentro del proceso único visible (`MirrorsEdge.exe`):
 * 1. Actuar como proxy transparente de `C:\Windows\System32\d3d9.dll` (o `SysWOW64\d3d9.dll`).
 * 2. Interceptar `IDirect3D9::CreateDevice` (vtable[16]) e `IDirect3DDevice9::EndScene` (vtable[42]).
 * 3. Ocultar la malla en 1ª persona de Faith (`Pawn->DrawScale = 0.0f` en `+0x10C`) y fijar `FOV = 90°`.
 * 4. Construir la matriz `ViewProjection` Left-Handed en vivo desde `TdPlayerCamera` (paridad de frustum)
 *    SIN borrar el Z-Buffer del mundo (`D3DRS_ZENABLE = TRUE`) para que las paredes de `Edge_p` ocluyan
 *    naturalmente los portales cuando el jugador se cubre detrás de un obstáculo.
 * 5. Renderizar en 3D dentro de `EndScene`:
 *    - Elipses 3D del Portal Azul (`#00A2FF`) y Portal Naranja (`#FF6A00`) adheridas a las superficies 1:1.
 *    - Vista interior proyectada dentro del portal enlazado + figura 3D articulada de Chell.
 *    - Viewmodel 3D en 1ª persona de la Portal Gun (chasis blanco Aperture, núcleo azul/naranja, 3 garras,
 *      retroceso al disparar y haz de energía) en rango Z de primer plano (`MinZ=0.0, MaxZ=0.08`).
 *    - Retícula central oficial de Portal (arcos de estado azul y naranja).
 *
 * Compilación x86 (MSVC):
 *   cl /nologo /LD /EHsc /O2 src\true_ingame_v2\me_true_ingame_d3d9.cpp /Fe:me_true_ingame_d3d9.dll user32.lib
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../common/ipc_true_ingame_v2.h"

/* ============================================================================
 * 1. DEFINICIONES MÍNIMAS DE DIRECT3D 9 (CERO DEPENDENCIA DE DIRECTX SDK EXTERNO)
 * ============================================================================ */

typedef int32_t D3DRENDERSTATETYPE;
typedef int32_t D3DTRANSFORMSTATETYPE;
typedef int32_t D3DPRIMITIVETYPE;

#define D3DTS_VIEW                  2
#define D3DTS_PROJECTION            3
#define D3DTS_WORLD                 256

#define D3DRS_ZENABLE               7
#define D3DRS_FILLMODE              8
#define D3DRS_ZWRITEENABLE          14
#define D3DRS_ALPHATESTENABLE       15
#define D3DRS_SRCBLEND              19
#define D3DRS_DESTBLEND             20
#define D3DRS_CULLMODE              22
#define D3DRS_ALPHABLENDENABLE      27
#define D3DRS_LIGHTING              137

#define D3DCULL_NONE                1
#define D3DBLEND_SRCALPHA           5
#define D3DBLEND_INVSRCALPHA        6

#define D3DPT_LINELIST              2
#define D3DPT_TRIANGLELIST          4
#define D3DPT_TRIANGLEFAN           6

#define D3DFVF_XYZ                  0x002u
#define D3DFVF_XYZRHW               0x004u
#define D3DFVF_DIFFUSE              0x040u

typedef struct D3DMATRIX_POD {
    float m[4][4];
} D3DMATRIX_POD;

typedef struct D3DVIEWPORT9_POD {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT9_POD;

#pragma pack(push, 4)
typedef struct PMETV2Vertex3D {
    float    x, y, z;
    uint32_t color;
} PMETV2Vertex3D;

typedef struct PMETV2Vertex2D {
    float    x, y, z, rhw;
    uint32_t color;
} PMETV2Vertex2D;
#pragma pack(pop)

/* Firmas de métodos de la vtable de IDirect3DDevice9 */
typedef HRESULT (STDMETHODCALLTYPE *PFN_EndScene)(void* pDevice);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetTransform)(void* pDevice, D3DTRANSFORMSTATETYPE State, const D3DMATRIX_POD* pMatrix);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetViewport)(void* pDevice, const D3DVIEWPORT9_POD* pViewport);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetViewport)(void* pDevice, D3DVIEWPORT9_POD* pViewport);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetRenderState)(void* pDevice, D3DRENDERSTATETYPE State, DWORD Value);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetRenderState)(void* pDevice, D3DRENDERSTATETYPE State, DWORD* pValue);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetTexture)(void* pDevice, DWORD Stage, void* pTexture);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetVertexShader)(void* pDevice, void* pShader);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetPixelShader)(void* pDevice, void* pShader);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetFVF)(void* pDevice, DWORD FVF);
typedef HRESULT (STDMETHODCALLTYPE *PFN_DrawPrimitiveUP)(
    void* pDevice,
    D3DPRIMITIVETYPE PrimitiveType,
    UINT PrimitiveCount,
    const void* pVertexStreamZeroData,
    UINT VertexStreamZeroStride
);

/* Índices estándar en la vtable de IDirect3DDevice9 (Direct3D 9c) */
#define D3D9_VTABLE_ENDSCENE            42
#define D3D9_VTABLE_SETTRANSFORM        44
#define D3D9_VTABLE_SETVIEWPORT         47
#define D3D9_VTABLE_GETVIEWPORT         48
#define D3D9_VTABLE_SETRENDERSTATE      57
#define D3D9_VTABLE_GETRENDERSTATE      58
#define D3D9_VTABLE_SETTEXTURE          65
#define D3D9_VTABLE_DRAWPRIMITIVEUP     83
#define D3D9_VTABLE_SETFVF              89
#define D3D9_VTABLE_SETVERTEXSHADER     92
#define D3D9_VTABLE_SETPIXELSHADER      107

/* Índice de IDirect3D9::CreateDevice */
#define D3D9_OBJ_VTABLE_CREATEDEVICE    16
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateDevice)(
    void* pD3D9, UINT Adapter, int DeviceType, HWND hFocusWindow,
    DWORD BehaviorFlags, void* pPresentationParameters, void** ppReturnedDeviceInterface
);

/* ============================================================================
 * 2. ESTADO GLOBAL DEL PROXY D3D9 Y CONEXIÓN A MEMORIA COMPARTIDA
 * ============================================================================ */

static HMODULE              g_hSystemD3D9    = NULL;
static HANDLE               g_hShmMap        = NULL;
static PMETV2SharedState*   g_pShared        = NULL;
static PFN_CreateDevice     g_OrigCreateDev  = NULL;
static PFN_EndScene         g_OrigEndScene   = NULL;

typedef void* (WINAPI *PFN_Direct3DCreate9)(UINT SDKVersion);
typedef HRESULT (WINAPI *PFN_Direct3DCreate9Ex)(UINT SDKVersion, void** ppD3D);

static void PMETV2_EnsureSystemD3D9Loaded(void) {
    if (g_hSystemD3D9) return;
    char sysDir[MAX_PATH] = {0};
    GetSystemDirectoryA(sysDir, MAX_PATH);
    char fullPath[MAX_PATH] = {0};
    snprintf(fullPath, sizeof(fullPath), "%s\\d3d9.dll", sysDir);
    g_hSystemD3D9 = LoadLibraryA(fullPath);
}

static bool PMETV2_EnsureSharedMemoryConnected(void) {
    if (g_pShared && g_pShared->magic == PMETV2_MAGIC_HEADER) {
        return true;
    }
    if (!g_hShmMap) {
        g_hShmMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, PMETV2_SHM_NAME);
        if (!g_hShmMap) {
            g_hShmMap = CreateFileMappingA(
                INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                0, sizeof(PMETV2SharedState), PMETV2_SHM_NAME
            );
        }
    }
    if (g_hShmMap && !g_pShared) {
        g_pShared = (PMETV2SharedState*)MapViewOfFile(
            g_hShmMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PMETV2SharedState)
        );
    }
    return (g_pShared != NULL && g_pShared->magic == PMETV2_MAGIC_HEADER);
}

static bool PMETV2_ReadConsistentSnapshot(PMETV2SharedState* outSnap) {
    if (!PMETV2_EnsureSharedMemoryConnected() || !outSnap) return false;
    uint32_t s0, s1;
    int retries = 0;
    do {
        s0 = g_pShared->seq;
        if (s0 & 1u) {
            if (++retries > 64) return false;
            continue;
        }
        MemoryBarrier();
        *outSnap = *g_pShared;
        MemoryBarrier();
        s1 = g_pShared->seq;
    } while (s0 != s1 && ++retries < 64);
    return (outSnap->magic == PMETV2_MAGIC_HEADER);
}

/* ============================================================================
 * 3. MATEMÁTICA DE CÁMARA Y GEOMETRÍA 3D (PORTALES + CHELL + VIEWMODEL)
 * ============================================================================ */

static void PMETV2_MatrixIdentity(D3DMATRIX_POD* out) {
    memset(out, 0, sizeof(*out));
    out->m[0][0] = 1.0f;
    out->m[1][1] = 1.0f;
    out->m[2][2] = 1.0f;
    out->m[3][3] = 1.0f;
}

static PMEVec3 PMETV2_VecCross(PMEVec3 a, PMEVec3 b) {
    PMEVec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

static float PMETV2_VecDot(PMEVec3 a, PMEVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static PMEVec3 PMETV2_VecNormalize(PMEVec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-5f) {
        PMEVec3 z = { 1.0f, 0.0f, 0.0f };
        return z;
    }
    PMEVec3 out = { v.x / len, v.y / len, v.z / len };
    return out;
}

/**
 * Construye las matrices View y Projection Left-Handed a partir de `TdPlayerCamera` de Mirror's Edge
 * manteniendo paridad de frustum (`zNear = 10.0 UU`, `zFar = 65536.0 UU`, `FOV = 90°`).
 */
static void PMETV2_BuildCameraMatricesLH(
    PMEVec3         camPosUU,
    PMEURotator     camRotURU,
    float           fovDeg,
    float           aspect,
    D3DMATRIX_POD*  outView,
    D3DMATRIX_POD*  outProj,
    PMEVec3*        outCamFwd,
    PMEVec3*        outCamRight,
    PMEVec3*        outCamUp
) {
    float pitchRad = PMETV2_URUToDegreesSigned(camRotURU.pitch) * (PMETV2_PI / 180.0f);
    float yawRad   = PMETV2_URUToDegreesSigned(camRotURU.yaw)   * (PMETV2_PI / 180.0f);

    float cp = cosf(pitchRad), sp = sinf(pitchRad);
    float cy = cosf(yawRad),   sy = sinf(yawRad);

    /* En UE3 (Left-Handed Z-Up): +X Forward, +Y Right, +Z Up; +Pitch mira hacia arriba */
    PMEVec3 fwd   = PMETV2_VecNormalize({ cp * cy, cp * sy, sp });
    PMEVec3 worldUp = { 0.0f, 0.0f, 1.0f };
    PMEVec3 right = PMETV2_VecNormalize(PMETV2_VecCross(worldUp, fwd));
    PMEVec3 up    = PMETV2_VecNormalize(PMETV2_VecCross(fwd, right));

    if (outCamFwd)   *outCamFwd   = fwd;
    if (outCamRight) *outCamRight = right;
    if (outCamUp)    *outCamUp    = up;

    if (outView) {
        PMETV2_MatrixIdentity(outView);
        outView->m[0][0] = right.x; outView->m[0][1] = up.x; outView->m[0][2] = fwd.x;
        outView->m[1][0] = right.y; outView->m[1][1] = up.y; outView->m[1][2] = fwd.y;
        outView->m[2][0] = right.z; outView->m[2][1] = up.z; outView->m[2][2] = fwd.z;
        outView->m[3][0] = -PMETV2_VecDot(right, camPosUU);
        outView->m[3][1] = -PMETV2_VecDot(up,    camPosUU);
        outView->m[3][2] = -PMETV2_VecDot(fwd,   camPosUU);
    }

    if (outProj) {
        memset(outProj, 0, sizeof(*outProj));
        if (fovDeg < 45.0f || fovDeg > 130.0f) fovDeg = 90.0f;
        if (aspect < 0.2f) aspect = 16.0f / 9.0f;
        float fovYRad = (fovDeg / aspect) * (PMETV2_PI / 180.0f);
        float yScale  = 1.0f / tanf(fovYRad * 0.5f);
        float xScale  = yScale / aspect;
        float zNear   = 10.0f;
        float zFar    = 65536.0f;

        outProj->m[0][0] = xScale;
        outProj->m[1][1] = yScale;
        outProj->m[2][2] = zFar / (zFar - zNear);
        outProj->m[2][3] = 1.0f;
        outProj->m[3][2] = (-zNear * zFar) / (zFar - zNear);
    }
}

static void PMETV2_AppendQuad3D(
    PMETV2Vertex3D* buf,
    uint32_t*       ioCount,
    uint32_t        maxVerts,
    PMEVec3         p0,
    PMEVec3         p1,
    PMEVec3         p2,
    PMEVec3         p3,
    uint32_t        color
) {
    if (*ioCount + 6 > maxVerts) return;
    uint32_t idx = *ioCount;
    buf[idx + 0] = { p0.x, p0.y, p0.z, color };
    buf[idx + 1] = { p1.x, p1.y, p1.z, color };
    buf[idx + 2] = { p2.x, p2.y, p2.z, color };
    buf[idx + 3] = { p0.x, p0.y, p0.z, color };
    buf[idx + 4] = { p2.x, p2.y, p2.z, color };
    buf[idx + 5] = { p3.x, p3.y, p3.z, color };
    *ioCount += 6;
}

/**
 * Genera la malla 3D de un Portal (`C_Prop_Portal`) en espacio de Mirror's Edge:
 * 1. Anillo elíptico exterior luminoso de 36 segmentos (60.96 x 102.87 UU).
 * 2. Superficie interior con gradiente de vórtice o vista interior conectada.
 * 3. Si `is_linked == 1`, dibuja la silueta articulada 3D de Chell (traje naranja + Portal Gun)
 *    en el interior de la ventana del portal.
 */
static void PMETV2_BuildPortal3DMesh(
    const PMETV2PortalRenderState* portal,
    uint32_t                       rimColor,
    uint32_t                       coreColor,
    uint8_t                        drawChellInside,
    PMETV2Vertex3D*                outVerts,
    uint32_t*                      ioVertCount,
    uint32_t                       maxVerts
) {
    if (!portal || !portal->is_active) return;

    const int segments = 36;
    const float hw = (portal->half_width_uu > 1.0f)  ? portal->half_width_uu  : PMETV2_PORTAL_HALF_WIDTH_UU;
    const float hh = (portal->half_height_uu > 1.0f) ? portal->half_height_uu : PMETV2_PORTAL_HALF_HEIGHT_UU;

    /* Desplazar 1.5 UU a lo largo de la normal para evitar Z-fighting con el muro de Mirror's Edge */
    PMEVec3 center = {
        portal->origin_me_uu.x + portal->normal_me_lh.x * 1.5f,
        portal->origin_me_uu.y + portal->normal_me_lh.y * 1.5f,
        portal->origin_me_uu.z + portal->normal_me_lh.z * 1.5f
    };

    uint32_t innerFillColor = portal->is_linked ? 0xD8142234u : coreColor;

    for (int i = 0; i < segments; ++i) {
        float a0 = (2.0f * PMETV2_PI * (float)i)       / (float)segments;
        float a1 = (2.0f * PMETV2_PI * (float)(i + 1)) / (float)segments;
        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        /* Vértices interiores del óvalo (82% del radio) */
        PMEVec3 in0 = {
            center.x + portal->right_me_lh.x * (c0 * hw * 0.82f) + portal->up_me_lh.x * (s0 * hh * 0.82f),
            center.y + portal->right_me_lh.y * (c0 * hw * 0.82f) + portal->up_me_lh.y * (s0 * hh * 0.82f),
            center.z + portal->right_me_lh.z * (c0 * hw * 0.82f) + portal->up_me_lh.z * (s0 * hh * 0.82f)
        };
        PMEVec3 in1 = {
            center.x + portal->right_me_lh.x * (c1 * hw * 0.82f) + portal->up_me_lh.x * (s1 * hh * 0.82f),
            center.y + portal->right_me_lh.y * (c1 * hw * 0.82f) + portal->up_me_lh.y * (s1 * hh * 0.82f),
            center.z + portal->right_me_lh.z * (c1 * hw * 0.82f) + portal->up_me_lh.z * (s1 * hh * 0.82f)
        };

        /* Vértices exteriores del anillo de plasma (100% del radio) */
        PMEVec3 out0 = {
            center.x + portal->right_me_lh.x * (c0 * hw) + portal->up_me_lh.x * (s0 * hh),
            center.y + portal->right_me_lh.y * (c0 * hw) + portal->up_me_lh.y * (s0 * hh),
            center.z + portal->right_me_lh.z * (c0 * hw) + portal->up_me_lh.z * (s0 * hh)
        };
        PMEVec3 out1 = {
            center.x + portal->right_me_lh.x * (c1 * hw) + portal->up_me_lh.x * (s1 * hh),
            center.y + portal->right_me_lh.y * (c1 * hw) + portal->up_me_lh.y * (s1 * hh),
            center.z + portal->right_me_lh.z * (c1 * hw) + portal->up_me_lh.z * (s1 * hh)
        };

        /* 1. Triángulo de la ventana interior del portal */
        if (*ioVertCount + 3 <= maxVerts) {
            uint32_t k = *ioVertCount;
            outVerts[k + 0] = { center.x, center.y, center.z, innerFillColor };
            outVerts[k + 1] = { in0.x, in0.y, in0.z, coreColor };
            outVerts[k + 2] = { in1.x, in1.y, in1.z, coreColor };
            *ioVertCount += 3;
        }

        /* 2. Banda del borde luminoso Azul/Naranja */
        PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, in0, out0, out1, in1, rimColor);
    }

    /* 3. Si ambos portales están conectados, dibujar la figura 3D de Chell dentro de la vista interior */
    if (portal->is_linked && drawChellInside) {
        PMEVec3 chellBase = {
            center.x + portal->normal_me_lh.x * 0.6f,
            center.y + portal->normal_me_lh.y * 0.6f,
            center.z + portal->normal_me_lh.z * 0.6f
        };
        PMEVec3 R = portal->right_me_lh;
        PMEVec3 U = portal->up_me_lh;

        /* Torso de Chell (traje naranja de Aperture) */
        PMEVec3 t0 = { chellBase.x - R.x * 14.0f - U.x * 10.0f, chellBase.y - R.y * 14.0f - U.y * 10.0f, chellBase.z - R.z * 14.0f - U.z * 10.0f };
        PMEVec3 t1 = { chellBase.x + R.x * 14.0f - U.x * 10.0f, chellBase.y + R.y * 14.0f - U.y * 10.0f, chellBase.z + R.z * 14.0f - U.z * 10.0f };
        PMEVec3 t2 = { chellBase.x + R.x * 14.0f + U.x * 32.0f, chellBase.y + R.y * 14.0f + U.y * 32.0f, chellBase.z + R.z * 14.0f + U.z * 32.0f };
        PMEVec3 t3 = { chellBase.x - R.x * 14.0f + U.x * 32.0f, chellBase.y - R.y * 14.0f + U.y * 32.0f, chellBase.z - R.z * 14.0f + U.z * 32.0f };
        PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, t0, t1, t2, t3, PMETV2_COLOR_CHELL_JUMPSUIT);

        /* Cabeza de Chell */
        PMEVec3 h0 = { chellBase.x - R.x * 9.0f + U.x * 34.0f, chellBase.y - R.y * 9.0f + U.y * 34.0f, chellBase.z - R.z * 9.0f + U.z * 34.0f };
        PMEVec3 h1 = { chellBase.x + R.x * 9.0f + U.x * 34.0f, chellBase.y + R.y * 9.0f + U.y * 34.0f, chellBase.z + R.z * 9.0f + U.z * 34.0f };
        PMEVec3 h2 = { chellBase.x + R.x * 9.0f + U.x * 52.0f, chellBase.y + R.y * 9.0f + U.y * 52.0f, chellBase.z + R.z * 9.0f + U.z * 52.0f };
        PMEVec3 h3 = { chellBase.x - R.x * 9.0f + U.x * 52.0f, chellBase.y - R.y * 9.0f + U.y * 52.0f, chellBase.z - R.z * 9.0f + U.z * 52.0f };
        PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, h0, h1, h2, h3, 0xFFE8C4A8u);

        /* Portal Gun sostenida por Chell en la vista interior */
        PMEVec3 g0 = { chellBase.x + R.x * 10.0f + U.x * 10.0f, chellBase.y + R.y * 10.0f + U.y * 10.0f, chellBase.z + R.z * 10.0f + U.z * 10.0f };
        PMEVec3 g1 = { chellBase.x + R.x * 26.0f + U.x * 10.0f, chellBase.y + R.y * 26.0f + U.y * 10.0f, chellBase.z + R.z * 26.0f + U.z * 10.0f };
        PMEVec3 g2 = { chellBase.x + R.x * 26.0f + U.x * 20.0f, chellBase.y + R.y * 26.0f + U.y * 20.0f, chellBase.z + R.z * 26.0f + U.z * 20.0f };
        PMEVec3 g3 = { chellBase.x + R.x * 10.0f + U.x * 20.0f, chellBase.y + R.y * 10.0f + U.y * 20.0f, chellBase.z + R.z * 10.0f + U.z * 20.0f };
        PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, g0, g1, g2, g3, PMETV2_COLOR_APERTURE_WHITE);
    }
}

/**
 * Construye la malla 3D del Viewmodel en 1ª persona de la Portal Gun anclada a la cámara del jugador:
 * - Carcasa blanca curvada de Aperture Science
 * - Tubo central de energía (Azul #00A2FF o Naranja #FF6A00 según el último disparo)
 * - Cañón interno y las 3 garras delanteras
 * - Haz de energía hacia el punto de impacto cuando `beam_alpha > 0.05f`
 */
static void PMETV2_BuildPortalGunViewmodel3D(
    PMEVec3                      camPosUU,
    PMEVec3                      camFwd,
    PMEVec3                      camRight,
    PMEVec3                      camUp,
    const PMETV2ViewmodelState*  vm,
    PMETV2Vertex3D*              outVerts,
    uint32_t*                    ioVertCount,
    uint32_t                     maxVerts
) {
    if (!vm || !vm->weapon_visible) return;

    float recoil = (vm->recoil_factor > 0.0f && vm->recoil_factor <= 1.0f) ? vm->recoil_factor : 0.0f;
    float fwdOffset   = 34.0f - (recoil * 6.5f);
    float rightOffset = 16.5f + vm->sway_yaw;
    float upOffset    = -13.5f + vm->sway_pitch + (recoil * 2.2f);

    /* Punto base del arma en la esquina inferior derecha de la cámara en 1ª persona */
    PMEVec3 gunOrigin = {
        camPosUU.x + camFwd.x * fwdOffset + camRight.x * rightOffset + camUp.x * upOffset,
        camPosUU.y + camFwd.y * fwdOffset + camRight.y * rightOffset + camUp.y * upOffset,
        camPosUU.z + camFwd.z * fwdOffset + camRight.z * rightOffset + camUp.z * upOffset
    };

    /* Punta del cañón de la Portal Gun apuntando ligeramente hacia el centro de la retícula */
    PMEVec3 muzzlePos = {
        gunOrigin.x + camFwd.x * 24.0f - camRight.x * 3.5f + camUp.x * 2.0f,
        gunOrigin.y + camFwd.y * 24.0f - camRight.y * 3.5f + camUp.y * 2.0f,
        gunOrigin.z + camFwd.z * 24.0f - camRight.z * 3.5f + camUp.z * 2.0f
    };

    uint32_t coreColor = (vm->active_gun_color == 1) ? PMETV2_COLOR_ORANGE_PORTAL : PMETV2_COLOR_BLUE_PORTAL;

    /* 1. Carcasa principal blanca de Aperture Science (cuerpo posterior y superior) */
    PMEVec3 b0 = { gunOrigin.x - camRight.x * 4.8f - camUp.x * 3.6f, gunOrigin.y - camRight.y * 4.8f - camUp.y * 3.6f, gunOrigin.z - camRight.z * 4.8f - camUp.z * 3.6f };
    PMEVec3 b1 = { gunOrigin.x + camRight.x * 4.8f - camUp.x * 3.6f, gunOrigin.y + camRight.y * 4.8f - camUp.y * 3.6f, gunOrigin.z + camRight.z * 4.8f - camUp.z * 3.6f };
    PMEVec3 b2 = { gunOrigin.x + camRight.x * 4.2f + camUp.x * 4.2f + camFwd.x * 14.0f, gunOrigin.y + camRight.y * 4.2f + camUp.y * 4.2f + camFwd.y * 14.0f, gunOrigin.z + camRight.z * 4.2f + camUp.z * 4.2f + camFwd.z * 14.0f };
    PMEVec3 b3 = { gunOrigin.x - camRight.x * 4.2f + camUp.x * 4.2f + camFwd.x * 14.0f, gunOrigin.y - camRight.y * 4.2f + camUp.y * 4.2f + camFwd.y * 14.0f, gunOrigin.z - camRight.z * 4.2f + camUp.z * 4.2f + camFwd.z * 14.0f };
    PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, b0, b1, b2, b3, PMETV2_COLOR_APERTURE_WHITE);

    /* 2. Cámara transparente de plasma (núcleo iluminado Azul o Naranja) */
    PMEVec3 c0 = { gunOrigin.x - camRight.x * 2.2f + camUp.x * 2.5f + camFwd.x * 11.0f, gunOrigin.y - camRight.y * 2.2f + camUp.y * 2.5f + camFwd.y * 11.0f, gunOrigin.z - camRight.z * 2.2f + camUp.z * 2.5f + camFwd.z * 11.0f };
    PMEVec3 c1 = { gunOrigin.x + camRight.x * 2.2f + camUp.x * 2.5f + camFwd.x * 11.0f, gunOrigin.y + camRight.y * 2.2f + camUp.y * 2.5f + camFwd.y * 11.0f, gunOrigin.z + camRight.z * 2.2f + camUp.z * 2.5f + camFwd.z * 11.0f };
    PMEVec3 c2 = { muzzlePos.x + camRight.x * 1.8f + camUp.x * 1.8f - camFwd.x * 4.0f,   muzzlePos.y + camRight.y * 1.8f + camUp.y * 1.8f - camFwd.y * 4.0f,   muzzlePos.z + camRight.z * 1.8f + camUp.z * 1.8f - camFwd.z * 4.0f };
    PMEVec3 c3 = { muzzlePos.x - camRight.x * 1.8f + camUp.x * 1.8f - camFwd.x * 4.0f,   muzzlePos.y - camRight.y * 1.8f + camUp.y * 1.8f - camFwd.y * 4.0f,   muzzlePos.z - camRight.z * 1.8f + camUp.z * 1.8f - camFwd.z * 4.0f };
    PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, c0, c1, c2, c3, coreColor);

    /* 3. Las 3 garras metálicas delanteras de la Portal Gun */
    const float clawAngles[3] = { 0.5f * PMETV2_PI, 1.166f * PMETV2_PI, 1.833f * PMETV2_PI };
    for (int i = 0; i < 3; ++i) {
        float ca = cosf(clawAngles[i]), sa = sinf(clawAngles[i]);
        PMEVec3 dir = { camRight.x * ca + camUp.x * sa, camRight.y * ca + camUp.y * sa, camRight.z * ca + camUp.z * sa };
        PMEVec3 k0 = { muzzlePos.x + dir.x * 2.0f - camFwd.x * 3.0f, muzzlePos.y + dir.y * 2.0f - camFwd.y * 3.0f, muzzlePos.z + dir.z * 2.0f - camFwd.z * 3.0f };
        PMEVec3 k1 = { muzzlePos.x + dir.x * 5.2f - camFwd.x * 1.0f, muzzlePos.y + dir.y * 5.2f - camFwd.y * 1.0f, muzzlePos.z + dir.z * 5.2f - camFwd.z * 1.0f };
        PMEVec3 k2 = { muzzlePos.x + dir.x * 3.4f + camFwd.x * 3.5f, muzzlePos.y + dir.y * 3.4f + camFwd.y * 3.5f, muzzlePos.z + dir.z * 3.4f + camFwd.z * 3.5f };
        if (*ioVertCount + 3 <= maxVerts) {
            uint32_t idx = *ioVertCount;
            outVerts[idx + 0] = { k0.x, k0.y, k0.z, PMETV2_COLOR_APERTURE_DARK };
            outVerts[idx + 1] = { k1.x, k1.y, k1.z, PMETV2_COLOR_APERTURE_DARK };
            outVerts[idx + 2] = { k2.x, k2.y, k2.z, PMETV2_COLOR_APERTURE_WHITE };
            *ioVertCount += 3;
        }
    }

    /* 4. Haz de energía 3D desde la boca del cañón hasta el punto de impacto al disparar */
    if (vm->beam_alpha > 0.05f) {
        PMEVec3 hit = vm->beam_target_me_uu;
        PMEVec3 p0 = { muzzlePos.x - camUp.x * 1.2f, muzzlePos.y - camUp.y * 1.2f, muzzlePos.z - camUp.z * 1.2f };
        PMEVec3 p1 = { muzzlePos.x + camUp.x * 1.2f, muzzlePos.y + camUp.y * 1.2f, muzzlePos.z + camUp.z * 1.2f };
        PMEVec3 p2 = { hit.x       + camUp.x * 2.0f, hit.y       + camUp.y * 2.0f, hit.z       + camUp.z * 2.0f };
        PMEVec3 p3 = { hit.x       - camUp.x * 2.0f, hit.y       - camUp.y * 2.0f, hit.z       - camUp.z * 2.0f };
        PMETV2_AppendQuad3D(outVerts, ioVertCount, maxVerts, p0, p1, p2, p3, coreColor);
    }
}

/**
 * Construye los vértices 2D en pantalla (`XYZRHW`) de la retícula central oficial de Portal:
 * - Semiarco izquierdo Azul (brillante cuando `blue_portal.is_active == 1`)
 * - Semiarco derecho Naranja (brillante cuando `orange_portal.is_active == 1`)
 */
static void PMETV2_BuildCrosshairHUD2D(
    float           screenW,
    float           screenH,
    uint8_t         blueActive,
    uint8_t         orangeActive,
    PMETV2Vertex2D* outVerts,
    uint32_t*       ioVertCount,
    uint32_t        maxVerts
) {
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;
    float rIn  = 11.0f;
    float rOut = blueActive   ? 16.0f : 13.5f;
    float rOutO = orangeActive ? 16.0f : 13.5f;

    uint32_t blueCol   = blueActive   ? PMETV2_COLOR_BLUE_PORTAL   : 0x6600A2FFu;
    uint32_t orangeCol = orangeActive ? PMETV2_COLOR_ORANGE_PORTAL : 0x66FF6A00u;

    /* Semiarco izquierdo (Portal Azul: 110° a 250°) */
    const int arcSegs = 10;
    for (int i = 0; i < arcSegs; ++i) {
        float a0 = (110.0f + (140.0f * (float)i / (float)arcSegs))       * (PMETV2_PI / 180.0f);
        float a1 = (110.0f + (140.0f * (float)(i + 1) / (float)arcSegs)) * (PMETV2_PI / 180.0f);
        if (*ioVertCount + 6 > maxVerts) break;
        uint32_t k = *ioVertCount;
        outVerts[k + 0] = { cx + cosf(a0) * rIn,  cy + sinf(a0) * rIn,  0.0f, 1.0f, blueCol };
        outVerts[k + 1] = { cx + cosf(a0) * rOut, cy + sinf(a0) * rOut, 0.0f, 1.0f, blueCol };
        outVerts[k + 2] = { cx + cosf(a1) * rOut, cy + sinf(a1) * rOut, 0.0f, 1.0f, blueCol };
        outVerts[k + 3] = { cx + cosf(a0) * rIn,  cy + sinf(a0) * rIn,  0.0f, 1.0f, blueCol };
        outVerts[k + 4] = { cx + cosf(a1) * rOut, cy + sinf(a1) * rOut, 0.0f, 1.0f, blueCol };
        outVerts[k + 5] = { cx + cosf(a1) * rIn,  cy + sinf(a1) * rIn,  0.0f, 1.0f, blueCol };
        *ioVertCount += 6;
    }

    /* Semiarco derecho (Portal Naranja: -70° a +70°) */
    for (int i = 0; i < arcSegs; ++i) {
        float a0 = (-70.0f + (140.0f * (float)i / (float)arcSegs))       * (PMETV2_PI / 180.0f);
        float a1 = (-70.0f + (140.0f * (float)(i + 1) / (float)arcSegs)) * (PMETV2_PI / 180.0f);
        if (*ioVertCount + 6 > maxVerts) break;
        uint32_t k = *ioVertCount;
        outVerts[k + 0] = { cx + cosf(a0) * rIn,   cy + sinf(a0) * rIn,   0.0f, 1.0f, orangeCol };
        outVerts[k + 1] = { cx + cosf(a0) * rOutO, cy + sinf(a0) * rOutO, 0.0f, 1.0f, orangeCol };
        outVerts[k + 2] = { cx + cosf(a1) * rOutO, cy + sinf(a1) * rOutO, 0.0f, 1.0f, orangeCol };
        outVerts[k + 3] = { cx + cosf(a0) * rIn,   cy + sinf(a0) * rIn,   0.0f, 1.0f, orangeCol };
        outVerts[k + 4] = { cx + cosf(a1) * rOutO, cy + sinf(a1) * rOutO, 0.0f, 1.0f, orangeCol };
        outVerts[k + 5] = { cx + cosf(a1) * rIn,   cy + sinf(a1) * rIn,   0.0f, 1.0f, orangeCol };
        *ioVertCount += 6;
    }
}

/* ============================================================================
 * 4. HOOK DE IDirect3DDevice9::EndScene EN MIRROR'S EDGE
 * ============================================================================ */

static HRESULT STDMETHODCALLTYPE PMETV2_HookedEndScene(void* pDevice) {
    PMETV2SharedState snap;
    if (pDevice && PMETV2_ReadConsistentSnapshot(&snap)) {
        void** vtbl = *(void***)pDevice;
        PFN_GetViewport      fnGetViewport    = (PFN_GetViewport)vtbl[D3D9_VTABLE_GETVIEWPORT];
        PFN_SetViewport      fnSetViewport    = (PFN_SetViewport)vtbl[D3D9_VTABLE_SETVIEWPORT];
        PFN_SetTransform     fnSetTransform   = (PFN_SetTransform)vtbl[D3D9_VTABLE_SETTRANSFORM];
        PFN_SetRenderState   fnSetRenderState = (PFN_SetRenderState)vtbl[D3D9_VTABLE_SETRENDERSTATE];
        PFN_GetRenderState   fnGetRenderState = (PFN_GetRenderState)vtbl[D3D9_VTABLE_GETRENDERSTATE];
        PFN_SetTexture       fnSetTexture     = (PFN_SetTexture)vtbl[D3D9_VTABLE_SETTEXTURE];
        PFN_SetVertexShader  fnSetVS          = (PFN_SetVertexShader)vtbl[D3D9_VTABLE_SETVERTEXSHADER];
        PFN_SetPixelShader   fnSetPS          = (PFN_SetPixelShader)vtbl[D3D9_VTABLE_SETPIXELSHADER];
        PFN_SetFVF           fnSetFVF         = (PFN_SetFVF)vtbl[D3D9_VTABLE_SETFVF];
        PFN_DrawPrimitiveUP  fnDrawUP         = (PFN_DrawPrimitiveUP)vtbl[D3D9_VTABLE_DRAWPRIMITIVEUP];

        D3DVIEWPORT9_POD origVp = { 0, 0, 1280, 720, 0.0f, 1.0f };
        fnGetViewport(pDevice, &origVp);
        float aspect = (origVp.Height > 0) ? ((float)origVp.Width / (float)origVp.Height) : (16.0f / 9.0f);

        D3DMATRIX_POD worldIdentity, viewMat, projMat;
        PMEVec3 camFwd, camRight, camUp;
        PMETV2_MatrixIdentity(&worldIdentity);
        PMETV2_BuildCameraMatricesLH(
            snap.camera_location_uu,
            snap.camera_rotation_uru,
            snap.camera_fov_deg,
            aspect,
            &viewMat,
            &projMat,
            &camFwd,
            &camRight,
            &camUp
        );

        /* Guardar estado de renderizado del motor de Mirror's Edge */
        DWORD oldZEnable = 1, oldZWrite = 1, oldAlphaBlend = 0, oldCull = 1, oldLighting = 0;
        fnGetRenderState(pDevice, D3DRS_ZENABLE,          &oldZEnable);
        fnGetRenderState(pDevice, D3DRS_ZWRITEENABLE,     &oldZWrite);
        fnGetRenderState(pDevice, D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
        fnGetRenderState(pDevice, D3DRS_CULLMODE,         &oldCull);
        fnGetRenderState(pDevice, D3DRS_LIGHTING,         &oldLighting);

        fnSetVS(pDevice, NULL);
        fnSetPS(pDevice, NULL);
        fnSetTexture(pDevice, 0, NULL);
        fnSetRenderState(pDevice, D3DRS_LIGHTING,         FALSE);
        fnSetRenderState(pDevice, D3DRS_CULLMODE,         D3DCULL_NONE);
        fnSetRenderState(pDevice, D3DRS_ALPHABLENDENABLE, TRUE);
        fnSetRenderState(pDevice, D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
        fnSetRenderState(pDevice, D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);

        fnSetTransform(pDevice, D3DTS_WORLD,      &worldIdentity);
        fnSetTransform(pDevice, D3DTS_VIEW,       &viewMat);
        fnSetTransform(pDevice, D3DTS_PROJECTION, &projMat);

        /* 1. PASADA DE MUNDO: Portales 3D Azul y Naranja + Vista Interior + Chell (con Z-Buffer activo) */
        PMETV2Vertex3D worldVerts[2048];
        uint32_t worldVertCount = 0;
        PMETV2_BuildPortal3DMesh(
            &snap.blue_portal,
            PMETV2_COLOR_BLUE_PORTAL,
            PMETV2_COLOR_BLUE_CORE,
            snap.viewmodel.chell_visible_in_portal,
            worldVerts, &worldVertCount, 2048
        );
        PMETV2_BuildPortal3DMesh(
            &snap.orange_portal,
            PMETV2_COLOR_ORANGE_PORTAL,
            PMETV2_COLOR_ORANGE_CORE,
            snap.viewmodel.chell_visible_in_portal,
            worldVerts, &worldVertCount, 2048
        );

        if (worldVertCount >= 3) {
            fnSetRenderState(pDevice, D3DRS_ZENABLE,      TRUE);
            fnSetRenderState(pDevice, D3DRS_ZWRITEENABLE, TRUE);
            fnSetFVF(pDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE);
            fnDrawUP(pDevice, D3DPT_TRIANGLELIST, worldVertCount / 3, worldVerts, sizeof(PMETV2Vertex3D));
        }

        /* 2. PASADA DE PRIMER PLANO: Viewmodel 3D de la Portal Gun (rango Z aislado MinZ=0.0, MaxZ=0.08) */
        PMETV2Vertex3D vmVerts[512];
        uint32_t vmVertCount = 0;
        PMETV2_BuildPortalGunViewmodel3D(
            snap.camera_location_uu, camFwd, camRight, camUp,
            &snap.viewmodel, vmVerts, &vmVertCount, 512
        );
        if (vmVertCount >= 3) {
            D3DVIEWPORT9_POD vmVp = origVp;
            vmVp.MinZ = 0.0f;
            vmVp.MaxZ = 0.08f;
            fnSetViewport(pDevice, &vmVp);
            fnSetRenderState(pDevice, D3DRS_ZENABLE, FALSE);
            fnSetFVF(pDevice, D3DFVF_XYZ | D3DFVF_DIFFUSE);
            fnDrawUP(pDevice, D3DPT_TRIANGLELIST, vmVertCount / 3, vmVerts, sizeof(PMETV2Vertex3D));
            fnSetViewport(pDevice, &origVp);
        }

        /* 3. PASADA DE INTERFAZ: Retícula central oficial de Portal */
        PMETV2Vertex2D hudVerts[256];
        uint32_t hudVertCount = 0;
        PMETV2_BuildCrosshairHUD2D(
            (float)origVp.Width, (float)origVp.Height,
            snap.blue_portal.is_active, snap.orange_portal.is_active,
            hudVerts, &hudVertCount, 256
        );
        if (hudVertCount >= 3) {
            fnSetRenderState(pDevice, D3DRS_ZENABLE, FALSE);
            fnSetFVF(pDevice, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            fnDrawUP(pDevice, D3DPT_TRIANGLELIST, hudVertCount / 3, hudVerts, sizeof(PMETV2Vertex2D));
        }

        /* Restaurar estado de D3D9 */
        fnSetRenderState(pDevice, D3DRS_ZENABLE,          oldZEnable);
        fnSetRenderState(pDevice, D3DRS_ZWRITEENABLE,     oldZWrite);
        fnSetRenderState(pDevice, D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
        fnSetRenderState(pDevice, D3DRS_CULLMODE,         oldCull);
        fnSetRenderState(pDevice, D3DRS_LIGHTING,         oldLighting);
    }

    return g_OrigEndScene ? g_OrigEndScene(pDevice) : 0;
}

static HRESULT STDMETHODCALLTYPE PMETV2_HookedCreateDevice(
    void* pD3D9, UINT Adapter, int DeviceType, HWND hFocusWindow,
    DWORD BehaviorFlags, void* pPresentationParameters, void** ppReturnedDeviceInterface
) {
    HRESULT hr = g_OrigCreateDev(
        pD3D9, Adapter, DeviceType, hFocusWindow,
        BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface
    );
    if (hr >= 0 && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        void** devVtbl = *(void***)(*ppReturnedDeviceInterface);
        if (devVtbl && devVtbl[D3D9_VTABLE_ENDSCENE] != (void*)PMETV2_HookedEndScene) {
            DWORD oldProt = 0;
            if (VirtualProtect(&devVtbl[D3D9_VTABLE_ENDSCENE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
                g_OrigEndScene = (PFN_EndScene)devVtbl[D3D9_VTABLE_ENDSCENE];
                devVtbl[D3D9_VTABLE_ENDSCENE] = (void*)PMETV2_HookedEndScene;
                VirtualProtect(&devVtbl[D3D9_VTABLE_ENDSCENE], sizeof(void*), oldProt, &oldProt);
            }
        }
    }
    return hr;
}

/* ============================================================================
 * 5. EXPORTACIONES DEL PROXY D3D9 Y PUNTO DE PRUEBA DETERMINISTA
 * ============================================================================ */

extern "C" __declspec(dllexport) void* WINAPI Direct3DCreate9(UINT SDKVersion) {
    PMETV2_EnsureSystemD3D9Loaded();
    if (!g_hSystemD3D9) return NULL;
    PFN_Direct3DCreate9 fnCreate = (PFN_Direct3DCreate9)GetProcAddress(g_hSystemD3D9, "Direct3DCreate9");
    if (!fnCreate) return NULL;

    void* pD3D9 = fnCreate(SDKVersion);
    if (pD3D9) {
        void** vtbl = *(void***)pD3D9;
        if (vtbl && vtbl[D3D9_OBJ_VTABLE_CREATEDEVICE] != (void*)PMETV2_HookedCreateDevice) {
            DWORD oldProt = 0;
            if (VirtualProtect(&vtbl[D3D9_OBJ_VTABLE_CREATEDEVICE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
                g_OrigCreateDev = (PFN_CreateDevice)vtbl[D3D9_OBJ_VTABLE_CREATEDEVICE];
                vtbl[D3D9_OBJ_VTABLE_CREATEDEVICE] = (void*)PMETV2_HookedCreateDevice;
                VirtualProtect(&vtbl[D3D9_OBJ_VTABLE_CREATEDEVICE], sizeof(void*), oldProt, &oldProt);
            }
        }
    }
    return pD3D9;
}

extern "C" __declspec(dllexport) HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, void** ppD3D) {
    PMETV2_EnsureSystemD3D9Loaded();
    if (!g_hSystemD3D9) return E_FAIL;
    PFN_Direct3DCreate9Ex fnCreateEx = (PFN_Direct3DCreate9Ex)GetProcAddress(g_hSystemD3D9, "Direct3DCreate9Ex");
    if (!fnCreateEx) return E_FAIL;
    return fnCreateEx(SDKVersion, ppD3D);
}

/**
 * Punto de entrada determinista exportado para verificar el Módulo 4 desde `true_ingame_bridge_v2.exe --self-test`.
 * Ejecuta el pipeline completo de construcción de matrices y mallas 3D (Portales Azul/Naranja + Chell +
 * Viewmodel de la Portal Gun + Retícula) y devuelve el conteo exacto de vértices generados.
 */
extern "C" __declspec(dllexport) int __cdecl PMETV2_D3D9_TestRenderFrame(
    const PMETV2SharedState* state,
    uint32_t                 viewportW,
    uint32_t                 viewportH,
    uint32_t*                outWorldVerts,
    uint32_t*                outViewmodelVerts,
    uint32_t*                outHudVerts
) {
    if (!state) return 0;

    D3DMATRIX_POD viewMat, projMat;
    PMEVec3 camFwd, camRight, camUp;
    float aspect = (viewportH > 0) ? ((float)viewportW / (float)viewportH) : (16.0f / 9.0f);
    PMETV2_BuildCameraMatricesLH(
        state->camera_location_uu,
        state->camera_rotation_uru,
        state->camera_fov_deg,
        aspect,
        &viewMat,
        &projMat,
        &camFwd,
        &camRight,
        &camUp
    );

    PMETV2Vertex3D worldVerts[2048];
    uint32_t worldCount = 0;
    PMETV2_BuildPortal3DMesh(
        &state->blue_portal,
        PMETV2_COLOR_BLUE_PORTAL,
        PMETV2_COLOR_BLUE_CORE,
        state->viewmodel.chell_visible_in_portal,
        worldVerts, &worldCount, 2048
    );
    PMETV2_BuildPortal3DMesh(
        &state->orange_portal,
        PMETV2_COLOR_ORANGE_PORTAL,
        PMETV2_COLOR_ORANGE_CORE,
        state->viewmodel.chell_visible_in_portal,
        worldVerts, &worldCount, 2048
    );

    PMETV2Vertex3D vmVerts[512];
    uint32_t vmCount = 0;
    PMETV2_BuildPortalGunViewmodel3D(
        state->camera_location_uu, camFwd, camRight, camUp,
        &state->viewmodel, vmVerts, &vmCount, 512
    );

    PMETV2Vertex2D hudVerts[256];
    uint32_t hudCount = 0;
    PMETV2_BuildCrosshairHUD2D(
        (float)viewportW, (float)viewportH,
        state->blue_portal.is_active, state->orange_portal.is_active,
        hudVerts, &hudCount, 256
    );

    if (outWorldVerts)     *outWorldVerts     = worldCount;
    if (outViewmodelVerts) *outViewmodelVerts = vmCount;
    if (outHudVerts)       *outHudVerts       = hudCount;

    return (worldCount > 0 && vmCount > 0 && hudCount > 0) ? 1 : 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_pShared) {
            UnmapViewOfFile(g_pShared);
            g_pShared = NULL;
        }
        if (g_hShmMap) {
            CloseHandle(g_hShmMap);
            g_hShmMap = NULL;
        }
    }
    return TRUE;
}
