# qa

- **Dueño:** Benja · **Tareas:** B3, B5 (ver `PLAN_EQUIPO.md`)
- Pruebas por hito (`qa/resultados/H1.md`…), telemetría (`qa/telemetria/`) y demos. La checklist del mapa está en `docs/contratos/testchmb_a_00_checklist.md` (B1).
- Para entrar en el build, crea aquí un `CMakeLists.txt` (el `mod/CMakeLists.txt` lo detecta solo), enlaza `ar_contracts` y registra tus pruebas con `add_test`. Compila con `mod\build.bat`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.
- **2026-11-04:** B3 de H1 y H2 lo hizo **Sebas**. `qa/scripts/h2_smoke.ps1` repite H2 en Portal sin tocar el teclado. No lo modifiques sin hablar con Sebas.
