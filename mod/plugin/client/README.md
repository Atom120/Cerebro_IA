# mod/plugin/client

- **Dueño:** Gonzalo · **Tareas:** G1, G2, G4 (ver `PLAN_EQUIPO.md`)
- Lado cliente dentro de la misma DLL: predicción (`GameMovement001` de client.dll), `CreateMove` y dibujo de Faith en primera persona.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `moduild.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
