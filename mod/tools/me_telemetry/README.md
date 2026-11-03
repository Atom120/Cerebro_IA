# mod/tools/me_telemetry

- **Dueño:** Benja · **Tareas:** B2 (ver `PLAN_EQUIPO.md`)
- Grabador de SOLO LECTURA de Mirror's Edge: posición, velocidad y `MovementState` a CSV en `qa/telemetria/`. Offsets de GOG en `.agents/memory/LESSONS.md`; verificarlos en vivo.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `moduild.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
