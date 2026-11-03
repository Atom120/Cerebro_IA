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

## Relacionado
- [[Inicio]]
- [[PLAN_EQUIPO]]
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
- [[20_Chats/2026-10-28 - ApertureRunner núcleo y director IA]]
- [[30_Temas/Portales no euclidianos|Portales no euclidianos]]
