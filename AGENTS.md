# Protocolo de Colaboración Multi-Dispositivo y Aprendizaje Continuo (AntigravityColab)

Estás operando dentro de un repositorio compartido diseñado para que múltiples instancias de **Antigravity** (en distintas computadoras, sistemas operativos y cuentas de usuario) trabajen en equipo, mantengan el contexto sin pérdidas y aprendan de forma acumulativa a través de **Git / GitHub Desktop**.

---

## 1. Reglas Fundamentales de Sincronización y Contexto

1. **Lectura de Contexto al Iniciar (Warm Start)**:
   - Revisa siempre el estado actual en `.agents/memory/HANDOFF.md` y `.agents/memory/MEMORY.md` (incluidos abajo) antes de proponer cambios.
   - Si el usuario retoma una tarea o menciona cambios hechos en otra computadora, verifica el estado del repositorio con `git status` y `git log -n 5 --oneline`.

2. **Aprendizaje Multi-Dispositivo (Cross-Device Learning)**:
   - **Prohibido guardar aprendizajes solo localmente**: Los directorios `~/.gemini/` y `<appDataDir>/brain/` son locales a una sola máquina/cuenta y **no** se sincronizan con otras computadoras.
   - Cada vez que el usuario te corrija, exprese una preferencia, o descubras un patrón/solución importante:
     - Regístralo inmediatamente en `.agents/memory/LESSONS.md` o `.agents/memory/MEMORY.md`.
     - Si es una decisión técnica o de arquitectura, documéntala en `.agents/memory/DECISIONS.md`.
     - Si es un procedimiento repetible de varios pasos, crea o actualiza una Skill en `.agents/skills/<nombre-skill>/SKILL.md`.

3. **Relevo Continuo entre Computadoras y Cuentas (Handoff)**:
   - Al completar una tarea significativa, o cuando el usuario indique que va a guardar cambios, hacer commit en GitHub Desktop o cambiar de computadora/cuenta:
     1. Actualiza `.agents/memory/HANDOFF.md` con la fecha, qué se hizo, el estado actual de las tareas y los próximos pasos exactos.
     2. Añade una línea de resumen al final de `.agents/memory/SESSIONS.md`.
     3. Si creaste un artefacto importante en tu directorio local `<appDataDir>/brain/<conversation-id>/` que otra computadora necesitará leer, cópialo a `docs/artifacts/`.

4. **Compatibilidad Multiplataforma**:
   - No asumas rutas absolutas de un usuario o sistema operativo específico (ej. `C:\Users\Nombre\...` o `/Users/nombre/...`). Usa siempre rutas relativas a la raíz del repositorio.
   - Mantén el árbol de trabajo limpio y listo para que el usuario pueda sincronizar con un clic desde **GitHub Desktop** (Fetch / Pull / Commit / Push).

5. **Integridad del Vertical Slice y Validación Frente a Referencias Reales**:
   - **Prohibido recortar el valor central por simplificar la arquitectura**: Un *Vertical Slice* reduce la cantidad de escenario (ej. 1 azotea en vez de un nivel entero), pero **jamás** debe eliminar la experiencia visual y de control que define al proyecto (jugar en una sola ventana viendo al personaje, arma y mecánicas del Juego A integrados dentro del Mundo B).
   - **Contrastación con referencias reales**: Antes de dar por buena una arquitectura o simplificación, compárala explícitamente contra implementaciones reales existentes en internet (ej. `libsm64`, hooks `d3d9.dll` `EndScene`, *Stencil/Render-to-Texture* de portales).
   - **Tests de consola $\neq$ Experiencia de usuario**: Que un script pase `--self-test` en terminal solo valida las matemáticas internas; nunca afirmes que el flujo está completo sin validar la experiencia visual e interactiva en una sola ventana.

---

## 2. Contexto Dinámico Inyectado del Repositorio

### Memoria Global del Proyecto
@[Memoria Compartida](.agents/memory/MEMORY.md)

### Estado de Relevo Activo (Handoff)
@[Estado de Relevo Activo](.agents/memory/HANDOFF.md)

### Lecciones Aprendidas entre Dispositivos
@[Lecciones Aprendidas](.agents/memory/LESSONS.md)
