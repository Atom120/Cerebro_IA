# mod/camera

- **Dueño:** Gonzalo · **Tareas:** G3 (ver `PLAN_EQUIPO.md`)
- Cámara estilo Mirror's Edge (roll, balanceo, FOV, sacudidas) en C++ puro con pruebas. Entrada: `ar::FaithState` por tick.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `mod\build.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
- **2026-11-04:** G3 lo hizo **Sebas**: `UpdateCamera()` (inclinación `exp(-λ·dt)`, FOV, balanceo, sacudidas, Skill Roll y giro de 180°), con 30 pruebas. Falta aplicarlo a la vista (G4). No lo modifiques sin hablar con Sebas.
