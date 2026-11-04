# mod/plugin/client

- **Dueño:** Gonzalo · **Tareas:** G1, G2, G4 (ver `PLAN_EQUIPO.md`)
- Lado cliente dentro de la misma DLL: predicción (`GameMovement001` de client.dll), `CreateMove` y dibujo de Faith en primera persona.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `mod\build.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
- **2026-11-04:** G1 lo hizo **Sebas** (`client_movement_hook.*`, `docs/contratos/portal_client_offsets.md`). Con `cl_predict 0`, G2 no aplica. No lo modifiques sin hablar con Sebas.
