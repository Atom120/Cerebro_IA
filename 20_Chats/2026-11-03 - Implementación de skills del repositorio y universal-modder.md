---
tipo: chat
fecha: 2026-11-03
herramienta: Antigravity
proyecto: Portal x Mirror's Edge / AntigravityColab
tags: [antigravity, skills, universal-modder, portal, mirrors-edge, d3d9, cpp]
---
# Implementación de skills del repositorio y universal-modder

## Objetivo
- Buscar todas las skills presentes o referenciadas en el repositorio `Cerebro_IA` e implementar ambos grupos en el espacio de trabajo compartido.

## Resumen
- **Grupo 1 (7 skills de `.agents/skills/` — `AntigravityColab` y `Portal x Mirror's Edge`)**:
  - Se implementaron por completo los **4 módulos técnicos pendientes** de la skill activa [`portal-me-true-ingame-v2`](../.agents/skills/portal-me-true-ingame-v2/SKILL.md):
    1. **Módulo 1 (Geometría 1:1 de `Edge_p`)**: Manifiesto `levels/edge_p_true_ingame_v2/slice_manifest.json` calibrado con la azotea activa `(19500.0, -3009.5, 8517.2)` UU, generación de `me_edge_true_ingame_v2.vmf` (7 sólidos reales) y compilación con `vbsp.exe` de Steam a `me_edge_true_ingame_v2.bsp` (`256,076 bytes`).
    2. **Módulo 2 (Inyección de Inputs en Memoria + Audio)**: Captura de entradas cuando `MirrorsEdge.exe` está en foco, conversión inversa `PME_METoSource_Rotation` (`FRotator` URU $\rightarrow$ `QAngle` grados) y envío en segundo plano a la ventana `Valve001` de `hl2.exe` con `snd_mute_losefocus 0`.
    3. **Módulo 3 (Lectura Real de `C_Prop_Portal`)**: Escaneo de `cl_entitylist` (`client.dll + 0x004C6624`) para leer `m_vecOrigin`, `m_angAngles`, `m_bIsPortal2`, `m_bActivated` y `m_hLinkedPortal` de los portales Azul y Naranja, más trazador analítico 1:1 de respaldo en `src/true_ingame_v2/true_ingame_bridge_v2.cpp`.
    4. **Módulo 4 (Proxy `d3d9.dll` + Hook `IDirect3DDevice9::EndScene`)**: Implementado en `src/true_ingame_v2/me_true_ingame_d3d9.cpp` (`me_true_ingame_d3d9.dll`), ocultando a Faith (`DrawScale = 0.0f` en `Pawn + 0x10C`) y dibujando en 3D los Portales Azul/Naranja con Z-Buffer, la vista interior con **Chell**, el Viewmodel 3D en 1ª persona de la **Portal Gun** (`MinZ=0.0, MaxZ=0.08`) y la retícula oficial de *Portal*.
  - Se ejecutaron y verificaron todos los scripts de `portal-me-true-ingame-v2`, `portal-me-lean-slice`, `portal-mirrors-edge-passthrough`, `portal-mirrors-edge-stormdrain` y `colab-sync`.
- **Grupo 2 (13 skills + CLI `um` de `universal-modder`)**:
  - Se integraron las 13 skills de `rehan-remade/universal-modder` directamente en `.agents/skills/` (`mod-any-game`, `mashup-mods`, `game-recon`, `game-research-websearch`, `reverse-engineering`, `port-a-mod`, `asset-pipeline`, `fal-assets`, `game-automation`, `showcase-video`, `publish-mod`, `share-field-notes`, `blitz3d-decomp`).
  - Se instaló el paquete y el CLI `um` en `.agents/tools/universal-modder/bin/um.cmd` (y en Python local), verificando `um --help` y `um scan --list`.

## Decisiones / resultados
- **20 skills activas en `.agents/skills/`**: Al vivir dentro del repositorio bajo `.agents/skills/` (declarado en `.agents/skills.json`), cualquier computadora que haga Pull en `Cerebro_IA` dispone automáticamente de los dos grupos de skills sin configuración manual por máquina.
- Todos los tests de extremo a extremo (`build-and-verify-true-ingame-v2.js`, `build-and-verify-lean-slice.js`, `verify-bridge-pipeline.js`, `verify-stormdrain-pipeline.js` y `um scan --list`) pasaron con código `0`.

## Código, comandos o archivos clave
```powershell
node .agents/skills/portal-me-true-ingame-v2/scripts/build-and-verify-true-ingame-v2.js
node .agents/skills/portal-me-lean-slice/scripts/build-and-verify-lean-slice.js
.agents\tools\universal-modder\bin\um.cmd scan --list
```
- `src/common/ipc_true_ingame_v2.h`
- `src/true_ingame_v2/true_ingame_bridge_v2.cpp`
- `src/true_ingame_v2/me_true_ingame_d3d9.cpp`
- `levels/edge_p_true_ingame_v2/slice_manifest.json` y `me_edge_true_ingame_v2.bsp`

## Pendientes
- [x] Implementar los 4 módulos de `portal-me-true-ingame-v2` y compilar `.bsp` + binarios x86.
- [x] Integrar las 13 skills de `universal-modder` y el CLI `um` en `.agents/`.
- [ ] Prueba visual en vivo copiando `me_true_ingame_d3d9.dll` como `d3d9.dll` en `Mirror's Edge/Binaries/` y ejecutando `true_ingame_bridge_v2.exe --live`.

## Relacionado
- [[Inicio]]
- [[20_Chats/2026-11-03 - Instalación de universal-modder]]
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
- [[30_Temas/Portales no euclidianos|Portales no euclidianos]]
