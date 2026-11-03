---
name: session-handoff
description: >-
  Use this skill when finishing a task or session, before the user commits/pushes in GitHub Desktop,
  or when the user says they are switching to another computer or account.
---

# Relevo de Sesión entre Computadoras y Cuentas (Session Handoff)

Esta skill empaqueta el contexto de la conversación actual para que otra instancia de Antigravity (en otra computadora o con otra cuenta) pueda continuar el trabajo sin perder el hilo.

## Pasos para realizar el Handoff

1. **Exportar artefactos locales importantes (si existen)**:
   - Los artefactos creados en `<appDataDir>/brain/<conversation-id>/` (planes de implementación, reportes, diagramas) viven solo en esta computadora.
   - Si durante la sesión creaste un artefacto que la otra computadora necesitará consultar, cópialo a [`docs/artifacts/`](../../../docs/artifacts/) con un nombre descriptivo (ej. `docs/artifacts/2026-11-01-plan-arquitectura.md`).

2. **Actualizar [`.agents/memory/HANDOFF.md`](../../memory/HANDOFF.md)**:
   - Actualiza la sección **Última Actualización** con la fecha actual y el identificador del dispositivo/entorno.
   - En **Resumen de la Última Sesión**, resume en 2-4 puntos qué se logró y por qué.
   - En **Tareas en Curso**, marca con `[x]` lo completado y deja con `[ ]` lo pendiente.
   - En **Próximos Pasos Sugeridos**, escribe instrucciones claras para la siguiente instancia de Antigravity (archivos exactos a tocar y siguiente objetivo).
   - En **Notas y Bloqueos**, advierte sobre cualquier problema pendiente, test fallando o dependencia externa.

3. **Registrar la sesión en [`.agents/memory/SESSIONS.md`](../../memory/SESSIONS.md)**:
   - Añade **una sola línea al final del archivo** (para aprovechar `merge=union` sin conflictos de Git):
     `- **YYYY-MM-DD (Dispositivo - Resumen corto)**: Qué se hizo y archivos principales modificados.`

4. **Verificar aprendizajes nuevos**:
   - Si durante la sesión descubriste algo nuevo sobre el proyecto o recibiste correcciones del usuario, regístralo en [`.agents/memory/LESSONS.md`](../../memory/LESSONS.md) siguiendo la skill `cross-device-learn`.

5. **Listo para GitHub Desktop**:
   - Ejecuta `git status -sb` para verificar que todos los archivos modificados estén listos y sugiere al usuario un mensaje de commit claro para **GitHub Desktop**.
