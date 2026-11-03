# mod/tools/extractor

- **Dueño:** Antonio · **Tareas:** A1–A4 (ver `PLAN_EQUIPO.md`)
- Extractor que cada usuario ejecuta en su PC: copia propia de Mirror's Edge → UEViewer → Blender → studiomdl → `<Portal>/portal/custom/aperture_runner/`. Contrato: `docs/contratos/asset_manifest.json`. Nunca escribe archivos del juego dentro del repo.
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `mod\build.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
