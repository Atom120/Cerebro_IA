# Memoria Compartida del Proyecto (Cross-Device Memory)

> Este archivo mantiene el contexto global y permanente entre todas las instancias de **Antigravity** (diferentes computadoras, sistemas operativos y cuentas).

## 1. Propósito del Repositorio
- **Nombre**: `AntigravityColab`
- **Objetivo**: Espacio de trabajo colaborativo sincronizado mediante **Git / GitHub Desktop** donde múltiples instancias de Antigravity mantienen el contexto continuo, comparten habilidades (`skills`), reglas (`AGENTS.md`) y acumulan aprendizaje entre dispositivos.
- **Proyecto Activo**: Mod *Pass-Through* para jugar con el personaje y físicas de **Portal** (Chell + Portal Gun en Source Engine) dentro de una sección acotada de **Mirror's Edge** (Unreal Engine 3).

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
- `.agents/skills/`: Habilidades modulares compartidas entre todos los dispositivos:
  - `portal-me-true-ingame-v2`: **(Ruta Principal Activa en `segundo-intento`)** Flujo refinado en 4 módulos para jugar 100% dentro de *Mirror's Edge* con geometría 1:1, inyección de inputs en memoria, lectura real de `C_Prop_Portal` y hook `d3d9.dll` (`EndScene`) con Portales 3D, Chell y Viewmodel de la *Portal Gun*.
  - `portal-me-lean-slice`: Flujo de `primer-intento` (sincronización externa de coordenadas en 3 pasos, conservado como base de offsets GOG + Steam).
  - `portal-mirrors-edge-passthrough`: Flujo #1 original (`Edge_p` Azotea).
  - `portal-mirrors-edge-stormdrain`: Flujo #2 avanzado (`Stormdrain_p` Esclusa Vertical con Cubo y Kismet).
- `docs/artifacts/`: Carpeta versionada para exportar artefactos importantes que deban consultarse en otras computadoras.
