---
name: cross-device-learn
description: >-
  Use this skill whenever the user corrects the agent, asks to remember a preference or rule across devices,
  triggers a learning workflow, or when a reusable solution/pattern is discovered that other computers or accounts must know.
---

# Aprendizaje Continuo entre Dispositivos (Cross-Device Learn)

Dado que el directorio `~/.gemini/` es local a cada computadora y cuenta, **todo aprendizaje de Antigravity debe almacenarse dentro del repositorio** para que viaje vía Git / GitHub Desktop hacia el resto de dispositivos.

## ¿Dónde guardar cada tipo de aprendizaje?

Evalúa la naturaleza de lo aprendido y elige el destino adecuado:

| Tipo de Aprendizaje | Archivo / Destino en el Repo | Cuándo elegirlo |
| :--- | :--- | :--- |
| **Preferencia del usuario o contexto global** | [`.agents/memory/MEMORY.md`](../../memory/MEMORY.md) | Idioma, estilo de código general, stack del proyecto, convenciones permanentes. |
| **Lección puntual, bug resuelto o corrección** | [`.agents/memory/LESSONS.md`](../../memory/LESSONS.md) | Errores que no deben repetirse, comandos que fallan en cierto SO, trucos o soluciones específicas. |
| **Decisión de arquitectura o diseño (ADR)** | [`.agents/memory/DECISIONS.md`](../../memory/DECISIONS.md) | Elección de librerías, estructura de carpetas, contratos de API o cambios de diseño importantes. |
| **Flujo de trabajo o procedimiento repetible** | `.agents/skills/<nombre-skill>/SKILL.md` | Procesos paso a paso (despliegues, pruebas, generación de archivos, scripts recurrentes). |
| **Regla estricta de cumplimiento obligatorio** | [`AGENTS.md`](../../../AGENTS.md) | Restricciones críticas que toda instancia de Antigravity debe obedecer en cada turno. |

## Procedimiento Paso a Paso

1. **Clasifica el aprendizaje** usando la tabla anterior.
2. **Verifica que no sea específico de una sola máquina**:
   - Si el aprendizaje depende de un sistema operativo (ej. Windows vs macOS/Linux), documenta explícitamente a qué entorno aplica.
   - Nunca guardes rutas absolutas con nombres de usuario locales (`C:\Users\...`) como si fueran universales; usa variables o rutas relativas al repositorio.
3. **Escribe el aprendizaje de forma concisa**:
   - Recuerda que `AGENTS.md` (incluyendo los archivos inyectados vía `@[...](...)`) tiene un límite de **24 KB** por archivo.
   - En [`.agents/memory/LESSONS.md`](../../memory/LESSONS.md), agrega una nueva línea al final con el formato:
     `- **[YYYY-MM-DD | Categoría]** Descripción clara y accionable de la lección.`
   - Si `.agents/memory/LESSONS.md` supera los 15 KB en el futuro, consolida las lecciones antiguas o muévelas a una Skill específica en `.agents/skills/`.
4. **Confirma al usuario**:
   - Indica qué archivo actualizaste para que sepa que el conocimiento quedará sincronizado en su próximo **Commit & Push** desde GitHub Desktop.
