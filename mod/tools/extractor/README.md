# mod/tools/extractor

- **Dueño:** Antonio · **Tareas:** A1–A4 (ver `PLAN_EQUIPO.md`). Los scripts los preparó Sebas el 2026-11-09.
- Extractor que cada usuario corre en su PC: toma su propia copia de Mirror's Edge, pasa por UE Viewer, luego por la conversión a SMD (Python) y luego por `studiomdl`, y deja el resultado en `<Portal>/portal/custom/aperture_runner/`. El contrato es `docs/contratos/asset_manifest.json`. Nunca escribe archivos del juego dentro del repo: los archivos de trabajo van a `%TEMP%\aperture_runner_extract`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: los cambios se piden a Sebas.

## Uso

```
python mod/tools/extractor/extract.py --check     # comprueba rutas y herramientas, no cambia nada
python mod/tools/extractor/extract.py             # todo (A4): A1 -> A2 -> A3 -> verificación
python mod/tools/extractor/extract.py --step a1   # solo exportar con UE Viewer y escribir docs/contratos/faith_esqueleto.md
```

Antes de usarlo:
1. `config/paths.local.json` con `mirrors_edge_cooked_pc`, `mirrors_edge_exe`, `portal_root`, `source_sdk_bin` y `umodel_exe`.
2. **UE Viewer** (`umodel.exe`), descargado de https://www.gildor.org/en/projects/umodel y puesto en `tools/umodel/`. Esa carpeta está ignorada por Git.

No hace falta Blender: la conversión de PSK/PSA a SMD se hace en Python puro (`smd.py`). Esto cambia lo que decía el plan original de A2, que proponía un script de Blender con el addon `io_scene_psk_psa`. Así hay una herramienta menos que instalar en cada PC.

## Archivos

| Archivo | Tarea | Qué hace |
|---|---|---|
| `ue_psk.py` | A1/A2 | Lee los `.psk`/`.psa` de ActorX (y los escribe, solo para las pruebas) |
| `smd.py` | A2 | Convierte de UE3 a Source: ÷2.54, refleja el eje Y, conjuga las rotaciones de los huesos hijos, invierte el orden de los triángulos y voltea la V |
| `anim_map.json` | A2 | Asigna cada animación de ME a una secuencia de `FaithAnimState`. Usa nombres exactos revisados contra la lista real de A1, y además qué malla va en cada modelo |
| `extract.py` | A1–A4 | Orquesta todo: UE Viewer, SMD, `.qc`, `studiomdl`, instalación y verificación |
| `test_extractor.py` | — | Pruebas con PSK/PSA sintéticos (se registran en `ctest`) |

## Estado (2026-11-09, ME GOG v1.01)

`python mod/tools/extractor/extract.py` funciona de punta a punta en el PC de Sebas, con UE Viewer en `tools/umodel/umodel_win32/`:
- **A1:** exporta 5 mallas y 281 animaciones; la lista está en `docs/contratos/faith_esqueleto.md`. La etiqueta de UE Viewer para Mirror's Edge es `-game=medge`.
- **A2:** `faith_arms` usa `SK_UpperBody` y `faith_body` usa `SK_LowerBody`; cada una tiene 74 huesos y las 24 secuencias asignadas con nombres exactos (`anim_map.json` v2).
- **A3:** `studiomdl` compila los dos `.mdl` y `vtex` convierte 8 texturas (difusa y normal) de 4 materiales.
- **Esqueleto comprobado con números:** en la pose de referencia, los vértices de los brazos quedan a 1 su de su hueso (mediana); con la convención contraria quedarían a 34 su. La convención de `smd.ue_local` es la correcta.

## Qué falta comprobar a ojo

- Abrir `faith_arms.mdl` y `faith_body.mdl` en `hlmv` y revisar las animaciones: que no salgan giradas, que la escala esté bien y que los materiales se vean.
- `MI_SHtest` (material de prueba en `SK_LowerBody`) y la transparencia de la piel (`Female_1p_Trans`) todavía no se convierten.
- Algunas asignaciones son aproximadas: `wallclimb_jump` usa `JumpTurnFly` y `ledge_climb_up` usa `HangHeaveUp`.
