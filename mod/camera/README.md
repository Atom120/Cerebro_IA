# mod/camera

- **Dueño:** Gonzalo · **Tareas:** G3 (ver `PLAN_EQUIPO.md`)
- Cámara estilo Mirror's Edge (roll, balanceo, FOV, sacudidas) en C++ puro con pruebas. Entrada: `ar::FaithState` por tick.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `moduild.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
