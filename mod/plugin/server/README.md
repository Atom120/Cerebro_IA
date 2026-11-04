# mod/plugin/server

- **Dueño:** Zarek · **Tareas:** Z1–Z5 (ver `PLAN_EQUIPO.md`)
- DLL plugin de servidor (VSP) x86 para Portal build 19017868: carga, interfaces, hook de `IGameMovement::ProcessMovement`, `ITraceProvider` del servidor. Zarek también es dueño de `mod/plugin/main.cpp` y `mod/plugin/CMakeLists.txt` (la DLL `aperture_runner.dll`).
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `mod\build.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
- **2026-11-04:** Z2 y Z5 (y las correcciones de Z4) los hizo **Sebas**. Ver `docs/contratos/source_2013_abi.md`. No los modifiques sin hablar con Sebas.
