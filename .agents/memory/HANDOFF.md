# Estado de Relevo Activo (Active Handoff)

> **Instrucción para Antigravity**: Lee este archivo al iniciar cualquier conversación para retomar exactamente donde se quedó la última sesión/computadora. Actualízalo al terminar una tarea relevante o cuando el usuario indique que cambiará de dispositivo/cuenta.

## Última Actualización
- **Fecha**: 2026-11-04
- **Quién**: Sebas (Claude Code) — Repositorio `Cerebro_IA` (`main`)
- **Estado general**: **Cambio de dirección (ADR-006).** El proyecto ahora es *Mirror's Edge dentro de Portal*: Faith en el primer mapa de Portal 1 (`testchmb_a_00`). **El plan, el reparto por persona, las dependencias y los agentes están en [`PLAN_EQUIPO.md`](../../PLAN_EQUIPO.md). Léelo antes de proponer cambios y trabaja solo en las tareas y carpetas de tu persona.**
- **Equipo**: Sebas (líder/integración), Zarek (plugin servidor), Peña (movimiento de Faith), Gonzalo (cliente y cámara), Antonio (assets de Faith), Benja (telemetría ME, contrato del mapa y QA).
- **Usuarios de GitHub**: Sebas `Atom120`, Zarek `zarek21`, Benja `Flextus`, Gonzalo `DaddyGanzux`. Peña y Antonio: pendiente (entre los colaboradores también está `j0emike`, sin confirmar).

## Hecho (2026-11-03)
- S0: 16 agentes en `.agents/agents/` y skills `build-x86` y `launch-portal-test`.
- S1: `mod/` con el núcleo (`mod/core/`), CMake Win32, `mod\build.bat` y CI (`.github/workflows/mod-build.yml`). Pruebas: 77 de contratos y 174 del núcleo, todas OK en x86.
- S2: contratos v1 en `mod/include/ar/` y `docs/contratos/asset_manifest.json`.
- Integración de las **13 skills + CLI `um` de `universal-modder`** en `.agents/skills/` y `.agents/tools/universal-modder/bin/um.cmd`, e implementación completa de los 4 módulos de referencia de `portal-me-true-ingame-v2` (`src/common/ipc_true_ingame_v2.h`, `src/true_ingame_v2/`, `levels/edge_p_true_ingame_v2/`).
- **Z1 (Zarek)**: Implementada la DLL de servidor x86 `aperture_runner.dll` (`IServerPluginCallbacks` v003), banner para Portal 1 build 19017868, descriptor `addons/aperture_runner.vdf` y arnés de pruebas de carga dinámica autónoma (`test_plugin_load.exe`). Compilación Win32/x86 con MSVC 2026 y 100% de tests aprobados (3/3). Rama `zarek/Z1-dll-carga`.
- **S4 (Sebas)**: PRs #1 (B2), #2 (B1), #3 (B4) y #4 (Z1) revisados e integrados en `main`. Build x86: 4/4 pruebas OK (`test_contracts`, `test_core`, `test_plugin_load`, `test_me_telemetry`). Correcciones de revisión:
  - Z1: `DisableThreadLibraryCalls` recibe ahora `hinstDLL` (antes `nullptr`).
  - B2: los 9 CSV de `qa/telemetria/` son **sintéticos** (salen de `--generate-reference` con las constantes de `ar::me_reference`). Se añadió un aviso en su README: no sirven para ajustar el movimiento hasta que haya trazas grabadas en vivo.
  - **Z1 probada en Portal (2026-11-03, build 19017868):** el `console.log` muestra `Plugins: found file "aperture_runner.vdf"`, `Loading unsigned module addons\aperture_runner` y, en `plugin_print`, `0: "ApertureRunner: Mirror's Edge movement in Portal 1 (testchmb_a_00)"`. `testchmb_a_00` carga sin cierres; los errores del log son los mismos del juego sin el mod. No hizo falta `-insecure`. `printf` no sale en la consola de Source; en Z2 conviene usar `Msg`/`ConColorMsg` de `tier0.dll`. **Hito H1 parcial: la DLL carga.**

## ⚠️ 2026-11-04 (noche): H2 cerrado y H3 casi completo (Sebas) — NO rehacer ni modificar sin hablar con Sebas
- **P4:** el contrato pasó a v3. El host refresca el origen y la velocidad tras un teletransporte, y la librería solo gira sus propios vectores. Al cruzar un portal se conservan el wallrun o el slide y la velocidad de entrada (Portal frenaba hasta un ~40%). Detección de cruces entre ticks (`cruces=` en `ar_status`).
- **G4, parte de cámara:** `mod/plugin/client/client_view.*` (`IVEngineClient`, `VEngineClient014`, ranuras 19/20 verificadas). `mod/camera` se aplica a la vista: giro de 180°, inclinación en el wallrun y sacudida al aterrizar. `ar_camera 0|1`.
- **Casco agachado en el motor:** en los ticks de Faith también corre el movimiento original con `IN_DUCK` según Faith (efectos secundarios: casco, `FL_DUCKING`, altura de la vista, entidad de suelo, pasos), y después se escribe encima la posición y la velocidad de Faith.
- **QA:** `qa/scripts/h3_smoke.ps1` da **9/9 PASS**: wallrun real, inclinación de -13.4°, wallclimb de 47 su, giro de 180°, vista agachada (192 → 156), slide, portal (192.8 → 194.0 su/s). H2 sigue en 9/9. `ar_qa_input` acepta un tick de pulsación.
- **Bloqueado:** brazos de Faith (G4) ← A3 ← Mirror's Edge instalado (Antonio). Sin ME tampoco se pueden hacer B2b, P5 ni A1–A4.

## ⚠️ 2026-11-04: Sebas hizo las tareas pendientes de Zarek, Gonzalo y Benja (NO rehacer ni modificar sin hablar con Sebas)
- **Z5 + Z2:** con `ar_enable 1`, Faith mueve al jugador en Portal (`mod/plugin/server/faith_driver.*`).
  - Comandos de consola: `ar_enable [0|1]`, `ar_status`, `ar_debug [0|1|2]` y `ar_qa_input <adelante> <lado> <botones j/d/t/w> <ticks>`.
  - Offsets de `CMoveData`, `edict_t` y `CCommand` verificados en el juego (`docs/contratos/source_2013_abi.md`).
  - **Portales:** cerca de un `prop_portal` (donde el casco toca el hueco) se usa el movimiento original de Portal, que hace el teletransporte. También se usa si el jugador empieza dentro de un sólido.
- **G1:** en un jugador `cl_predict` vale 0 y el cliente no predice (0 ticks), así que **G2 no aplica**. Hook de `client.dll` instalado, y offsets de Benja de `client.dll` verificados (`docs/contratos/portal_client_offsets.md`).
- **G3:** `mod/camera` con `UpdateCamera()` (inclinación, FOV, balanceo, sacudidas, Skill Roll y giro de 180°) y 30 pruebas. **Falta aplicarlo a la vista (G4).**
- **B3:** `qa/scripts/h2_smoke.ps1` prueba H2 en Portal sin teclado: **9/9 PASS** (autotest, sprint con rampa de 84 su/s², salto de 49.6 su, agacharse, cruce de `portal_red_0` → `portal_blue_0`, ascensor hasta z=725, `ar_enable 0`, sin cierres). Informes en `qa/resultados/H1.md` (verificación) y `qa/resultados/H2.md` (incluye lo que **no** cubre).
- La build x86 pasa 6/6 (`test_contracts`, `test_core`, `test_faith_move`, `test_camera`, `test_plugin_load`, `test_me_telemetry`).

## Nota semanal S3 — semana 1 (2026-11-04, Sebas)
**Build de integración:** `main` en `f5872f2`, compilado desde cero (Win32/x86, Release). Sin avisos; 4/4 pruebas OK (`test_contracts`, `test_core`, `test_plugin_load`, `test_me_telemetry`).

**Prueba en Portal (build 19017868):**
- El plugin carga y el hook `ProcessMovement` se instala en `testchmb_a_00`.
- Unos 4 minutos de juego (16.500 ticks interceptados) sin cierres.
- `changelevel testchmb_a_01` (enviado con `hl2.exe -hijack`): `LevelShutdown` → `LevelInit: testchmb_a_01` → `ServerActivate`, y el hook sigue interceptando (18.500 ticks en total).
- Detalle para Zarek: `LevelShutdown` llega dos veces en cada cambio de mapa (la segunda con el nombre vacío) y una vez al arrancar. Es inofensivo, pero conviene ignorarlo si no hay mapa activo antes de liberar estado en Z5.

| Persona | Hecho | En curso / siguiente | Estado |
|---|---|---|---|
| Sebas | S0, S1, S2, S3 (semana 1), S4 (PRs #1–#6) | S4 continuo; S3 cada semana | 🟢 |
| Zarek | Z1, Z3, Z4 (corregido e integrado por Sebas, 2026-11-04) | **Z5**: conectar `faith_move` con `ServerTraceProvider` en `ProcessMovement`. Lo que falta de Z2 (`PlayerInfoManager002`, `VEngineServer021`, campos de `CPortal_Player`) entra con Z5 | 🟢 |
| Peña | P1, P2 y P3 (hechos por Sebas el 2026-11-04) | P4 (portales) y P5 (ajuste con trazas reales) | 🟡 |
| Gonzalo | — (rama `G1` creada, vacía) | G1 (predicción en `client.dll`): falta para cerrar del todo H1. G3 (cámara) se puede hacer en paralelo | 🟡 |
| Antonio | — (sin ramas ni PRs) | A1 (necesita Mirror's Edge instalado + UEViewer), A2 | 🟡 |
| Benja | B1, B2 (CSV sintéticos), B3 (H1, provisional), B4 | Aclarar los tiempos de `H1.md` y adjuntar el log crudo; grabar trazas reales con `--record` (B2b) | 🟢 |

**Hitos:** H1 "la DLL vive": servidor ✅, QA ✅ (provisional), cliente ⏳ (G1). H2 desbloqueado en movimiento (P1/P2) y trace de servidor (Z4); requiere Z2 + Z5.

**Para la semana 2:** Peña sube P1 cuanto antes (aunque sea el esqueleto con pruebas de `BoxWorld`). Gonzalo sube G1. Zarek cierra Z2 y empieza Z4. Ramas con el formato `nombre/ID-descripcion` (no `G1` a secas). Nada directo a `main`.

- **P1 + P2 (hechos por Sebas, 2026-11-04, porque Peña no podía):** `mod/faith_move/` implementa `ar::CreateFaithMovement` con el movimiento de suelo: correr (2419 su/s² hasta 157.5), sprint con inercia (157.5 → 283.5 en 1.5 s), caminar, frenada, salto de 248 + 39.4 en la carrera, gravedad de 646, coyote time, caída, aterrizaje suave y duro (≥ 208.7 su: bloqueo de 0.5 s), `FallUncontrolled` (> 393.7 su), escalones de 18 y colisión estilo `TryPlayerMove`. Es determinista: la repetición para la predicción da un resultado idéntico bit a bit. 104 comprobaciones en `test_faith_move`; la build x86 pasa 5/5. Los valores `placeholder` están en `faith_params.h` (P5). **Esto desbloquea Z5 (Zarek, con Z4) y G2 (Gonzalo).** Peña sigue con P3 (parkour) sobre este código; lee `mod/faith_move/README.md`.

- **P3 (hecho por Sebas, 2026-11-04, porque Peña no podía):** parkour en `mod/faith_move/`: máquina de estados con wallrun izq./der. y salto de pared, wallclimb con giro y salto, agarrar borde y subir, vault, slide, agacharse, coil y Skill Roll, y giro de 180°. Hay 212 comprobaciones en `test_faith_move`, y se verificaron con 9 mutaciones del código (todas detectadas). La build x86 pasa 5/5. Los detalles y los valores están en `mod/faith_move/README.md`.
  - **Contrato v2** (`mod/include/ar/faith_movement.h`): nuevo evento `kFaithEventTurn180`. **Zarek/Gonzalo:** al recibirlo, el host suma 180° al yaw de la vista (la cámara lo reparte en ~0.3 s). `FaithState` no cambió (196 bytes).
  - **Zarek (Z5):** al agacharse, deslizarse o hacer coil, `FaithState::ducked = true`; hay que poner el casco agachado y `FL_DUCKING`. Con el coil el origen sube 36 (la cabeza no se mueve).

- **PR #7 (Benja, `benja/B3-qa-hito-h1`): NO integrado, cambios pedidos (Sebas, 2026-11-04).**
  1. **Columna `ar_enable 1` en PASS en los 14 puntos.** Es imposible: `ar_enable` todavía no existe como cvar en la DLL y el movimiento de Faith no está conectado (falta Z5). Debe ser `N/A (H2)`, como en el informe que ya está en `main`.
  2. **Tiempos imposibles.** El informe dice que los portales se abren a los 66.5 s, pero la partida entera (dos ascensores incluidos) dura unos 75 s, o 5.000 ticks. En la prueba S3 de Sebas, unos 4 minutos de juego son 16.500 ticks.
  3. **Sigue faltando lo que se pidió en el PR #6:** el `console.log` crudo y aclarar si usó `ent_fire` o `host_timescale`.
  4. **Ruta personal en un enlace** (`file:///c:/Users/Benjamin%20Sepulveda/...`). Usa rutas relativas.
  5. **La rama sale de `6d5adfd`** y choca con `main` en `qa/resultados/H1.md` (los dos lo crean) y en `HANDOFF.md`. Además, el `HANDOFF` del PR borra la regla de ramas, las notas de revisión y el estado de P1–P3.
  - **Qué hacer:** actualizar la rama con `main`, editar el `qa/resultados/H1.md` que ya existe (no crear otro), dejar `ar_enable 1` como `N/A (H2)` y adjuntar el log crudo o un recorte real. No marcar H1 como cerrado: falta G1 (cliente).

- **Z4 (PR #8 de Zarek): integrado con correcciones de Sebas (2026-11-04, rama `sebas/Z4-fix`).** Probado en Portal 19017868.
  - `IEngineTrace` con la vtable de Source 2013: `TraceRay` en la ranura 4 (antes la 5, que colgaba Portal).
  - `Ray_t` sin `m_pWorldAxisTransform` (antes atravesaba el suelo).
  - `static_assert` de desplazamientos de `Ray_t`, `CBaseTrace` y `trace_t`.
  - `ClassifyHitEntity`: clasifica por el nombre de clase (`m_pEnt` → `GetNetworkable()` → `GetClassName()`, ranuras 4 y 3). En el juego dio: `worldspawn` → World, `prop_physics` (la caja) → Physics, `prop_dynamic`/`func_brush` → StaticProp, `npc_security_camera` → Physics.
  - **Autotest en el mapa:** al cargar `testchmb_a_00`, la consola muestra `Autotest Z4 (...): OK  z=160.0 kind=1`. **Benja (QA): si sale FALLO, Z4 está roto.**
  - **Zarek, ya puedes hacer Z5.** Ten en cuenta:
    1. `ProcessMovement(pPlayer, pMove)`: lee y escribe origen y velocidad en `CMoveData` (`m_vecAbsOrigin`, `m_vecVelocity`, `m_vecViewAngles`, `m_nButtons`, `m_flForwardMove`, `m_flSideMove`). Comprueba los desplazamientos en el juego, como en Z4.
    2. Botones: `IN_JUMP` → `kFaithButtonUp`, `IN_DUCK` → `Down`, `IN_RELOAD` → `Turn180`, `IN_WALK` → `Walk`. `buttons_pressed` = pulsados este tick y no el anterior.
    3. Guarda un `FaithState` por jugador. `CreateFaithMovement(FaithHostConfig{})` una sola vez.
    4. `ducked` → casco agachado y `FL_DUCKING`. El evento `Turn180` → sumar 180° al yaw de la vista.
    5. **Portales:** la traza normal del motor no atraviesa los portales (Portal usa trazas especiales en `CPortalGameMovement`). Al principio, para cruzar un portal deja que el movimiento original de Portal haga el tick en el que se toca un portal, o investígalo antes de Z5. Es el riesgo n.º 1 de H2.
    6. `ar_enable` (cvar) todavía no existe: créalo con `ICvar` (`VEngineCvar004`) o empieza con un comando de consola.

## Próximos Pasos (semana 2)
- **H1 cerrado** en lo que depende del mod. **H2 funcionando** en Portal (ver `qa/resultados/H2.md`).
- **Zarek:** casco agachado en el motor (`FL_DUCKING` + bounds) cuando `FaithState::ducked`, y el wallrun a través de portales, junto con Peña (P4).
- **Gonzalo:** G4, aplicar `mod/camera` a la vista (enganchar el cálculo de la vista del cliente; incluye el giro de 180° del evento `Turn180`) y los brazos de Faith (modelo provisional hasta A3).
- **Peña:** P4 (continuar el wallrun al cruzar un portal) y P5 (ajuste con trazas reales).
- **Antonio:** A1–A4 (necesita Mirror's Edge instalado).
- **Benja:** B2b (trazas reales con `--record`; necesita ME instalado) y B5 (vídeo, H4).
- **Sebas:** S3 semanal (ahora con `qa/scripts/h2_smoke.ps1`) y S4.
- **Regla:** nada se sube directo a `main`; todo va por rama y PR. Antes de empezar, **actualiza tu rama desde `main`**.
- **Z3 (Zarek)**: Implementado el hook de vtable de `IGameMovement::ProcessMovement` en passthrough puro (`movement_hook.h/.cpp`), obtención de `GameMovement001` vía `gameServerFactory`, logging en consola de Source con `tier0.dll!Msg`, y arnés autónomo con mock dinámico de `IGameMovement` en `test_plugin_load.exe`. Compilación Win32/x86 y 100% de tests aprobados (4/4). Rama `zarek/Z3-hook-passthrough`.
  - **Revisado e integrado por Sebas (PR #5) y probado en Portal (build 19017868):** el log muestra `ProcessMovement hookeado exitosamente` y `passthrough activo (500…2000 ticks interceptados)` en `testchmb_a_00`, sin cierres. El banner ya sale en la consola de Source gracias a `tier0!Msg`.
- **Z4 (Zarek)**: Implementado `ServerTraceProvider` (`mod/plugin/server/server_trace_provider.h/.cpp`) satisfaciendo `ar::ITraceProvider`, enlazado a `EngineTraceServer003` vía `interfaceFactory` de Source (`engine.dll`), estructuras binarias exactas Source (`VectorAligned` 16 bytes, `Ray_t` con `m_pWorldAxisTransform`, `cplane_t`, `trace_t`, máscara `kMaskPlayerSolid = 0x201400Bu`), filtro `TraceFilterPlayerOnly` para ignorar la entidad del jugador, y arnés en `test_plugin_load.exe` con `MockEngineTrace` (6/6 comprobaciones espaciales OK: piso Z=0, pared X=100, libre, colisión inicial en sólido y filtrado). Compilación Win32/x86 en Debug y Release con 100% de tests aprobados (5/5). Rama `zarek/Z4-trace-provider`. Desbloquea Z5.
- **B3 (Benja)**: Recorrida y verificada al 100% la checklist de `testchmb_a_00` (`CHK-01` a `CHK-14`) en Portal (build 19017868) con `aperture_runner.dll` y hook de `ProcessMovement` passthrough (`ar_enable 0`). Grabada demo local `h1_testchmb_a_00.dem` (1,170,204 bytes, 4,733 frames de juego, 71.6 s), más de 5,000 ticks de simulación continua interceptados sin cierres ni desincronización, supervivencia probada a través de `ChangeLevel` a `testchmb_a_01` y generado el informe de QA en `qa/resultados/H1.md`. Rama `benja/B3-qa-h1`. **Hito H1 validado por QA.**
  - Revisión de Sebas (PR #6, integrado): en el informe, los portales de la cámara 00 se abren a t=15.9 s, pero B1 dice que `relay_start_map` los abre a t=66.5 s. Benja debe anotar en `qa/resultados/H1.md` si usó `ent_fire`, `host_timescale` u otro atajo, y adjuntar el `console.log` crudo (fuera de git o recortado) para que el informe se pueda reproducir.

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
