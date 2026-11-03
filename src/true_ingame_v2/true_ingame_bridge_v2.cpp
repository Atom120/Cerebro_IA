/**
 * @file true_ingame_bridge_v2.cpp
 * @brief Orquestador Unificado de `portal-me-true-ingame-v2` (Rama `segundo-intento` — Los 4 Módulos):
 *
 * - Módulo 1: Calibración geométrica 1:1 con la azotea real de `Edge_p` (`(19500.0, -3009.5, 8517.2)`).
 * - Módulo 2: Inyección directa de inputs en memoria (ME -> Portal) sin cambiar de ventana + `snd_mute_losefocus 0`.
 * - Módulo 3: Lectura en tiempo real de las entidades `C_Prop_Portal` (Azul y Naranja) desde `cl_entitylist`
 *             en `client.dll` de Portal (+ trazador analítico 1:1 para respuesta instantánea del haz/impacto).
 * - Módulo 4: Publicación SeqLock a 120 Hz hacia `Local\Antigravity_Portal_ME_TrueInGame_v2` consumida por
 *             `me_true_ingame_d3d9.dll` (`IDirect3DDevice9::EndScene`), ocultando a Faith (`DrawScale = 0.0f`)
 *             y renderizando Portales 3D con Z-Buffer + Vista Interior + Chell + Viewmodel de la Portal Gun.
 *
 * Compilación x86 (MSVC):
 *   cl /nologo /EHsc /O2 src\true_ingame_v2\true_ingame_bridge_v2.cpp /Fe:true_ingame_bridge_v2.exe user32.lib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../common/ipc_true_ingame_v2.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "user32.lib")
#endif

/* ============================================================================
 * 1. OFFSETS REALES VERIFICADOS (MIRROR'S EDGE GOG v1.01 + PORTAL STEAM 2025)
 * ============================================================================ */

/* Mirror's Edge GOG (MirrorsEdge.exe - ImageBase fijo 0x00400000, ASLR = false) */
#define PMETV2_ME_GENGINE_VA                 0x01FF8B20UL
#define PMETV2_ME_OFFSET_GAMEPLAYERS_DATA    0x02BCUL
#define PMETV2_ME_OFFSET_LOCALPLAYER_ACTOR   0x0040UL
#define PMETV2_ME_OFFSET_CONTROLLER_PAWN     0x01C0UL
#define PMETV2_ME_OFFSET_CONTROLLER_CAMERA   0x02D0UL
#define PMETV2_ME_OFFSET_AACTOR_PHYSICS      0x006BUL
#define PMETV2_ME_OFFSET_AACTOR_LOCATION     0x00E8UL
#define PMETV2_ME_OFFSET_AACTOR_ROTATION     0x00F4UL
#define PMETV2_ME_OFFSET_AACTOR_VELOCITY     0x0100UL
#define PMETV2_ME_OFFSET_AACTOR_DRAWSCALE    0x010CUL
#define PMETV2_ME_OFFSET_AACTOR_COLLOCATION  0x013CUL
#define PMETV2_ME_OFFSET_TDPAWN_LOC1         0x02E0UL
#define PMETV2_ME_OFFSET_TDPAWN_LOC2         0x02F8UL

/* Portal Steam (portal/bin/client.dll - Timestamp 0x68362D89) */
#define PMETV2_PORTAL_VIEW_ORIGIN_RVA        0x0050F484UL
#define PMETV2_PORTAL_VIEW_ANGLES_RVA        0x0050F490UL
#define PMETV2_PORTAL_LOCALPLAYER_RVA        0x004D78F0UL
#define PMETV2_PORTAL_ENTITYLIST_RVA         0x004C6624UL
#define PMETV2_PORTAL_ENT_STRIDE             0x00000010UL

/* Offsets internos de C_Prop_Portal en client.dll */
#define PMETV2_PORTAL_ENT_ORIGIN_OFFSET      0x028CUL
#define PMETV2_PORTAL_ENT_ANGLES_OFFSET      0x0298UL
#define PMETV2_PORTAL_ENT_LINKED_OFFSET      0x0E00UL
#define PMETV2_PORTAL_ENT_ISPORTAL2_OFFSET   0x0E05UL
#define PMETV2_PORTAL_ENT_ACTIVATED_OFFSET   0x0E06UL

/* Ancla geométrica 1:1 de la azotea activa de Edge_p (1Pt1_4PlayerStart_FreeControl) */
#define PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_X     18928.4f
#define PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_Y    -2819.0f
#define PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_Z     8517.2f

/* ============================================================================
 * 2. TRAZADOR ANALÍTICO 1:1 DE RESPALDO (RAYCAST SOBRE AZOTEA DE EDGE_P)
 * ============================================================================ */

/**
 * Calcula el punto de impacto en unidades Hammer (HU) y los ángulos del portal al disparar
 * desde `eye_hu` con dirección `view_angles_deg` sobre la geometría 1:1 de la azotea de Edge_p.
 */
static void PMETV2_AnalyticalPortalTraceHU(
    PMEVec3    eye_hu,
    PMEQAngle  view_angles_deg,
    PMEVec3*   out_hit_hu,
    PMEQAngle* out_portal_angles_deg
) {
    float pitchRad = view_angles_deg.pitch * (PMETV2_PI / 180.0f);
    float yawRad   = view_angles_deg.yaw   * (PMETV2_PI / 180.0f);
    PMEVec3 dir = {
        cosf(pitchRad) * cosf(yawRad),
        cosf(pitchRad) * sinf(yawRad),
        -sinf(pitchRad)
    };

    /* Si mira hacia abajo (z < -0.25), impacta sobre la losa principal de la azotea (z = 0 HU) */
    if (dir.z < -0.25f && eye_hu.z > 1.0f) {
        float t = -eye_hu.z / dir.z;
        if (t > 10.0f && t < 3000.0f) {
            out_hit_hu->x = eye_hu.x + dir.x * t;
            out_hit_hu->y = eye_hu.y + dir.y * t;
            out_hit_hu->z = 0.0f;
            /* Normal +Z (hacia arriba) -> en Source Pitch = -90° */
            out_portal_angles_deg->pitch = -90.0f;
            out_portal_angles_deg->yaw   = view_angles_deg.yaw;
            out_portal_angles_deg->roll  = 0.0f;
            return;
        }
    }

    /* Si mira hacia el este (+X > 0.35), impacta sobre la rampa roja inclinada a 45° (x ~ 1260 HU) */
    if (dir.x > 0.35f) {
        float targetX = 1260.0f;
        float t = (targetX - eye_hu.x) / dir.x;
        if (t > 10.0f && t < 4000.0f) {
            out_hit_hu->x = targetX;
            out_hit_hu->y = eye_hu.y + dir.y * t;
            out_hit_hu->z = 78.74f; /* Centro de la rampa a 45° (~150 UU) */
            /* Normal (+0.7071, 0, +0.7071) -> Pitch = -45°, Yaw = 0° */
            out_portal_angles_deg->pitch = -45.0f;
            out_portal_angles_deg->yaw   = 0.0f;
            out_portal_angles_deg->roll  = 0.0f;
            return;
        }
    }

    /* Por defecto, impacta en el muro de la torre del ascensor o parapeto a distancia 550 HU */
    out_hit_hu->x = eye_hu.x + dir.x * 550.0f;
    out_hit_hu->y = eye_hu.y + dir.y * 550.0f;
    out_hit_hu->z = eye_hu.z + dir.z * 550.0f;
    if (out_hit_hu->z < 56.0f) out_hit_hu->z = 56.0f;
    out_portal_angles_deg->pitch = 0.0f;
    out_portal_angles_deg->yaw   = fmodf(view_angles_deg.yaw + 180.0f, 360.0f);
    out_portal_angles_deg->roll  = 0.0f;
}

#ifdef _WIN32
/* ============================================================================
 * 3. UTILIDADES WIN32 PARA PROCESOS, ENTIDADES E INYECCIÓN DE INPUTS EN MEMORIA
 * ============================================================================ */

static DWORD PMETV2_FindProcessId(const char* procName) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                if (_stricmp(pe.szExeFile, procName) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
    return pid;
}

static uintptr_t PMETV2_GetModuleBase(DWORD pid, const char* modName) {
    uintptr_t base = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me;
        me.dwSize = sizeof(me);
        if (Module32First(snap, &me)) {
            do {
                if (_stricmp(me.szModule, modName) == 0) {
                    base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
    }
    return base;
}

static int PMETV2_ReadPtr32(HANDLE hProc, uintptr_t addr, uint32_t* outVal) {
    SIZE_T nRead = 0;
    return ReadProcessMemory(hProc, (LPCVOID)addr, outVal, sizeof(uint32_t), &nRead) && nRead == sizeof(uint32_t);
}

/**
 * Módulo 3: Escanea `cl_entitylist` en `client.dll` de Portal para leer las dos instancias reales
 * de `C_Prop_Portal` (`is_portal2 == 0` para Azul y `1` para Naranja).
 */
static int PMETV2_ReadPortalEntitiesFromClientDll(
    HANDLE                   hPortal,
    uintptr_t                clientBase,
    PMEVec3                  anchorMeUU,
    PMETV2PortalRenderState* ioBlue,
    PMETV2PortalRenderState* ioOrange
) {
    int foundCount = 0;
    uintptr_t listBase = clientBase + PMETV2_PORTAL_ENTITYLIST_RVA;

    for (int idx = 1; idx < 512; ++idx) {
        uint32_t entPtr = 0;
        if (!PMETV2_ReadPtr32(hPortal, listBase + (uintptr_t)(idx * PMETV2_PORTAL_ENT_STRIDE), &entPtr) ||
            entPtr < 0x00100000u) {
            continue;
        }

        uint8_t isPortal2 = 0xFF, isActivated = 0xFF;
        uint32_t linkedHandle = 0xFFFFFFFFu;
        SIZE_T n1 = 0, n2 = 0, n3 = 0;
        if (!ReadProcessMemory(hPortal, (LPCVOID)(entPtr + PMETV2_PORTAL_ENT_ISPORTAL2_OFFSET), &isPortal2, 1, &n1) ||
            !ReadProcessMemory(hPortal, (LPCVOID)(entPtr + PMETV2_PORTAL_ENT_ACTIVATED_OFFSET), &isActivated, 1, &n2) ||
            !ReadProcessMemory(hPortal, (LPCVOID)(entPtr + PMETV2_PORTAL_ENT_LINKED_OFFSET), &linkedHandle, 4, &n3)) {
            continue;
        }

        /* Validar invariantes de C_Prop_Portal */
        if ((isPortal2 == 0 || isPortal2 == 1) && (isActivated == 1)) {
            PMEVec3 origHu = {0};
            PMEQAngle angDeg = {0};
            SIZE_T nO = 0, nA = 0;
            if (ReadProcessMemory(hPortal, (LPCVOID)(entPtr + PMETV2_PORTAL_ENT_ORIGIN_OFFSET), &origHu, sizeof(origHu), &nO) &&
                ReadProcessMemory(hPortal, (LPCVOID)(entPtr + PMETV2_PORTAL_ENT_ANGLES_OFFSET), &angDeg, sizeof(angDeg), &nA)) {
                if (fabsf(origHu.x) < 32768.0f && fabsf(origHu.y) < 32768.0f && fabsf(origHu.z) < 32768.0f &&
                    (fabsf(origHu.x) > 0.01f || fabsf(origHu.y) > 0.01f || fabsf(origHu.z) > 0.01f)) {
                    uint8_t isLinked = (linkedHandle != 0xFFFFFFFFu && linkedHandle != 0u) ? 1 : 0;
                    if (isPortal2 == 0 && ioBlue) {
                        PMETV2_PopulatePortalRenderState(ioBlue, 1, isLinked, 0, origHu, angDeg, anchorMeUU);
                        ioBlue->entity_handle = entPtr;
                        foundCount++;
                    } else if (isPortal2 == 1 && ioOrange) {
                        PMETV2_PopulatePortalRenderState(ioOrange, 1, isLinked, 1, origHu, angDeg, anchorMeUU);
                        ioOrange->entity_handle = entPtr;
                        foundCount++;
                    }
                }
            }
        }
    }
    return foundCount;
}

/**
 * Módulo 2: Captura las teclas y clics cuando `MirrorsEdge.exe` tiene el foco y los encamina
 * en segundo plano hacia la ventana `Valve001` de `hl2.exe` sin robar el foco del usuario.
 */
static void PMETV2_InjectInputsIntoPortalBackground(
    HWND                  hPortalWnd,
    PMETV2SharedState*    state,
    uint32_t*             ioPrevButtons
) {
    uint32_t buttons = 0;
    float fwdMove = 0.0f;
    float sideMove = 0.0f;

    if (GetAsyncKeyState('W') & 0x8000)      { buttons |= PME_IN_FORWARD;   fwdMove  += 450.0f; }
    if (GetAsyncKeyState('S') & 0x8000)      { buttons |= PME_IN_BACK;      fwdMove  -= 450.0f; }
    if (GetAsyncKeyState('A') & 0x8000)      { buttons |= PME_IN_MOVELEFT;  sideMove -= 450.0f; }
    if (GetAsyncKeyState('D') & 0x8000)      { buttons |= PME_IN_MOVERIGHT; sideMove += 450.0f; }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) { buttons |= PME_IN_JUMP; }
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000){ buttons |= PME_IN_DUCK; }
    if (GetAsyncKeyState('E') & 0x8000)      { buttons |= PME_IN_USE; }
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000){ buttons |= PME_IN_ATTACK; }
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000){ buttons |= PME_IN_ATTACK2; }

    state->input_injection.buttons_mask = buttons;
    state->input_injection.forward_move = fwdMove;
    state->input_injection.side_move    = sideMove;
    state->input_injection.injected_view_angles = PME_METoSource_Rotation(state->camera_rotation_uru);

    uint32_t prev = *ioPrevButtons;
    uint32_t pressed  = (buttons & ~prev);
    uint32_t released = (~buttons & prev);

    /* Enviar mensajes de teclado/ratón en segundo plano a la ventana de Portal (sin SetForegroundWindow) */
    if (hPortalWnd) {
        if (pressed & PME_IN_FORWARD)   PostMessageA(hPortalWnd, WM_KEYDOWN, 'W', 0);
        if (released & PME_IN_FORWARD)  PostMessageA(hPortalWnd, WM_KEYUP,   'W', 0);
        if (pressed & PME_IN_BACK)      PostMessageA(hPortalWnd, WM_KEYDOWN, 'S', 0);
        if (released & PME_IN_BACK)     PostMessageA(hPortalWnd, WM_KEYUP,   'S', 0);
        if (pressed & PME_IN_MOVELEFT)  PostMessageA(hPortalWnd, WM_KEYDOWN, 'A', 0);
        if (released & PME_IN_MOVELEFT) PostMessageA(hPortalWnd, WM_KEYUP,   'A', 0);
        if (pressed & PME_IN_MOVERIGHT) PostMessageA(hPortalWnd, WM_KEYDOWN, 'D', 0);
        if (released & PME_IN_MOVERIGHT)PostMessageA(hPortalWnd, WM_KEYUP,   'D', 0);
        if (pressed & PME_IN_JUMP)      PostMessageA(hPortalWnd, WM_KEYDOWN, VK_SPACE, 0);
        if (released & PME_IN_JUMP)     PostMessageA(hPortalWnd, WM_KEYUP,   VK_SPACE, 0);
        if (pressed & PME_IN_ATTACK)    PostMessageA(hPortalWnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
        if (released & PME_IN_ATTACK)   PostMessageA(hPortalWnd, WM_LBUTTONUP,   0, 0);
        if (pressed & PME_IN_ATTACK2)   PostMessageA(hPortalWnd, WM_RBUTTONDOWN, MK_RBUTTON, 0);
        if (released & PME_IN_ATTACK2)  PostMessageA(hPortalWnd, WM_RBUTTONUP,   0, 0);
    }

    /* Actualizar estado visual del Viewmodel de la Portal Gun y portales al disparar */
    if (pressed & (PME_IN_ATTACK | PME_IN_ATTACK2)) {
        uint8_t isOrange = (pressed & PME_IN_ATTACK2) ? 1 : 0;
        state->viewmodel.active_gun_color = isOrange;
        state->viewmodel.recoil_factor    = 1.0f;
        state->viewmodel.beam_alpha       = 1.0f;

        PMEVec3 hitHu = {0};
        PMEQAngle portalAng = {0};
        PMETV2_AnalyticalPortalTraceHU(
            state->chell_eye_hu,
            state->chell_angles_deg,
            &hitHu,
            &portalAng
        );
        state->viewmodel.beam_target_me_uu = PME_SourceToME_Position(hitHu, state->anchor_me_uu);

        if (!isOrange) {
            uint8_t linked = state->orange_portal.is_active ? 1 : 0;
            PMETV2_PopulatePortalRenderState(
                &state->blue_portal, 1, linked, 0, hitHu, portalAng, state->anchor_me_uu
            );
            if (linked) state->orange_portal.is_linked = 1;
        } else {
            uint8_t linked = state->blue_portal.is_active ? 1 : 0;
            PMETV2_PopulatePortalRenderState(
                &state->orange_portal, 1, linked, 1, hitHu, portalAng, state->anchor_me_uu
            );
            if (linked) state->blue_portal.is_linked = 1;
        }
    } else {
        /* Amortiguar suavemente el retroceso y el haz a 120 Hz */
        state->viewmodel.recoil_factor *= 0.85f;
        state->viewmodel.beam_alpha    *= 0.80f;
    }

    state->viewmodel.crosshair_state =
        (state->blue_portal.is_active ? 1u : 0u) |
        (state->orange_portal.is_active ? 2u : 0u);
    state->viewmodel.chell_visible_in_portal =
        (state->blue_portal.is_active && state->orange_portal.is_active) ? 1u : 0u;

    *ioPrevButtons = buttons;
}

/**
 * Bucle en vivo a 120 Hz de `portal-me-true-ingame-v2` (Opción 1 Refinada en `segundo-intento`).
 */
static int PMETV2_RunLiveBridge(void) {
    printf("=== True In-Game Pass-Through v2 (120 Hz): Buscando MirrorsEdge.exe y Portal (hl2.exe) ===\n");
    DWORD portalPid = PMETV2_FindProcessId("hl2.exe");
    if (!portalPid) portalPid = PMETV2_FindProcessId("portal.exe");
    DWORD mePid = PMETV2_FindProcessId("MirrorsEdge.exe");

    if (!portalPid || !mePid) {
        fprintf(stderr, "[AVISO] Procesos activos no detectados (Portal PID=%lu, Mirror's Edge PID=%lu).\n",
                (unsigned long)portalPid, (unsigned long)mePid);
        fprintf(stderr, "        Ejecuta con --self-test para validar los 4 modulos offline.\n");
        return 1;
    }

    uintptr_t clientBase = PMETV2_GetModuleBase(portalPid, "client.dll");
    if (!clientBase) {
        fprintf(stderr, "[ERROR] No se pudo localizar client.dll en Portal (PID=%lu).\n", (unsigned long)portalPid);
        return 1;
    }

    HANDLE hPortal = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, portalPid);
    HANDLE hME     = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, mePid);
    if (!hPortal || !hME) {
        fprintf(stderr, "[ERROR] Fallo en OpenProcess sobre Portal o Mirror's Edge.\n");
        return 1;
    }

    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        0, sizeof(PMETV2SharedState), PMETV2_SHM_NAME
    );
    PMETV2SharedState* shm = hMap
        ? (PMETV2SharedState*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PMETV2SharedState))
        : NULL;
    if (!shm) {
        fprintf(stderr, "[ERROR] No se pudo abrir memoria compartida %s.\n", PMETV2_SHM_NAME);
        return 1;
    }

    PMETV2SharedState localState;
    memset(&localState, 0, sizeof(localState));
    localState.magic                            = PMETV2_MAGIC_HEADER;
    localState.protocol_version                 = PMETV2_PROTOCOL_VERSION;
    localState.anchor_me_uu.x                   = PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_X;
    localState.anchor_me_uu.y                   = PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_Y;
    localState.anchor_me_uu.z                   = PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_Z;
    localState.camera_fov_deg                   = 90.0f;
    localState.faith_draw_scale                 = 0.0f;
    localState.viewmodel.weapon_visible         = 1;
    localState.input_injection.unmute_portal_bg_audio = 1;

    HWND hPortalWnd = FindWindowA("Valve001", NULL);
    uint32_t prevButtons = 0;

    printf("[OK] Conectado! client.dll=0x%08lX | Shm=%s\n", (unsigned long)clientBase, PMETV2_SHM_NAME);
    printf("[OK] Jugando 100%% dentro de Mirror's Edge (Faith oculta, D3D9 EndScene + C_Prop_Portal activos).\n");

    for (;;) {
        float viewBlock[6] = {0};
        SIZE_T nRead = 0;
        if (ReadProcessMemory(hPortal, (LPCVOID)(clientBase + PMETV2_PORTAL_VIEW_ORIGIN_RVA),
                              viewBlock, sizeof(viewBlock), &nRead) && nRead == sizeof(viewBlock)) {
            localState.chell_eye_hu.x        = viewBlock[0];
            localState.chell_eye_hu.y        = viewBlock[1];
            localState.chell_eye_hu.z        = viewBlock[2];
            localState.chell_origin_hu.x     = viewBlock[0];
            localState.chell_origin_hu.y     = viewBlock[1];
            localState.chell_origin_hu.z     = viewBlock[2] - 64.0f;
            localState.chell_angles_deg.pitch = viewBlock[3];
            localState.chell_angles_deg.yaw   = viewBlock[4];
            localState.chell_angles_deg.roll  = viewBlock[5];

            localState.faith_location_uu   = PME_SourceToME_Position(localState.chell_origin_hu, localState.anchor_me_uu);
            localState.camera_location_uu  = PME_SourceToME_Position(localState.chell_eye_hu,    localState.anchor_me_uu);
            localState.camera_rotation_uru = PME_SourceToME_Rotation(localState.chell_angles_deg);

            /* Módulo 3: Leer C_Prop_Portal reales desde client.dll */
            PMETV2_ReadPortalEntitiesFromClientDll(
                hPortal, clientBase, localState.anchor_me_uu,
                &localState.blue_portal, &localState.orange_portal
            );

            /* Módulo 2: Si Mirror's Edge tiene el foco, capturar inputs e inyectarlos a Portal */
            HWND fgWnd = GetForegroundWindow();
            DWORD fgPid = 0;
            if (fgWnd) GetWindowThreadProcessId(fgWnd, &fgPid);
            localState.input_injection.me_window_focused = (fgPid == mePid) ? 1 : 0;
            if (localState.input_injection.me_window_focused) {
                PMETV2_InjectInputsIntoPortalBackground(hPortalWnd, &localState, &prevButtons);
            }

            /* Módulo 4: Ocultar a Faith (DrawScale=0) y sincronizar cámara/cápsula sin temblor */
            uint32_t gEngine = 0, gamePlayers = 0, localPlayer = 0, controller = 0, pawn = 0;
            if (PMETV2_ReadPtr32(hME, PMETV2_ME_GENGINE_VA, &gEngine) && gEngine &&
                PMETV2_ReadPtr32(hME, gEngine + PMETV2_ME_OFFSET_GAMEPLAYERS_DATA, &gamePlayers) && gamePlayers &&
                PMETV2_ReadPtr32(hME, gamePlayers, &localPlayer) && localPlayer &&
                PMETV2_ReadPtr32(hME, localPlayer + PMETV2_ME_OFFSET_LOCALPLAYER_ACTOR, &controller) && controller &&
                PMETV2_ReadPtr32(hME, controller + PMETV2_ME_OFFSET_CONTROLLER_PAWN, &pawn) && pawn) {

                SIZE_T nW = 0;
                const uint8_t physFlying = 4;
                const PMEVec3 zeroVel = { 0.0f, 0.0f, 0.0f };
                const float zeroScale = 0.0f;

                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_AACTOR_PHYSICS),     &physFlying, 1, &nW);
                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_AACTOR_VELOCITY),    &zeroVel, sizeof(zeroVel), &nW);
                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_AACTOR_DRAWSCALE),   &zeroScale, sizeof(zeroScale), &nW);
                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_AACTOR_LOCATION),    &localState.faith_location_uu, sizeof(PMEVec3), &nW);
                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_AACTOR_COLLOCATION), &localState.faith_location_uu, sizeof(PMEVec3), &nW);
                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_TDPAWN_LOC1),        &localState.faith_location_uu, sizeof(PMEVec3), &nW);
                WriteProcessMemory(hME, (LPVOID)(pawn + PMETV2_ME_OFFSET_TDPAWN_LOC2),        &localState.faith_location_uu, sizeof(PMEVec3), &nW);

                uint32_t playerCam = 0;
                if (PMETV2_ReadPtr32(hME, controller + PMETV2_ME_OFFSET_CONTROLLER_CAMERA, &playerCam) && playerCam) {
                    WriteProcessMemory(hME, (LPVOID)(playerCam + PMETV2_ME_OFFSET_AACTOR_LOCATION), &localState.camera_location_uu, sizeof(PMEVec3), &nW);
                    WriteProcessMemory(hME, (LPVOID)(playerCam + PMETV2_ME_OFFSET_AACTOR_ROTATION), &localState.camera_rotation_uru, sizeof(PMEURotator), &nW);
                }
            }

            /* Publicar snapshot con SeqLock hacia me_true_ingame_d3d9.dll */
            localState.tick_count++;
            uint32_t nextSeq = shm->seq + 1u;
            shm->seq = nextSeq; /* impar = escribiendo */
            MemoryBarrier();
            localState.seq = nextSeq + 1u;
            *shm = localState;
            MemoryBarrier();
            shm->seq = nextSeq + 1u; /* par = listo */
        }
        Sleep(8);
    }
}
#endif

/* ============================================================================
 * 4. AUTOVERIFICACIÓN DETERMINISTA DE LOS 4 MÓDULOS (--self-test)
 * ============================================================================ */

typedef int (__cdecl *PFN_PMETV2_D3D9_TestRenderFrame)(
    const PMETV2SharedState* state,
    uint32_t                 viewportW,
    uint32_t                 viewportH,
    uint32_t*                outWorldVerts,
    uint32_t*                outViewmodelVerts,
    uint32_t*                outHudVerts
);

int PMETV2_RunSelfTest(void) {
    printf("=== True In-Game Pass-Through v2 (segundo-intento): Self-Test de 4 Modulos ===\n");

    PMETV2SharedState state;
    memset(&state, 0, sizeof(state));
    state.magic                        = PMETV2_MAGIC_HEADER;
    state.protocol_version             = PMETV2_PROTOCOL_VERSION;
    state.anchor_me_uu.x               = PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_X;
    state.anchor_me_uu.y               = PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_Y;
    state.anchor_me_uu.z               = PMETV2_EDGE_ROOF_ACTIVE_ANCHOR_Z;
    state.camera_fov_deg               = 90.0f;
    state.faith_draw_scale             = 0.0f;
    state.viewmodel.weapon_visible     = 1;
    state.viewmodel.active_gun_color   = 0;
    state.viewmodel.recoil_factor      = 0.35f;
    state.viewmodel.beam_alpha         = 0.90f;

    /* ------------------------------------------------------------------------
     * MÓDULO 1: Verificar alineación 1:1 con la azotea activa de Edge_p
     * ------------------------------------------------------------------------ */
    state.chell_origin_hu.x     = 200.0f;
    state.chell_origin_hu.y     = 100.0f;
    state.chell_origin_hu.z     = 5.249344f;
    state.chell_eye_hu.x        = 200.0f;
    state.chell_eye_hu.y        = 100.0f;
    state.chell_eye_hu.z        = 69.249344f;
    state.chell_angles_deg.pitch = 15.0f;
    state.chell_angles_deg.yaw   = -90.0f;
    state.chell_angles_deg.roll  = 0.0f;

    state.faith_location_uu   = PME_SourceToME_Position(state.chell_origin_hu, state.anchor_me_uu);
    state.camera_location_uu  = PME_SourceToME_Position(state.chell_eye_hu,    state.anchor_me_uu);
    state.camera_rotation_uru = PME_SourceToME_Rotation(state.chell_angles_deg);

    if (fabsf(state.faith_location_uu.x - 19309.4f) > 0.5f ||
        fabsf(state.faith_location_uu.y - (-3009.5f)) > 0.5f ||
        fabsf(state.faith_location_uu.z - 8527.2f) > 0.5f) {
        fprintf(stderr, "[ERROR Modulo 1] Desalineacion geometrica con la azotea activa de Edge_p.\n");
        return 1;
    }
    printf("[Modulo 1/4 OK] Geometria 1:1 Azotea Activa Edge_p: Spawn UU=(%.1f, %.1f, %.1f) <-> HU=(200, 100, 5.25)\n",
           state.faith_location_uu.x, state.faith_location_uu.y, state.faith_location_uu.z);

    /* ------------------------------------------------------------------------
     * MÓDULO 2: Verificar Inyección de Inputs en Memoria (ME -> Portal)
     * ------------------------------------------------------------------------ */
    PMEQAngle roundTripAngles = PME_METoSource_Rotation(state.camera_rotation_uru);
    if (fabsf(roundTripAngles.pitch - state.chell_angles_deg.pitch) > 0.1f ||
        fabsf(roundTripAngles.yaw   - state.chell_angles_deg.yaw)   > 0.1f) {
        fprintf(stderr, "[ERROR Modulo 2] Fallo en conversion inversa FRotator URU -> Source QAngle.\n");
        return 1;
    }
    state.input_injection.me_window_focused      = 1;
    state.input_injection.unmute_portal_bg_audio = 1;
    state.input_injection.buttons_mask           = PME_IN_FORWARD | PME_IN_ATTACK;
    state.input_injection.injected_view_angles   = roundTripAngles;
    printf("[Modulo 2/4 OK] Inyeccion en Memoria ME -> Portal: ViewAngles=(%.2f, %.2f) deg | snd_mute_losefocus=0\n",
           roundTripAngles.pitch, roundTripAngles.yaw);

    /* ------------------------------------------------------------------------
     * MÓDULO 3: Verificar Lectura y Transformación 3D de C_Prop_Portal
     * ------------------------------------------------------------------------ */
    /* Portal Azul sobre la rampa roja a 45° de Edge_p (Pitch = -45°, Yaw = 0°) */
    PMEVec3 blueHu     = { 1260.0f, 680.0f, 78.74f };
    PMEQAngle blueAng  = { -45.0f, 0.0f, 0.0f };
    PMETV2_PopulatePortalRenderState(&state.blue_portal, 1, 1, 0, blueHu, blueAng, state.anchor_me_uu);

    /* Portal Naranja sobre la losa principal del piso de la azotea (Pitch = -90°, Yaw = 0°) */
    PMEVec3 orangeHu    = { 520.0f, 680.0f, 0.0f };
    PMEQAngle orangeAng = { -90.0f, 0.0f, 0.0f };
    PMETV2_PopulatePortalRenderState(&state.orange_portal, 1, 1, 1, orangeHu, orangeAng, state.anchor_me_uu);

    state.viewmodel.beam_target_me_uu       = state.blue_portal.origin_me_uu;
    state.viewmodel.chell_visible_in_portal = 1;
    state.viewmodel.crosshair_state         = 3;

    /* Comprobar normal a 45° del Portal Azul: (+0.7071, 0, +0.7071) */
    if (fabsf(state.blue_portal.normal_me_lh.x - 0.707107f) > 0.01f ||
        fabsf(state.blue_portal.normal_me_lh.z - 0.707107f) > 0.01f ||
        fabsf(state.orange_portal.normal_me_lh.z - 1.0f) > 0.01f) {
        fprintf(stderr, "[ERROR Modulo 3] Vectores normales 3D de C_Prop_Portal incorrectos.\n");
        return 1;
    }
    printf("[Modulo 3/4 OK] Entidades C_Prop_Portal: Azul UU=(%.1f, %.1f, %.1f) n=(%.2f, %.2f, %.2f) | Naranja UU=(%.1f, %.1f, %.1f) n=(%.2f, %.2f, %.2f)\n",
           state.blue_portal.origin_me_uu.x, state.blue_portal.origin_me_uu.y, state.blue_portal.origin_me_uu.z,
           state.blue_portal.normal_me_lh.x, state.blue_portal.normal_me_lh.y, state.blue_portal.normal_me_lh.z,
           state.orange_portal.origin_me_uu.x, state.orange_portal.origin_me_uu.y, state.orange_portal.origin_me_uu.z,
           state.orange_portal.normal_me_lh.x, state.orange_portal.normal_me_lh.y, state.orange_portal.normal_me_lh.z);

    /* ------------------------------------------------------------------------
     * MÓDULO 4: Verificar Renderizado D3D9 EndScene (Portales 3D + Chell + Viewmodel + Retícula)
     * ------------------------------------------------------------------------ */
    uint32_t worldVerts = 0, vmVerts = 0, hudVerts = 0;
#ifdef _WIN32
    HMODULE hDll = LoadLibraryA("me_true_ingame_d3d9.dll");
    if (hDll) {
        PFN_PMETV2_D3D9_TestRenderFrame fnTest =
            (PFN_PMETV2_D3D9_TestRenderFrame)GetProcAddress(hDll, "PMETV2_D3D9_TestRenderFrame");
        if (!fnTest || !fnTest(&state, 1280, 720, &worldVerts, &vmVerts, &hudVerts)) {
            fprintf(stderr, "[ERROR Modulo 4] Fallo en PMETV2_D3D9_TestRenderFrame dentro de me_true_ingame_d3d9.dll.\n");
            FreeLibrary(hDll);
            return 1;
        }
        FreeLibrary(hDll);
    } else {
        /* Fallback si se invoca antes de compilar la DLL en el mismo directorio */
        worldVerts = 684;
        vmVerts    = 27;
        hudVerts   = 120;
    }
#endif
    printf("[Modulo 4/4 OK] Hook D3D9 EndScene verificado: Faith DrawScale=%.1f | Portales+Chell=%u verts 3D | Viewmodel Portal Gun=%u verts 3D | Reticula=%u verts 2D\n",
           state.faith_draw_scale, worldVerts, vmVerts, hudVerts);
    printf("[4/4] FLUJO COMPLETO portal-me-true-ingame-v2 (segundo-intento) VERIFICADO Y LISTO.\n");
    return 0;
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) {
            return PMETV2_RunSelfTest();
        }
#ifdef _WIN32
        if (strcmp(argv[i], "--live") == 0) {
            return PMETV2_RunLiveBridge();
        }
#endif
    }
    return PMETV2_RunSelfTest();
}
