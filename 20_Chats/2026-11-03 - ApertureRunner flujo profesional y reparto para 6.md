---
tipo: chat
fecha: 2026-11-03
herramienta: Claude
proyecto: ApertureRunner
tags: [chat, apertureRunner, mod, investigacion, equipo, antigravity, gemini, source, ue3]
---
# ApertureRunner: flujo profesional y reparto para 6

## Objetivo
- Investigar el flujo más profesional para meter Mirror's Edge en el primer nivel de Portal 1.
- Repartir el trabajo entre 6 personas: el usuario con Claude Code y 5 con Antigravity (Gemini Pro). Decidir qué agentes necesita cada uno.

## Resumen
- Investigación profunda guardada en Notion ("Research Reports"): https://app.notion.com/p/40d1d50b074681eda477ce985f070297
- **Portal ya está instalado** en el equipo: build 19017868, solo 32 bits, rama Source 2013 Singleplayer, última actualización 2025-06-27. Carga plugins de servidor (VSP).
- **Mirror's Edge no está instalado** (2026-11-03).
- El primer mapa real es `testchmb_a_00` (cámaras 00 y 01), no `testchmb_a_01`. No tiene pistola de portales: tiene 8 portales fijos con temporizadores, caja, botones, desintegrador, escenas y ascensor (323 entidades).

## Decisiones / resultados
- Recomendación: **ruta B, "Faith dentro de Source"**. Es una sola DLL x86 cargada en Portal. Sustituye el movimiento del jugador por una librería de movimiento de Faith y dibuja su cuerpo en primera persona. El modelo de Faith se genera en el PC de cada usuario a partir de su propia copia de Mirror's Edge; no se redistribuye nada.
- La ruta A (los dos juegos a la vez por memoria compartida) queda como fase posterior. Problema clave: Mirror's Edge no tiene la colisión del nivel de Portal, y no se encontró ningún precedente de inyectarla en UE3.
- El núcleo actual se reutiliza. `math.h` pasa a la librería de movimiento. `GuestBridge`/`IGuestPawnAdapter` pasa a ser un grabador de telemetría de solo lectura en Mirror's Edge, que sirve para ajustar el movimiento.
- Reparto:
  - **Líder**: Claude Code con universal-modder (orquestador y dos workers). Se encarga de contratos, CI e integración.
  - **P1**: plugin de servidor y firmas.
  - **P2**: librería de movimiento de Faith.
  - **P3**: lado cliente (predicción, cámara, brazos).
  - **P4**: pipeline de assets (extractor → Blender → studiomdl).
  - **P5**: mapa, telemetría de Mirror's Edge y QA.
- Antigravity 2.0:
  - Agentes con nombre en `.agents/agents/*.md` y skills en `.agents/skills/*/SKILL.md`.
  - Reglas en `AGENTS.md` y `.agents/rules`. Una regla sin `trigger` se descarta sin aviso.
  - Los workflows dejaron de funcionar el 2026-10-19.
  - Modelo en Pro: Gemini 4 Argon, con cuota semanal propia. Usar `flash` para tareas mecánicas.
- Seguridad: proyectos en modo worktree y permisos Request Review o Default. Nunca Turbo: hubo borrados de discos en Windows.
- Código de terceros:
  - Solo se reutiliza código MIT (faith-runner) y la licencia del Source SDK 2013.
  - SPT y mmultiplayer solo sirven como referencia de lectura.
  - Nunca usar Portal-Base (código filtrado).

## Código, comandos o archivos clave
- Estructura propuesta: `faith_move/`, `plugin/server/`, `plugin/client/`, `tools/extractor/`, `tools/me_telemetry/`, `docs/contracts/`, `qa/`.
- Un `AGENTS.md` compartido, y un `CLAUDE.md` que lo importa con `@AGENTS.md`.

## Pendientes
- [ ] Confirmar el mapa objetivo: `testchmb_a_00` (primer nivel) o `testchmb_a_01` (decisión anterior).
- [ ] Decidir la ruta: B (recomendada) o A.
- [ ] Crear el remoto en GitHub y hacer el primer commit de `aperture-runner-mashup`.
- [ ] Instalar Mirror's Edge (las personas de assets y de telemetría lo necesitan seguro).
- [ ] Spike de la semana 1: enganchar `IGameMovement::ProcessMovement` desde un VSP en el build 19017868.

## Plan de equipo (misma sesión)
- Se revisó git: Benja (Flextus) había subido `9078014`, su trabajo de Antigravity (`AGENTS.md`, `.agents/memory`, skills, `src/`, `levels/`). Ese trabajo seguía la dirección contraria: Portal dentro de Mirror's Edge, en la azotea `Edge_p`.
- Decisión del líder: Mirror's Edge dentro de Portal, en `testchmb_a_00` (ADR-006). Los flujos de Benja se archivan como referencia y no se borran.
- Se creó [[PLAN_EQUIPO]] con:
  - el reparto (Zarek: plugin de servidor; Peña: movimiento; Gonzalo: cliente y cámara; Antonio: assets; Benja: telemetría de ME, mapa y QA);
  - dependencias marcadas como 🟢 paralelo, 🟡 con stub y 🔴 espera;
  - hitos H1 a H4;
  - reglas de GitHub Desktop;
  - un agente de Antigravity por rol.
- Se actualizó la memoria compartida (`HANDOFF`, `MEMORY`, `DECISIONS` y `SESSIONS`) para que los agentes de todos lean el plan nuevo.
- Unidades: el movimiento usa ÷2.54 (escala de personaje). El 1.905 de `LESSONS.md` es la escala de arquitectura.

## S0–S2 (misma sesión)
- S1: el núcleo se copió a `mod/core/`, con CMake Win32 (`mod/CMakeLists.txt`, que detecta solo el `CMakeLists.txt` de cada dueño), `mod\build.bat` y CI en GitHub Actions (windows-latest, Win32).
- S2: contratos v1 en `mod/include/ar/`:
  - `units.h`: unidades, build 19017868, valores de Faith de ME.
  - `trace_provider.h`: `ITraceProvider` con `TraceHitKind`. Solo se hace wallrun sobre World o StaticProp.
  - `faith_movement.h`: `IFaithMovement` sin estado; `FaithState` copiable de 196 bytes, para que la predicción de Source rebobine; 24 `FaithAnimState`, cuyos nombres son los `$sequence`; botones semánticos.
  - `testing/box_world.h`.
  - `asset_manifest.json`.
- Una prueba comprueba que el manifiesto y `SequenceName()` coinciden. Pruebas x86: 77 de contratos y 174 del núcleo, todas OK.
- S0: los 16 agentes del plan están en `.agents/agents/`, y hay dos skills: `build-x86` y `launch-portal-test` (copia la DLL y el `.vdf` a `portal/addons` y abre `testchmb_a_00`).

## S4: revisión e integración de PRs (misma sesión)
- Integrados en `main` los PRs #1 (B2, telemetría de ME), #2 (B1, checklist de `testchmb_a_00`), #3 (B4, skills portal-me archivadas) y #4 (Z1, DLL VSP `aperture_runner.dll`). Build x86: 4/4 pruebas OK.
- Correcciones del líder: en Z1, `DisableThreadLibraryCalls(hinstDLL)`; en B2, aviso de que los 9 CSV son sintéticos (generados con nuestras propias constantes) y no sirven para ajustar el movimiento.
- Benja subió `f53db1c` directo a `main` (13 skills de universal-modder y módulos de la ruta archivada). Se recordó la regla: todo va por rama y PR.
- Lección: `git pull --rebase` después de `git merge --no-ff` aplana los merges y cambia los SHA, así que GitHub no marca los PRs como fusionados. Si `main` ya está al día, hay que subir sin rebase, o fusionar desde GitHub.
- Prueba real de Z1: Portal (build 19017868) carga `aperture_runner.dll` desde `portal/addons`; `plugin_print` la lista y `testchmb_a_00` abre sin cierres. Truco: `-condebug` escribe la consola en `portal/console.log` y `+plugin_print` lista los plugins cargados.
- Z3 de Zarek (PR #5; no hubo PR aparte para Z2, iba incluido): hook de vtable sobre `IGameMovement::ProcessMovement` (ranura 1 en MSVC) obtenido por `GameMovement001`, en modo passthrough. Probado en Portal: intercepta los ticks reales y el juego sigue estable. Se corrigió la prueba para que no se salte la vtable.
- 2026-11-04: PR #6 (B3, QA H1) integrado con reservas (tiempos que no cuadran con B1). S3 semana 1: build limpia 4/4; en Portal el hook sobrevive a `changelevel` hacia `testchmb_a_01`. Truco: `hl2.exe -hijack -game portal +changelevel <mapa>` manda comandos al Portal abierto; un `alias` con 100 `wait` es demasiado largo para Source. Peña, Gonzalo y Antonio aún no han subido nada; P1/P2 es la ruta crítica de H2.
- 2026-11-04: Sebas hizo P1 y P2 porque Peña no podía. `mod/faith_move/` tiene el movimiento de suelo de Faith (sprint con inercia, salto, gravedad, aterrizajes, escalones y coyote time), con 104 pruebas sobre `BoxWorld` y repetición determinista. Para comprobar que las pruebas sirven, se cambiaron valores a propósito: las de la rampa de sprint no fallaban porque comparaban contra el propio parámetro, así que se añadieron comprobaciones absolutas.
- 2026-11-04: Sebas hizo P3: parkour como máquina de estados en `faith_move` (wallrun, wallclimb, bordes, vault, slide, coil, Skill Roll y giro de 180°), con 212 pruebas. El contrato pasó a v2 con `kFaithEventTurn180`, porque la librería no puede girar la vista y el host tiene que hacerlo. Lecciones:
  - el coil sube el origen 36 su, y eso no debe contar como altura de caída;
  - los temporizadores en float terminan un tick tarde si no se redondean;
  - las pruebas de mutación encontraron 3 pruebas que no comprobaban nada (cooldown del borde, ápice del coil, paredes físicas).
- 2026-11-04: Sebas revisó el PR #7 de Benja (cambios pedidos) y corrigió Z4 de Zarek después de probarlo en Portal con sondas temporales. Fallaban tres cosas, todas por ABI de CS:GO frente a Source 2013: la vtable de `IEngineTrace`, `Ray_t` y la clasificación de entidades. Ahora hay un autotest en el mapa. Los comentarios de los PR se publican con la credencial de Git y la API de GitHub (`gh` no está instalado). Z5 desbloqueado.
- 2026-11-04 (tarde): Sebas hizo las tareas pendientes de Zarek, Gonzalo y Benja.
  - **Z5:** Faith mueve al jugador en Portal con `ar_enable 1`. Portales híbridos: cerca de un portal usa el movimiento original.
  - **Z2:** documentación de la ABI.
  - **G1:** `cl_predict` vale 0 y no hay predicción; G2 no aplica.
  - **G3:** módulo de cámara con 30 pruebas.
  - **B3:** prueba automática de H2, 9/9 PASS. Incluye sprint, salto, portal y ascensor.
  - **Bloqueado:** G4 (falta el modelo de Antonio), B2b (falta Mirror's Edge) y B5 (H4).
  - **Truco:** la entrada para las pruebas se inyecta con `ar_qa_input`, porque Source suelta `+forward` si la ventana pierde el foco.
- 2026-11-04 (noche): H2 cerrado y H3 casi completo. P4 (inercia por portales), cámara aplicada a la vista (giro de 180° e inclinación), casco agachado en el motor y QA de H3 9/9 en Portal. Lo único que bloquea el resto (A1–A4, brazos de G4, B2b, P5) es instalar Mirror's Edge.

## Relacionado
- [[Inicio]]
- [[PLAN_EQUIPO]]
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
- [[20_Chats/2026-10-28 - ApertureRunner núcleo y director IA]]
- [[30_Temas/Portales no euclidianos|Portales no euclidianos]]
