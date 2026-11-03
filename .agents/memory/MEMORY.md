# Memoria Compartida del Proyecto (Cross-Device Memory)

> Este archivo mantiene el contexto global y permanente entre todas las instancias de **Antigravity** (diferentes computadoras, sistemas operativos y cuentas).

## 1. Propósito del Repositorio
- **Nombre**: `AntigravityColab`
- **Objetivo**: Espacio de trabajo colaborativo sincronizado mediante **Git / GitHub Desktop** donde múltiples instancias de Antigravity mantienen el contexto continuo, comparten habilidades (`skills`), reglas (`AGENTS.md`) y acumulan aprendizaje entre dispositivos.
- **Proyecto Activo** (desde 2026-11-03, ADR-006): **ApertureRunner**, *Mirror's Edge dentro de Portal*. Jugar el primer mapa de Portal 1 (`testchmb_a_00`) moviéndose como Faith, en una sola ventana (Portal). Plan y reparto: `PLAN_EQUIPO.md`.
- **Proyecto anterior (archivado)**: mod *Pass-Through* de Portal (Chell + Portal Gun) dentro de una azotea de Mirror's Edge (ADR-002 a ADR-005).

## 2. Preferencias Globales del Usuario y Filosofía de Ingeniería
- **Idioma principal**: Español (documentación, explicaciones, resúmenes de handoff y comentarios).
- **Alcance estricto de Vertical Slice**: No se busca fusionar el juego completo ni siquiera un nivel completo; **con un Vertical Slice funcional basta**.
- **Alcance estricto de Vertical Slice sin sacrificar la Experiencia Visual Central**: No se busca fusionar el juego completo ni un nivel entero; basta con una sección acotada (1 azotea de `Edge_p`), pero **debe jugarse en una sola ventana viendo al personaje/arma/portales de Portal dentro del mundo de Mirror's Edge** (nunca reducir el mod a solo copiar coordenadas `(X, Y, Z)` entre dos ventanas separadas).
- **Preservación Iterativa**: Al explorar o crear nuevos flujos de trabajo, **no eliminar los flujos anteriores**; conservarlos como referencia modular y documentar comparativamente sus ventajas y desventajas.
- **Diseño amigable para las herramientas de Gemini 4 Argon**: Favorecer arquitecturas deterministas verificables por consola (`run_command`), scripts Todo-en-Uno y mínima dependencia de inspección manual de punteros dinámicos en RAM.
- **Flujo de sincronización**: Git y **GitHub Desktop** son el puente entre dispositivos y cuentas.
- **Persistencia de aprendizaje**: Todo nuevo aprendizaje, preferencia o flujo de trabajo debe guardarse dentro de este repositorio (`.agents/` o `AGENTS.md`), **nunca** únicamente en `~/.gemini/`.

## 3. Estructura de Memoria y Personalización
- `AGENTS.md`: Reglas siempre activas y protocolo de colaboración multi-dispositivo.
- `.agents/memory/HANDOFF.md`: Estado actual de trabajo y relevo inmediato para la siguiente computadora/cuenta.
- `.agents/memory/LESSONS.md`: Lecciones aprendidas, correcciones del usuario, patrones efectivos y errores resueltos.
- `.agents/memory/DECISIONS.md`: Decisiones arquitectónicas y técnicas tomadas (`ADR-001` a `ADR-005`).
- `.agents/memory/SESSIONS.md`: Bitácora cronológica de sesiones entre dispositivos.
- `.agents/skills/`: Habilidades modulares compartidas entre todos los dispositivos (22 skills en total):
  - `build-x86` y `launch-portal-test`: Compilación Win32/x86 de `mod/` (`ApertureRunner`) y prueba directa en Portal (`testchmb_a_00`).
  - `portal-me-true-ingame-v2`: **(Archivada por ADR-006; antes ruta principal en `segundo-intento`, 4 módulos implementados)** Flujo refinado en 4 módulos para jugar 100% dentro de *Mirror's Edge* con geometría 1:1, inyección de inputs en memoria, lectura real de `C_Prop_Portal` y hook `d3d9.dll` (`EndScene`) con Portales 3D, Chell y Viewmodel de la *Portal Gun*.
  - `portal-me-lean-slice`: Flujo de `primer-intento` (sincronización externa de coordenadas en 3 pasos, conservado como base de offsets GOG + Steam).
  - `portal-mirrors-edge-passthrough`: Flujo #1 original (`Edge_p` Azotea).
  - `portal-mirrors-edge-stormdrain`: Flujo #2 avanzado (`Stormdrain_p` Esclusa Vertical con Cubo y Kismet).
  - `colab-sync`, `cross-device-learn`, `session-handoff`: Sincronización Git/GitHub Desktop, aprendizaje continuo y relevo entre sesiones.
  - **Toolkit `universal-modder` (13 skills + CLI `.agents/tools/universal-modder/bin/um.cmd`)**: `mod-any-game`, `mashup-mods`, `game-recon`, `game-research-websearch`, `reverse-engineering`, `port-a-mod`, `asset-pipeline`, `fal-assets`, `game-automation`, `showcase-video`, `publish-mod`, `share-field-notes`, `blitz3d-decomp`.
- `docs/artifacts/`: Carpeta versionada para exportar artefactos importantes que deban consultarse en otras computadoras.
