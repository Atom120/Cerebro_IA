# mod/plugin/server

- **Dueño:** Zarek · **Tareas:** Z1–Z5 (ver `PLAN_EQUIPO.md`)
- DLL plugin de servidor (VSP) x86 para Portal build 19017868: carga, interfaces, hook de `IGameMovement::ProcessMovement`, `ITraceProvider` del servidor. Zarek también es dueño de `mod/plugin/main.cpp` y `mod/plugin/CMakeLists.txt` (la DLL `aperture_runner.dll`).
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `moduild.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
