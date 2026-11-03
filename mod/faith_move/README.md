# mod/faith_move

- **Dueño:** Peña · **Tareas:** P1–P5 (ver `PLAN_EQUIPO.md`)
- Librería de movimiento de Faith en C++ puro (sin Source ni UE3). Implementa `ar::IFaithMovement` y define `ar::CreateFaithMovement` (`mod/include/ar/faith_movement.h`). Prueba con `ar::testing::BoxWorld`.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `mod\build.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
