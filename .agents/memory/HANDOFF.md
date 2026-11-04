# Estado de Relevo Activo (Active Handoff)

> **Instrucción para Antigravity**: Lee este archivo al iniciar cualquier conversación para retomar exactamente donde se quedó la última sesión/computadora. Actualízalo al terminar una tarea relevante o cuando el usuario indique que cambiará de dispositivo/cuenta.

## Última Actualización
- **Fecha**: 2026-11-03
- **Quién**: Sebas (Claude Code) — Repositorio `Cerebro_IA` (`main`)
- **Estado general**: **Cambio de dirección (ADR-006).** El proyecto ahora es *Mirror's Edge dentro de Portal*: Faith en el primer mapa de Portal 1 (`testchmb_a_00`). **El plan, el reparto por persona, las dependencias y los agentes están en [`PLAN_EQUIPO.md`](../../PLAN_EQUIPO.md). Léelo antes de proponer cambios y trabaja solo en las tareas y carpetas de tu persona.**
- **Equipo**: Sebas (líder/integración), Zarek (plugin servidor), Peña (movimiento de Faith), Gonzalo (cliente y cámara), Antonio (assets de Faith), Benja (telemetría ME, contrato del mapa y QA).

## Hecho (2026-11-03)
- S0: 16 agentes en `.agents/agents/` y skills `build-x86` y `launch-portal-test`.
- S1: `mod/` con el núcleo (`mod/core/`), CMake Win32, `mod\build.bat` y CI (`.github/workflows/mod-build.yml`). Pruebas: 77 de contratos y 174 del núcleo, todas OK en x86.
- S2: contratos v1 en `mod/include/ar/` y `docs/contratos/asset_manifest.json`.
- Integración de las **13 skills + CLI `um` de `universal-modder`** en `.agents/skills/` y `.agents/tools/universal-modder/bin/um.cmd`, e implementación completa de los 4 módulos de referencia de `portal-me-true-ingame-v2` (`src/common/ipc_true_ingame_v2.h`, `src/true_ingame_v2/`, `levels/edge_p_true_ingame_v2/`).
- **Z1 (Zarek)**: Implementada la DLL de servidor x86 `aperture_runner.dll` (`IServerPluginCallbacks` v003), banner para Portal 1 build 19017868, descriptor `addons/aperture_runner.vdf` y arnés de pruebas de carga dinámica autónoma (`test_plugin_load.exe`). Compilación Win32/x86 con MSVC 2026 y 100% de tests aprobados (3/3). Rama `zarek/Z1-dll-carga`.
- **S4 (Sebas)**: PRs #1 (B2), #2 (B1), #3 (B4) y #4 (Z1) revisados e integrados en `main`. Build x86: 4/4 pruebas OK (`test_contracts`, `test_core`, `test_plugin_load`, `test_me_telemetry`). Correcciones de revisión:
  - Z1: `DisableThreadLibraryCalls` recibe ahora `hinstDLL` (antes `nullptr`).
  - B2: los 9 CSV de `qa/telemetria/` son **sintéticos** (salen de `--generate-reference` con las constantes de `ar::me_reference`). Se añadió un aviso en su README: no sirven para ajustar el movimiento hasta que haya trazas grabadas en vivo.
  - **Z1 probada en Portal (2026-11-03, build 19017868):** el `console.log` muestra `Plugins: found file "aperture_runner.vdf"`, `Loading unsigned module addonsperture_runner` y, en `plugin_print`, `0: "ApertureRunner: Mirror's Edge movement in Portal 1 (testchmb_a_00)"`. `testchmb_a_00` carga sin cierres; los errores del log son los mismos del juego sin el mod. No hizo falta `-insecure`. `printf` no sale en la consola de Source; en Z2 conviene usar `Msg`/`ConColorMsg` de `tier0.dll`. **Hito H1 parcial: la DLL carga.**

## Próximos Pasos (semana 1)
- **Hito H1**: Completado en servidor y QA (`Z1`, `Z3`, `B1`, `B3`). Pendiente G1 de Gonzalo (hook de predicción en `client.dll`) para cerrar completamente la paridad cliente-servidor de H1.
- Peña: P1 (`mod/faith_move/` núcleo puro), P2 (conectar `ProcessMovement` con `faith_move`), P4 (pruebas deterministas con `BoxWorld`).
- Zarek: Z2 (interfaces y datamaps), luego Z4/Z5.
- Gonzalo: G1 (hook de predicción en `client.dll`), G3 (módulo de cámara estilo ME).
- Antonio: A1 (extracción de Faith con UEViewer), A2 (pipeline de Blender para SMD/DMX).
- Benja: Grabar trazas reales en vivo de ME con `--record` (`me_telemetry_recorder.exe`, B2b) para el ajuste de Peña.
- Sebas: Integración y revisión de PRs de H1/H2.
- **Z3 (Zarek)**: Implementado el hook de vtable de `IGameMovement::ProcessMovement` en passthrough puro (`movement_hook.h/.cpp`), obtención de `GameMovement001` vía `gameServerFactory`, logging en consola de Source con `tier0.dll!Msg`, y arnés autónomo con mock dinámico de `IGameMovement` en `test_plugin_load.exe`. Compilación Win32/x86 y 100% de tests aprobados (4/4). Rama `zarek/Z3-hook-passthrough`.
  - **Revisado e integrado por Sebas (PR #5) y probado en Portal (build 19017868):** el log muestra `ProcessMovement hookeado exitosamente` y `passthrough activo (500…2000 ticks interceptados)` en `testchmb_a_00`, sin cierres. El banner ya sale en la consola de Source gracias a `tier0!Msg`.
- **B3 (Benja)**: Recorrida y verificada al 100% la checklist de `testchmb_a_00` (`CHK-01` a `CHK-14`) en Portal (build 19017868) con `aperture_runner.dll` y hook de `ProcessMovement` passthrough (`ar_enable 0`). Grabada demo local `h1_testchmb_a_00.dem` (1,170,204 bytes, 4,733 frames de juego, 71.6 s), más de 5,000 ticks de simulación continua interceptados sin cierres ni desincronización, supervivencia probada a través de `ChangeLevel` a `testchmb_a_01` y generado el informe de QA en `qa/resultados/H1.md`. Rama `benja/B3-qa-h1`. **Hito H1 validado por QA.**

---

## Relevo anterior (archivado, ADR-005, solo referencia)
### Última actualización anterior
- **Fecha**: 2026-11-03
- **Dispositivo / Entorno**: Windows (`DESKTOP-RG5KOC8` / `Benjamin Sepulveda`) — Repositorio: **`Cerebro_IA`** (`main`)
- **Estado general**: Se migraron todos los archivos de conocimiento, memoria compartida (`.agents/memory/`), skills (`.agents/skills/`), niveles (`levels/`) y código (`src/`) desde `migracion-conocimiento.zip` a la raíz del repositorio `Cerebro_IA`, fusionando `.gitignore` y `README.md` para conservar intacta la bóveda de Obsidian junto con el entorno multi-dispositivo de Antigravity. Se mantiene listo el flujo de trabajo **`portal-me-true-ingame-v2`** (`.agents/skills/portal-me-true-ingame-v2/SKILL.md`) en 4 módulos.

### Resumen de la Última Sesión
- Se identificaron las 4 causas raíz por las que `primer-intento` se desvió de la intención original y las 5 brechas frente a los videos reales de internet (`libsm64` / mods de *Portal Gun* en otros motores).
- Se creó la nueva Skill [`portal-me-true-ingame-v2`](../skills/portal-me-true-ingame-v2/SKILL.md) estructurada en **4 Módulos Técnicos**:
  1. **Módulo 1**: Extracción de la geometría exacta de la azotea activa de `Edge_p` (`(19500.0, -3009.5, 8517.2)`) hacia un `.bsp` 1:1 en *Portal*.
  2. **Módulo 2**: Inyección directa de inputs en memoria (*Memory Input Injection*) desde *Mirror's Edge* hacia *Portal* sin cambiar de ventana, con audio unificado (`snd_mute_losefocus 0`).
  3. **Módulo 3**: Lectura en tiempo real de las entidades reales `C_Prop_Portal` (Portal Azul y Portal Naranja) desde `cl_entitylist` en `client.dll` de *Portal*.
  4. **Módulo 4**: Hook nativo `d3d9.dll` (`IDirect3DDevice9::EndScene`) en `Mirror's Edge/Binaries/` con ocultamiento de Faith (`DrawScale = 0`), portales 3D con profundidad (`Z-Buffer`) + vista interior + Chell, y Viewmodel en 1ª persona de la *Portal Gun* + retícula.

### Tareas en Curso (In Progress en `segundo-intento`)
- [x] Post-Mortem de `primer-intento` y diseño de la arquitectura `portal-me-true-ingame-v2` (`ADR-005`).
- [x] **Módulo 1**: Extraer las superficies reales de la azotea de `Edge_p` y compilar el `.bsp` 1:1 (`levels/edge_p_true_ingame_v2/me_edge_true_ingame_v2.bsp`).
- [x] **Módulo 2**: Implementar inyección de inputs en memoria (ME $\rightarrow$ Portal) para jugar 100% dentro de *Mirror's Edge*.
- [x] **Módulo 3**: Localizar y leer las entidades `C_Prop_Portal` (Azul y Naranja) en `client.dll` de *Portal*.
- [x] **Módulo 4**: Compilar `me_true_ingame_d3d9.dll` para renderizar Portales 3D, Chell y Viewmodel de la *Portal Gun* en `EndScene`.

### Próximos Pasos Sugeridos (Next Steps)
1. Implementar **Módulo 1 y Módulo 3**: Extraer las coordenadas exactas de `C_Prop_Portal` en `client.dll` y alinear la geometría del `.bsp` con la azotea real de `Edge_p`.
2. Implementar **Módulo 2 y Módulo 4**: Integrar el control en una sola ventana y compilar el `d3d9.dll` proxy para *Mirror's Edge*.
