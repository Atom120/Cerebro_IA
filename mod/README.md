# mod/ — código del mod ApertureRunner

Mirror's Edge dentro de Portal 1 (`testchmb_a_00`). Plan, reparto y dependencias: [`PLAN_EQUIPO.md`](../PLAN_EQUIPO.md).

## Compilar y probar

```
mod\build.bat
```

Compila para **Win32/x86** (Portal es de 32 bits) y ejecuta todas las pruebas. CI hace lo mismo en cada PR (`.github/workflows/mod-build.yml`).

## Carpetas

| Carpeta | Dueño | Qué es |
|---|---|---|
| `include/ar/` | Sebas | **Contratos v1**: `units.h`, `trace_provider.h`, `faith_movement.h`, `testing/box_world.h` |
| `core/` | Sebas | Núcleo heredado de `aperture-runner-mashup`: `math.h` (conversión UE3↔Source, portales, wallrun, roll, Skill Roll), memoria compartida y puentes |
| `tests/` | Sebas | Pruebas de los contratos (incluye que `asset_manifest.json` coincida con las animaciones) |
| `faith_move/` | Peña | Librería de movimiento de Faith |
| `plugin/server/` + `plugin/main.cpp` | Zarek | DLL `aperture_runner.dll` (VSP) y hook de servidor |
| `plugin/client/` | Gonzalo | Predicción y dibujo en cliente |
| `camera/` | Gonzalo | Cámara estilo Mirror's Edge |
| `tools/extractor/`, `assets_src/` | Antonio | Modelo y animaciones de Faith desde la copia propia de ME |
| `tools/me_telemetry/` | Benja | Grabador de solo lectura de Mirror's Edge |

Cada dueño añade un `CMakeLists.txt` en su carpeta; `mod/CMakeLists.txt` lo detecta sin editarlo. Enlaza `ar_contracts`.

## Cómo encaja

```
Portal (hl2.exe, 32 bits)
 └─ aperture_runner.dll (plugin/)
     ├─ servidor: hook IGameMovement::ProcessMovement ─┐
     ├─ cliente:  predicción con la misma llamada ─────┼─> faith_move (IFaithMovement::Step)
     │                                                 │      └─ ITraceProvider (trazas del motor)
     └─ cámara + brazos de Faith  <── FaithState ──────┘
```
