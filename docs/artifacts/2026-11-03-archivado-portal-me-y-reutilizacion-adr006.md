# Archivado de Flujos `portal-me-*` y Guía de Reutilización en ApertureRunner (`ADR-006` — B4)

- **Fecha**: 2026-11-03
- **Autor**: Benja (`B4`)
- **Decisión asociada**: [`ADR-006`](../../.agents/memory/DECISIONS.md) y [`PLAN_EQUIPO.md`](../../PLAN_EQUIPO.md)

---

## 1. Resumen del Cambio de Dirección (`ADR-006`)

Hasta el `2026-11-02`, las iteraciones previas del repositorio (`ADR-002` a `ADR-005`) exploraron la dirección **Portal dentro de Mirror's Edge** (llevar a Chell y la *Portal Gun* a la azotea inicial de `Edge_p` o a la esclusa de `Stormdrain_p`).

A partir del `2026-11-03` (`ADR-006`), la dirección oficial del equipo de 6 personas es **ApertureRunner: Mirror's Edge dentro de Portal**:
- Jugar el primer mapa de *Portal 1* (`testchmb_a_00`, Cámaras 00 y 01, build `19017868` de 32 bits) en **una sola ventana (Portal)** moviéndose como **Faith** (sprint con inercia, wallrun, wallclimb, agarrar bordes, slide, coil, Skill Roll, giro de 180° y vista de brazos/cuerpo en 1ª persona).

Siguiendo el principio de **Preservación Iterativa** del repositorio, los 4 flujos anteriores **no se eliminan**, sino que quedan marcados con `status: archivado` como referencia técnica y documental:
1. [`.agents/skills/portal-me-true-ingame-v2/SKILL.md`](../../.agents/skills/portal-me-true-ingame-v2/SKILL.md)
2. [`.agents/skills/portal-me-lean-slice/SKILL.md`](../../.agents/skills/portal-me-lean-slice/SKILL.md)
3. [`.agents/skills/portal-mirrors-edge-passthrough/SKILL.md`](../../.agents/skills/portal-mirrors-edge-passthrough/SKILL.md)
4. [`.agents/skills/portal-mirrors-edge-stormdrain/SKILL.md`](../../.agents/skills/portal-mirrors-edge-stormdrain/SKILL.md)

> [!WARNING]
> **Para trabajo nuevo en ApertureRunner**: No sigas las skills `portal-me-*` ni `portal-mirrors-edge-*`. Usa siempre [`PLAN_EQUIPO.md`](../../PLAN_EQUIPO.md), los contratos en [`mod/include/ar/`](../../mod/include/ar/) y las skills activas [`build-x86`](../../.agents/skills/build-x86/SKILL.md) y [`launch-portal-test`](../../.agents/skills/launch-portal-test/SKILL.md).

---

## 2. Qué se Reutiliza Exactamente en ApertureRunner y Quién lo Usa

| Conocimiento / Componente de los Flujos Archivados | Ubicación de Referencia | Dónde se Reutiliza en ApertureRunner (`PLAN_EQUIPO.md`) |
|---|---|---|
| **1. Offsets verificados en *Mirror's Edge* GOG v1.01** (`MirrorsEdge.exe`, PE Timestamp `0x4965FFF1`, `ImageBase = 0x00400000`, sin ASLR):<br>• `GEngine = 0x01FF8B20` $\rightarrow$ `+0x02BC` (`GamePlayers.Data[0]`) $\rightarrow$ `+0x0040` (`ATdPlayerController`) $\rightarrow$ `+0x01C0` (`ATdPlayerPawn`)<br>• `Location` (`+0x00E8`), `Rotation` (`+0x00F4`), `Velocity` (`+0x0100`), `Physics` (`+0x006B`)<br>• `MovementState` (`+0x04FE`) y `WalkingState` (`+0x0503`) | [`LESSONS.md`](../../.agents/memory/LESSONS.md), [`src/lean_bridge/lean_external_bridge.cpp`](../../src/lean_bridge/lean_external_bridge.cpp) | **Benja (`B2`)** en [`mod/tools/me_telemetry/`](../../mod/tools/me_telemetry/) para grabar trazas reales de Faith en solo lectura (`ReadProcessMemory`) y **Peña (`P5`)** para ajustar las constantes de `mod/faith_move/`. |
| **2. Offsets de `client.dll` en *Portal* Steam** (build `19017868`, 32 bits):<br>• `MainViewOrigin` (`client.dll + 0x0050F484`) y `MainViewAngles` (`+0x0050F490`)<br>• `s_pLocalPlayer` (`+0x004D78F0`)<br>• `cl_entitylist` (`+0x004DD00C`) e inspección de entidades `C_Prop_Portal` | [`LESSONS.md`](../../.agents/memory/LESSONS.md), [`src/true_ingame_v2/portal_entity_reader.cpp`](../../src/true_ingame_v2/portal_entity_reader.cpp) | **Gonzalo (`G1`, `G2`, `G4`)** al verificar offsets e interfaces de `client.dll` en `docs/contratos/portal_client_offsets.md` y enganchar la predicción del cliente en `mod/plugin/client/`. |
| **3. Esqueleto de Plugin de Servidor VSP (`IServerPluginCallbacks`)** para Source Engine (x86) | [`src/backend_source/portal_me_vsp.cpp`](../../src/backend_source/portal_me_vsp.cpp) | **Zarek (`Z1`–`Z3`)** como referencia del ciclo de vida de un plugin VSP (`Load`, `Unload`, `GameFrame`, `ClientActive`) en `mod/plugin/server/`. |
| **4. Configuración segura de *Mirror's Edge* (`TdInput.ini` y `TdEngine.ini`)**:<br>• Prohibición de tocar `TdGame\Config\DefaultInput.ini` (comprobación de hash al arrancar).<br>• Uso de `Documents\EA Games\Mirror's Edge\TdGame\Config\TdInput.ini` (conservando `[IniVersion]`) y modo ventana en `TdEngine.ini` (`Fullscreen=False`, `ResX=960`, `ResY=540`). | [`LESSONS.md`](../../.agents/memory/LESSONS.md) | **Benja (`B2`)** y **Antonio (`A1`–`A4`)** al abrir o automatizar *Mirror's Edge* sin disparar el bloqueo de archivo corrupto. |
| **5. Descompresión LZO1X de paquetes UE3 (`.umap` / `.upk` / `TdGame.u`)** (`COMPRESS_LZO = 0x2`, tag `0x9E2A83C1`) | [`.agents/skills/reverse-engineering/references/ue3-packages/unpack_upk.py`](../../.agents/skills/reverse-engineering/references/ue3-packages/unpack_upk.py) | **Antonio (`A1`–`A4`)** para la inspección/extracción de paquetes en `mod/tools/extractor/` y **Benja (`B2`)** para verificar enums (`EMovement`, `WAS_*`) en `TdGame.u`. |
| **6. Álgebra de transporte por portales y lateralidad UE3 $\leftrightarrow$ Source** | [`mod/core/include/aperture_runner/math.h`](../../mod/core/include/aperture_runner/math.h), [`src/common/ipc_stormdrain_bridge_v2.h`](../../src/common/ipc_stormdrain_bridge_v2.h) | **Peña (`P4`)** para conservar la velocidad y el estado de wallrun al cruzar portales (`T = M_out · R_π · M_in⁻¹`). |
| **7. Protocolo de memoria compartida multi-dispositivo de Antigravity** (`AGENTS.md`, `HANDOFF.md`, `LESSONS.md`, `DECISIONS.md`, `SESSIONS.md` con `merge=union`) | [`AGENTS.md`](../../AGENTS.md), [`.agents/memory/`](../../.agents/memory/) | **Todo el equipo (6 personas)** para sincronizar avances, decisiones y aprendizajes vía Git / GitHub Desktop sin conflictos de merge. |

---

## 3. Nota Importante sobre la Escala de Unidades (`2.54` vs `1.905`)

- En los flujos archivados (`portal-me-*`), se usó `1 HU = 1.905 cm` porque es la escala de **arquitectura de niveles** de Source cuando se construían mapas proxy `.vmf` para representar la azotea de `Edge_p`.
- En **ApertureRunner (`ADR-006`)**, todo el movimiento y el modelo de Faith usan la **escala de personaje de Source (`1 su = 1 pulgada = 2.54 cm`)**:
  $$\text{source} = \frac{\text{uu}}{2.54} \quad (\text{con el eje } Y \text{ invertido})$$
  De este modo, la cápsula de Faith en *Mirror's Edge* (`2 × 90 uu = 180 uu`) mide `70.87 su`, encajando con la altura estándar de Chell en *Portal* (`72 su`). Esta constante ya está fijada en [`mod/include/ar/units.h`](../../mod/include/ar/units.h) (`ar::kSuPerUu = 1.0f / 2.54f`).
