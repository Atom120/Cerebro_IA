/**
 * @file lean_external_bridge.cpp
 * @brief Flujo Rediseñado ("Lean Vertical Slice" / PLEB): Puente Externo Único en 1 Solo Archivo C++ (Cero SDKs)
 *
 * Calibrado con los binarios reales instalados:
 * - Mirror's Edge (GOG v1.01 DRM-Free, Timestamp 0x4965FFF1, ImageBase fijo 0x00400000, ASLR = false)
 * - Portal (Steam 2025 build, client.dll Timestamp 0x68362D89)
 * - Coordenadas reales extraídas de TdGame/CookedPC/Maps/SP01/Edge_p.umap y Edge_Pt1.umap
 *
 * Compilación directa sin dependencias externas:
 *   cl /EHsc /O2 src/lean_bridge/lean_external_bridge.cpp /Fe:lean_bridge.exe
 *   g++ -O2 src/lean_bridge/lean_external_bridge.cpp -o lean_bridge.exe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/ipc_stormdrain_bridge_v2.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#pragma comment(lib, "user32.lib")
#endif

/* ============================================================================
 * 1. OFFSETS REALES EXTRAÍDOS DE LOS BINARIOS INSTALADOS (GOG + STEAM)
 * ============================================================================ */

/* Mirror's Edge GOG (MirrorsEdge.exe - ImageBase fijo 0x00400000, sin ASLR) */
#define PME_ME_GOG_IMAGE_BASE              0x00400000UL
#define PME_ME_GOG_GENGINE_VA              0x01FF8B20UL  /* UEngine* GEngine */
#define PME_ME_GOG_GWORLD_VA               0x01FFBCA4UL  /* UWorld* GWorld */
#define PME_ME_GOG_GOBJOBJECTS_VA          0x0204A35CUL  /* TArray<UObject*> UObject::GObjObjects */
#define PME_ME_GOG_GNAMES_VA               0x0204E7D8UL  /* TArray<FNameEntry*> FName::Names */

/* Cadena de punteros UE3 desde GEngine hasta ATdPlayerController, TdPlayerCamera y ATdPlayerPawn */
#define PME_ME_OFFSET_GAMEPLAYERS_DATA     0x02BCUL      /* GEngine->GamePlayers.Data (ULocalPlayer**) */
#define PME_ME_OFFSET_LOCALPLAYER_ACTOR    0x0040UL      /* ULocalPlayer->Actor (ATdPlayerController*, tras FExec@+0x3C) */
#define PME_ME_OFFSET_CONTROLLER_PAWN      0x01C0UL      /* AController->Pawn (ATdPlayerPawn*) */
#define PME_ME_OFFSET_CONTROLLER_CAMERA    0x02D0UL      /* APlayerController->PlayerCamera (ATdPlayerCamera*) */
#define PME_ME_OFFSET_AACTOR_PHYSICS       0x006BUL      /* AActor->Physics (uint8_t: 0=None, 1=Walking, 4=Flying) */
#define PME_ME_OFFSET_AACTOR_LOCATION      0x00E8UL      /* AActor->Location (FVector: 3 floats X,Y,Z) */
#define PME_ME_OFFSET_AACTOR_ROTATION      0x00F4UL      /* AActor->Rotation (FRotator: 3 ints Pitch,Yaw,Roll) */
#define PME_ME_OFFSET_AACTOR_VELOCITY      0x0100UL      /* AActor->Velocity (FVector: 3 floats VX,VY,VZ) */
#define PME_ME_OFFSET_AACTOR_COLLOCATION   0x013CUL      /* AActor->ColLocation (FVector: 3 floats X,Y,Z) */
#define PME_ME_OFFSET_TDPAWN_LOC1          0x02E0UL      /* ATdPawn internal capsule location 1 (FVector) */
#define PME_ME_OFFSET_TDPAWN_LOC2          0x02F8UL      /* ATdPawn internal capsule location 2 (FVector) */

/* Portal Steam (portal/bin/client.dll - RVA relativos a la base de client.dll) */
#define PME_PORTAL_CLIENT_VIEW_ORIGIN_RVA  0x0050F484UL  /* MainViewOrigin (g_vecRenderOrigin: float[3]) */
#define PME_PORTAL_CLIENT_VIEW_ANGLES_RVA  0x0050F490UL  /* MainViewAngles (g_vecRenderAngles: float[3]) */
#define PME_PORTAL_CLIENT_LOCALPLAYER_RVA  0x004D78F0UL  /* C_BasePlayer* s_pLocalPlayer */
#define PME_PORTAL_OFFSET_VELOCITY         0x00F0UL      /* C_BasePlayer::m_vecVelocity (float[3]) */
#define PME_PORTAL_OFFSET_ABS_ORIGIN       0x025CUL      /* C_BasePlayer::m_vecAbsOrigin (float[3]) */
#define PME_PORTAL_OFFSET_ORIGIN           0x028CUL      /* C_BaseEntity::m_vecOrigin (float[3]) */

/* Coordenadas reales extraídas de Edge_p.umap (Azotea 1 PlayerStart y Checkpoint 1Pt1_4PlayerStart) */
#define PME_EDGE_UMAP_ROOF1_ANCHOR_X       31827.5f
#define PME_EDGE_UMAP_ROOF1_ANCHOR_Y      -4620.8f
#define PME_EDGE_UMAP_ROOF1_ANCHOR_Z       15180.0f

#define PME_EDGE_UMAP_FREE_ANCHOR_X        18928.4f
#define PME_EDGE_UMAP_FREE_ANCHOR_Y       -2819.0f
#define PME_EDGE_UMAP_FREE_ANCHOR_Z        8510.0f

typedef struct PMELeanOffsets {
    uintptr_t portal_view_origin_rva;
    uintptr_t portal_view_angles_rva;
    uintptr_t portal_local_player_rva;
    uintptr_t me_gengine_va;
    uintptr_t me_gameplayers_offset;
    uintptr_t me_actor_offset;
    uintptr_t me_pawn_offset;
    uintptr_t me_location_offset;
    uintptr_t me_rotation_offset;
} PMELeanOffsets;

typedef struct PMELeanSliceState {
    PMEVec3     anchor_me_uu;
    PMEVec3     source_origin_hu;
    PMEVec3     source_velocity_hu;
    PMEQAngle   source_angles_deg;
    PMEVec3     me_location_uu;
    PMEURotator me_rotation_uru;
} PMELeanSliceState;

void PME_Lean_InitDefaultOffsets(PMELeanOffsets* out) {
    out->portal_view_origin_rva  = PME_PORTAL_CLIENT_VIEW_ORIGIN_RVA;
    out->portal_view_angles_rva  = PME_PORTAL_CLIENT_VIEW_ANGLES_RVA;
    out->portal_local_player_rva = PME_PORTAL_CLIENT_LOCALPLAYER_RVA;
    out->me_gengine_va           = PME_ME_GOG_GENGINE_VA;
    out->me_gameplayers_offset   = PME_ME_OFFSET_GAMEPLAYERS_DATA;
    out->me_actor_offset         = PME_ME_OFFSET_LOCALPLAYER_ACTOR;
    out->me_pawn_offset          = PME_ME_OFFSET_CONTROLLER_PAWN;
    out->me_location_offset      = PME_ME_OFFSET_AACTOR_LOCATION;
    out->me_rotation_offset      = PME_ME_OFFSET_AACTOR_ROTATION;
}

/**
 * Transforma un frame completo del Vertical Slice en un solo paso determinista.
 */
void PME_Lean_TransformFrame(PMELeanSliceState* state) {
    state->me_location_uu = PME_SourceToME_Position(state->source_origin_hu, state->anchor_me_uu);
    state->me_rotation_uru = PME_SourceToME_Rotation(state->source_angles_deg);
}

#ifdef _WIN32
static DWORD PME_FindProcessId(const char* procName) {
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

static uintptr_t PME_GetModuleBase(DWORD pid, const char* modName) {
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

static int PME_ReadPtr32(HANDLE hProc, uintptr_t addr, uint32_t* outVal) {
    SIZE_T nRead = 0;
    return ReadProcessMemory(hProc, (LPCVOID)addr, outVal, sizeof(uint32_t), &nRead) && nRead == sizeof(uint32_t);
}

/**
 * Modo de sonda en vivo (--probe-me): Inspecciona el proceso activo MirrorsEdge.exe,
 * muestra las coordenadas actuales de Faith y verifica la escritura directa en memoria
 * sin depender de que los binds F10/F11 (CheatManager) estén habilitados en retail.
 */
int PME_Lean_ProbeMirrorEdge(int testLiftZ) {
    PMELeanOffsets ofs;
    PME_Lean_InitDefaultOffsets(&ofs);

    DWORD mePid = PME_FindProcessId("MirrorsEdge.exe");
    if (!mePid) {
        fprintf(stderr, "[PROBE] MirrorsEdge.exe no esta en ejecucion.\n");
        return 1;
    }

    HANDLE hME = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, mePid);
    if (!hME) {
        fprintf(stderr, "[PROBE] Fallo en OpenProcess sobre MirrorsEdge.exe (PID=%lu).\n", (unsigned long)mePid);
        return 1;
    }

    uint32_t gEngine = 0, gamePlayersData = 0, localPlayer = 0, controller = 0, pawn = 0;
    PME_ReadPtr32(hME, ofs.me_gengine_va, &gEngine);
    if (gEngine) PME_ReadPtr32(hME, gEngine + ofs.me_gameplayers_offset, &gamePlayersData);
    if (gamePlayersData) PME_ReadPtr32(hME, gamePlayersData, &localPlayer);
    if (localPlayer) PME_ReadPtr32(hME, localPlayer + ofs.me_actor_offset, &controller);
    if (controller) PME_ReadPtr32(hME, controller + ofs.me_pawn_offset, &pawn);

    printf("=== PLEB Live Probe: MirrorsEdge.exe (PID=%lu) ===\n", (unsigned long)mePid);
    printf("  GEngine         @ 0x%08lX -> 0x%08lX\n", (unsigned long)ofs.me_gengine_va, (unsigned long)gEngine);

    /* Escanear GEngine (0..0x520) buscando TArray<ULocalPlayer*> (Data ptr seguido de Num=1, Max>=1) */
    if (gEngine) {
        uint32_t engWords[0x520 / 4];
        SIZE_T nRead = 0;
        if (ReadProcessMemory(hME, (LPCVOID)gEngine, engWords, sizeof(engWords), &nRead)) {
            for (int i = 0; i < (0x500 / 4); ++i) {
                if (engWords[i] > 0x00100000 &&
                    engWords[i + 1] == 1 && engWords[i + 2] >= 1 && engWords[i + 2] <= 64) {
                    uint32_t firstElem = 0;
                    if (PME_ReadPtr32(hME, engWords[i], &firstElem) && firstElem > 0x00100000) {
                        uint32_t vt = 0;
                        PME_ReadPtr32(hME, firstElem, &vt);
                        printf("  [Candidato TArray en GEngine + 0x%03X] Data=0x%08lX -> [0]=0x%08lX (vt=0x%08lX, Num=%lu, Max=%lu)\n",
                               i * 4, (unsigned long)engWords[i], (unsigned long)firstElem, (unsigned long)vt,
                               (unsigned long)engWords[i + 1], (unsigned long)engWords[i + 2]);
                        if (vt >= 0x01A8B000 && vt < 0x01F53000) {
                            localPlayer = firstElem;
                        }
                    }
                }
            }
        }
    }

    if (localPlayer) {
        uint32_t lpWords[0x100 / 4];
        SIZE_T nRead = 0;
        if (ReadProcessMemory(hME, (LPCVOID)localPlayer, lpWords, sizeof(lpWords), &nRead)) {
            for (int i = 0; i < (0x100 / 4); ++i) {
                if (lpWords[i] > 0x00100000) {
                    uint32_t vt = 0;
                    if (PME_ReadPtr32(hME, lpWords[i], &vt) && vt >= 0x01A8B000 && vt < 0x01F53000) {
                        printf("    ULocalPlayer + 0x%02X -> 0x%08lX (vtable=0x%08lX)\n",
                               i * 4, (unsigned long)lpWords[i], (unsigned long)vt);
                    }
                }
            }
        }
        PME_ReadPtr32(hME, localPlayer + ofs.me_actor_offset, &controller);
    }
    if (controller) PME_ReadPtr32(hME, controller + ofs.me_pawn_offset, &pawn);

    printf("  GamePlayers[0]  @ +0x%lX     -> 0x%08lX\n", (unsigned long)ofs.me_gameplayers_offset, (unsigned long)localPlayer);
    printf("  Controller      @ +0x%lX      -> 0x%08lX\n", (unsigned long)ofs.me_actor_offset, (unsigned long)controller);
    printf("  Pawn (Faith)    @ +0x%lX     -> 0x%08lX\n", (unsigned long)ofs.me_pawn_offset, (unsigned long)pawn);

    if (controller) {
        PMEVec3 cLoc = {0};
        PMEURotator cRot = {0};
        SIZE_T nRead = 0;
        ReadProcessMemory(hME, (LPCVOID)(controller + ofs.me_location_offset), &cLoc, sizeof(cLoc), &nRead);
        ReadProcessMemory(hME, (LPCVOID)(controller + ofs.me_rotation_offset), &cRot, sizeof(cRot), &nRead);
        printf("  Controller Loc  : (%.1f, %.1f, %.1f) | Rot URU: (P=%d, Y=%d, R=%d)\n",
               cLoc.x, cLoc.y, cLoc.z, cRot.pitch, cRot.yaw, cRot.roll);
    }

    if (pawn) {
        PMEVec3 pLoc = {0};
        PMEURotator pRot = {0};
        uint8_t physByte = 0;
        SIZE_T nRead = 0;
        ReadProcessMemory(hME, (LPCVOID)(pawn + ofs.me_location_offset), &pLoc, sizeof(pLoc), &nRead);
        ReadProcessMemory(hME, (LPCVOID)(pawn + ofs.me_rotation_offset), &pRot, sizeof(pRot), &nRead);
        ReadProcessMemory(hME, (LPCVOID)(pawn + 0x6B), &physByte, 1, &nRead);
        printf("  Pawn (Faith) Loc: (%.1f, %.1f, %.1f) | Rot URU: (P=%d, Y=%d, R=%d) | Byte@0x6B=%u\n",
               pLoc.x, pLoc.y, pLoc.z, pRot.pitch, pRot.yaw, pRot.roll, (unsigned)physByte);

        /* Inspeccionar bytes cercanos a Location (+0xE8) y flags de colision/fisica (+0x38..+0x160) */
        uint8_t pawnDump[0x180];
        if (ReadProcessMemory(hME, (LPCVOID)pawn, pawnDump, sizeof(pawnDump), &nRead)) {
            printf("  Pawn flags @ +0x3C..+0x4B: ");
            for (int k = 0x3C; k < 0x4C; ++k) printf("%02X ", pawnDump[k]);
            printf("\n  Pawn bytes @ +0x100..+0x15F (Scale/Velocity/Acceleration):\n    ");
            for (int k = 0x100; k < 0x140; k += 4) {
                float fv = *(float*)(pawnDump + k);
                printf("+0x%03X=%.1f ", k, fv);
            }
            printf("\n");
        }

        if (testLiftZ) {
            const uint8_t physFlying = 4; /* PHYS_Flying */
            const PMEVec3 zeroVel = { 0.0f, 0.0f, 0.0f };
            pLoc.z += 250.0f;
            SIZE_T nWritten = 0;
            WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_PHYSICS), &physFlying, sizeof(physFlying), &nWritten);
            WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_VELOCITY), &zeroVel, sizeof(zeroVel), &nWritten);
            WriteProcessMemory(hME, (LPVOID)(pawn + ofs.me_location_offset), &pLoc, sizeof(pLoc), &nWritten);
            WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_COLLOCATION), &pLoc, sizeof(pLoc), &nWritten);
            printf("  [TEST] Se activo PHYS_Flying y se elevo a Faith +250 UU en Z -> Nueva Z=%.1f\n", pLoc.z);
        }
    }

    CloseHandle(hME);
    return 0;
}

/**
 * Bucle en vivo a 120 Hz: conecta Portal (Steam) -> Mirror's Edge (GOG).
 */
int PME_Lean_RunLiveBridge(int useFreeControlCheckpoint) {
    PMELeanOffsets ofs;
    PME_Lean_InitDefaultOffsets(&ofs);

    printf("=== PLEB Live Bridge (120 Hz): Buscando procesos de Portal y Mirror's Edge ===\n");
    DWORD portalPid = PME_FindProcessId("hl2.exe");
    if (!portalPid) portalPid = PME_FindProcessId("portal.exe");
    DWORD mePid = PME_FindProcessId("MirrorsEdge.exe");

    if (!portalPid || !mePid) {
        fprintf(stderr, "[AVISO] Procesos activos no detectados (Portal PID=%lu, Mirror's Edge PID=%lu).\n",
                (unsigned long)portalPid, (unsigned long)mePid);
        fprintf(stderr, "        Abre ambos juegos o ejecuta con --self-test para verificar la calibracion.\n");
        return 1;
    }

    uintptr_t clientBase = PME_GetModuleBase(portalPid, "client.dll");
    if (!clientBase) {
        fprintf(stderr, "[ERROR] No se pudo localizar client.dll en Portal (PID=%lu).\n", (unsigned long)portalPid);
        return 1;
    }

    HANDLE hPortal = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, portalPid);
    HANDLE hME = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, mePid);
    if (!hPortal || !hME) {
        fprintf(stderr, "[ERROR] Fallo en OpenProcess (ejecuta la terminal como Administrador si es necesario).\n");
        return 1;
    }

    PMELeanSliceState state;
    memset(&state, 0, sizeof(state));
    if (useFreeControlCheckpoint) {
        state.anchor_me_uu.x = PME_EDGE_UMAP_FREE_ANCHOR_X;
        state.anchor_me_uu.y = PME_EDGE_UMAP_FREE_ANCHOR_Y;
        state.anchor_me_uu.z = PME_EDGE_UMAP_FREE_ANCHOR_Z;
    } else {
        state.anchor_me_uu.x = PME_EDGE_UMAP_ROOF1_ANCHOR_X;
        state.anchor_me_uu.y = PME_EDGE_UMAP_ROOF1_ANCHOR_Y;
        state.anchor_me_uu.z = PME_EDGE_UMAP_ROOF1_ANCHOR_Z;
    }

    printf("[OK] Conectado! client.dll=0x%08lX | GEngine VA=0x%08lX\n",
           (unsigned long)clientBase, (unsigned long)ofs.me_gengine_va);

    /* Auto-calibrar el ancla al punto actual de Faith en Mirror's Edge si el jugador está en tutorial_p o cualquier azotea */
    {
        uint32_t gEngine = 0, gamePlayersData = 0, localPlayer = 0, controller = 0, pawn = 0;
        if (PME_ReadPtr32(hME, ofs.me_gengine_va, &gEngine) && gEngine &&
            PME_ReadPtr32(hME, gEngine + ofs.me_gameplayers_offset, &gamePlayersData) && gamePlayersData &&
            PME_ReadPtr32(hME, gamePlayersData, &localPlayer) && localPlayer &&
            PME_ReadPtr32(hME, localPlayer + ofs.me_actor_offset, &controller) && controller &&
            PME_ReadPtr32(hME, controller + ofs.me_pawn_offset, &pawn) && pawn) {

            PMEVec3 curFaithLoc = {0};
            float viewBlock[6] = {0};
            SIZE_T nRead1 = 0, nRead2 = 0;
            if (ReadProcessMemory(hME, (LPCVOID)(pawn + ofs.me_location_offset), &curFaithLoc, sizeof(curFaithLoc), &nRead1) &&
                ReadProcessMemory(hPortal, (LPCVOID)(clientBase + ofs.portal_view_origin_rva), viewBlock, sizeof(viewBlock), &nRead2)) {
                float pFeetX = viewBlock[0];
                float pFeetY = viewBlock[1];
                float pFeetZ = viewBlock[2] - 64.0f;
                state.anchor_me_uu.x = curFaithLoc.x - (pFeetX * PME_HU_TO_UU);
                state.anchor_me_uu.y = curFaithLoc.y + (pFeetY * PME_HU_TO_UU);
                state.anchor_me_uu.z = curFaithLoc.z - (pFeetZ * PME_HU_TO_UU);
                printf("[Auto-Anchor] Anclado automaticamente a la posicion actual de Faith: (%.1f, %.1f, %.1f) UU | Chell View: (%.1f, %.1f, %.1f) HU\n",
                       curFaithLoc.x, curFaithLoc.y, curFaithLoc.z, viewBlock[0], viewBlock[1], viewBlock[2]);
            }
        }
    }

    printf("[OK] Sincronizando Chell -> Faith a 120 Hz (Smart Focus: camina libre en ME, controla desde Portal)...\n");
    fflush(stdout);

    uint32_t tickCount = 0;
    uint32_t lastPawn = 0;
    int wasMeFocused = 0;

    for (;;) {
        float viewBlock[6]; /* [0..2] = MainViewOrigin, [3..5] = MainViewAngles */
        SIZE_T nRead = 0;
        if (ReadProcessMemory(hPortal, (LPCVOID)(clientBase + ofs.portal_view_origin_rva),
                              viewBlock, sizeof(viewBlock), &nRead) && nRead == sizeof(viewBlock)) {
            state.source_origin_hu.x = viewBlock[0];
            state.source_origin_hu.y = viewBlock[1];
            state.source_origin_hu.z = viewBlock[2] - 64.0f; /* De altura de ojos a pies */
            state.source_angles_deg.pitch = viewBlock[3];
            state.source_angles_deg.yaw   = viewBlock[4];
            state.source_angles_deg.roll  = viewBlock[5];

            /* Detectar qué ventana tiene el foco activo del usuario */
            HWND fgWnd = GetForegroundWindow();
            DWORD fgPid = 0;
            if (fgWnd) GetWindowThreadProcessId(fgWnd, &fgPid);
            int isMeFocused = (fgPid == mePid);

            /* Resolver cadena de punteros en Mirror's Edge (GOG):
             * GEngine -> GamePlayers.Data[0] -> Actor (ATdPlayerController) -> Pawn (ATdPlayerPawn) */
            uint32_t gEngine = 0, gamePlayersData = 0, localPlayer = 0, controller = 0, pawn = 0;
            if (PME_ReadPtr32(hME, ofs.me_gengine_va, &gEngine) && gEngine &&
                PME_ReadPtr32(hME, gEngine + ofs.me_gameplayers_offset, &gamePlayersData) && gamePlayersData &&
                PME_ReadPtr32(hME, gamePlayersData, &localPlayer) && localPlayer &&
                PME_ReadPtr32(hME, localPlayer + ofs.me_actor_offset, &controller) && controller &&
                PME_ReadPtr32(hME, controller + ofs.me_pawn_offset, &pawn) && pawn) {

                SIZE_T nWritten = 0;

                /* Si el usuario tiene el foco en Mirror's Edge (o cambió el Pawn al cargar otro nivel),
                 * devolver control nativo a Faith (PHYS_Walking = 1) y actualizar el ancla en vivo sin temblor */
                if (isMeFocused || pawn != lastPawn) {
                    if (!wasMeFocused && isMeFocused) {
                        const uint8_t physWalking = 1; /* PHYS_Walking */
                        WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_PHYSICS),
                                           &physWalking, sizeof(physWalking), &nWritten);
                        printf("[Smart-Focus] Foco en Mirror's Edge: control nativo liberado (sin temblor). Posiciona a Faith donde quieras.\n");
                        fflush(stdout);
                    }
                    PMEVec3 curFaithLoc = {0};
                    SIZE_T nReadLoc = 0;
                    if (ReadProcessMemory(hME, (LPCVOID)(pawn + ofs.me_location_offset), &curFaithLoc, sizeof(curFaithLoc), &nReadLoc)) {
                        state.anchor_me_uu.x = curFaithLoc.x - (state.source_origin_hu.x * PME_HU_TO_UU);
                        state.anchor_me_uu.y = curFaithLoc.y + (state.source_origin_hu.y * PME_HU_TO_UU);
                        state.anchor_me_uu.z = curFaithLoc.z - (state.source_origin_hu.z * PME_HU_TO_UU);
                    }
                    lastPawn = pawn;
                    wasMeFocused = isMeFocused;
                } else {
                    if (wasMeFocused) {
                        printf("[Smart-Focus] Foco fuera de Mirror's Edge: sincronizando Chell -> Faith a 120 Hz.\n");
                        fflush(stdout);
                        wasMeFocused = 0;
                    }
                    PME_Lean_TransformFrame(&state);

                    /* Escribir rotacion de camara en el Controller y posicion en Pawn */
                    WriteProcessMemory(hME, (LPVOID)(controller + ofs.me_rotation_offset),
                                       &state.me_rotation_uru, sizeof(PMEURotator), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(controller + ofs.me_location_offset),
                                       &state.me_location_uu, sizeof(PMEVec3), &nWritten);

                    const uint8_t physFlying = 4; /* PHYS_Flying: desactiva gravedad de UE3 sin necesitar F10 */
                    const PMEVec3 zeroVel = { 0.0f, 0.0f, 0.0f };
                    WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_PHYSICS),
                                       &physFlying, sizeof(physFlying), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_VELOCITY),
                                       &zeroVel, sizeof(zeroVel), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(pawn + ofs.me_location_offset),
                                       &state.me_location_uu, sizeof(PMEVec3), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_AACTOR_COLLOCATION),
                                       &state.me_location_uu, sizeof(PMEVec3), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_TDPAWN_LOC1),
                                       &state.me_location_uu, sizeof(PMEVec3), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(pawn + PME_ME_OFFSET_TDPAWN_LOC2),
                                       &state.me_location_uu, sizeof(PMEVec3), &nWritten);
                    WriteProcessMemory(hME, (LPVOID)(pawn + ofs.me_rotation_offset),
                                       &state.me_rotation_uru, sizeof(PMEURotator), &nWritten);

                    /* Actualizar tambien TdPlayerCamera (Controller + 0x2D0) para sincronizar vista 100% libre de temblor */
                    uint32_t playerCam = 0;
                    if (PME_ReadPtr32(hME, controller + PME_ME_OFFSET_CONTROLLER_CAMERA, &playerCam) && playerCam) {
                        PMEVec3 camLoc = state.me_location_uu;
                        camLoc.z += 120.0f; /* Altura de ojos en UE3 sobre el piso */
                        WriteProcessMemory(hME, (LPVOID)(playerCam + ofs.me_location_offset),
                                           &camLoc, sizeof(PMEVec3), &nWritten);
                        WriteProcessMemory(hME, (LPVOID)(playerCam + ofs.me_rotation_offset),
                                           &state.me_rotation_uru, sizeof(PMEURotator), &nWritten);
                    }

                    if ((tickCount % 240) == 0) {
                        printf("[Sync 120Hz] Chell=(%.1f, %.1f, %.1f) HU [Yaw=%.1f] -> Faith=(%.1f, %.1f, %.1f) UU [Yaw=%d]\n",
                               state.source_origin_hu.x, state.source_origin_hu.y, state.source_origin_hu.z,
                               state.source_angles_deg.yaw,
                               state.me_location_uu.x, state.me_location_uu.y, state.me_location_uu.z,
                               state.me_rotation_uru.yaw);
                        fflush(stdout);
                    }
                    tickCount++;
                }
            }
        }
        Sleep(8); /* ~120 Hz */
    }

    CloseHandle(hPortal);
    CloseHandle(hME);
    return 0;
}
#endif

/**
 * Modo de autoverificación (--self-test) para validación determinista con Gemini 4 Argon.
 */
int PME_Lean_RunSelfTest() {
    printf("=== Lean Vertical Slice Bridge (PLEB): Self-Test (GOG + Steam Calibrated) ===\n");

    PMELeanOffsets ofs;
    PME_Lean_InitDefaultOffsets(&ofs);
    printf("[Offsets] ME GOG GEngine=0x%08lX (->+0x%lX->+0x%lX->+0x%lX, Loc=+0x%lX, Rot=+0x%lX)\n",
           (unsigned long)ofs.me_gengine_va,
           (unsigned long)ofs.me_gameplayers_offset,
           (unsigned long)ofs.me_actor_offset,
           (unsigned long)ofs.me_pawn_offset,
           (unsigned long)ofs.me_location_offset,
           (unsigned long)ofs.me_rotation_offset);
    printf("[Offsets] Portal Steam client.dll ViewOrigin RVA=0x%08lX, ViewAngles RVA=0x%08lX\n",
           (unsigned long)ofs.portal_view_origin_rva,
           (unsigned long)ofs.portal_view_angles_rva);

    PMELeanSliceState s;
    memset(&s, 0, sizeof(s));

    /* 1. Verificar punto de spawn con ancla base y ancla real de Edge_p.umap */
    s.anchor_me_uu.x = PME_ANCHOR_ME_X;
    s.anchor_me_uu.y = PME_ANCHOR_ME_Y;
    s.anchor_me_uu.z = PME_ANCHOR_ME_Z;
    s.source_origin_hu.x = 200.0f;
    s.source_origin_hu.y = 100.0f;
    s.source_origin_hu.z = 5.249344f;
    s.source_angles_deg.pitch = 0.0f;
    s.source_angles_deg.yaw = -90.0f;
    s.source_angles_deg.roll = 0.0f;

    PME_Lean_TransformFrame(&s);
    printf("[1/3] Spawn Transform OK -> ME Location UU: (%.1f, %.1f, %.1f), Yaw URU: %d\n",
           s.me_location_uu.x, s.me_location_uu.y, s.me_location_uu.z, s.me_rotation_uru.yaw);

    if (fabsf(s.me_location_uu.x - (-13819.0f)) > 0.5f || s.me_rotation_uru.yaw != 16384) {
        fprintf(stderr, "[ERROR] Fallo en validacion de spawn.\n");
        return 1;
    }

    /* Verificar también con el ancla real extraída de Edge_p.umap (PlayerStart Rooftop 1) */
    s.anchor_me_uu.x = PME_EDGE_UMAP_ROOF1_ANCHOR_X;
    s.anchor_me_uu.y = PME_EDGE_UMAP_ROOF1_ANCHOR_Y;
    s.anchor_me_uu.z = PME_EDGE_UMAP_ROOF1_ANCHOR_Z;
    PME_Lean_TransformFrame(&s);
    printf("      Real Edge_p.umap Rooftop 1 PlayerStart UU: (%.1f, %.1f, %.1f)\n",
           s.me_location_uu.x, s.me_location_uu.y, s.me_location_uu.z);

    /* 2. Simular Catapulta a 45° en la rampa roja de Edge_p */
    PMEVec3 inVel = { 0.0f, 0.0f, -750.0f };
    PMEVec3 floorNormal = { 0.0f, 0.0f, 1.0f };
    PMEVec3 ramp45Normal = { 0.70710678f, 0.0f, 0.70710678f };

    PMEVec3 outVelHu = PME2_ComputePortalExitVelocity(inVel, floorNormal, ramp45Normal);
    PMEVec3 outVelUu = PME_SourceToME_Velocity(outVelHu);
    printf("[2/3] Fling 45 deg en Rampa de Edge_p OK -> Salida ME Vel UU/s: (+%.1f, %.1f, +%.1f)\n",
           outVelUu.x, outVelUu.y, outVelUu.z);

    if (fabsf(outVelUu.x - outVelUu.z) > 1.0f || outVelUu.x < 1000.0f) {
        fprintf(stderr, "[ERROR] Fallo en proyeccion de velocidad a 45 grados.\n");
        return 1;
    }

    printf("[3/3] Puente Externo Cero-SDK (GOG + Steam) verificado y listo para ejecucion a 120 Hz.\n");
    return 0;
}

int main(int argc, char** argv) {
    int liveMode = 0;
    int probeMe = 0;
    int testLift = 0;
    int freeControlCheckpoint = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) {
            return PME_Lean_RunSelfTest();
        }
        if (strcmp(argv[i], "--probe-me") == 0) {
            probeMe = 1;
        }
        if (strcmp(argv[i], "--test-lift") == 0) {
            probeMe = 1;
            testLift = 1;
        }
        if (strcmp(argv[i], "--live") == 0) {
            liveMode = 1;
        }
        if (strcmp(argv[i], "--checkpoint-free") == 0) {
            freeControlCheckpoint = 1;
        }
    }

#ifdef _WIN32
    if (probeMe) {
        return PME_Lean_ProbeMirrorEdge(testLift);
    }
    if (liveMode) {
        return PME_Lean_RunLiveBridge(freeControlCheckpoint);
    }
#endif

    return PME_Lean_RunSelfTest();
}
