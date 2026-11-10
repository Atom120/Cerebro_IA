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
| `anim_map.json` | A2 | Asigna cada animación de ME a una secuencia de `FaithAnimState`. Es **provisional** hasta tener la lista real que genera A1 |
| `extract.py` | A1–A4 | Orquesta todo: UE Viewer, SMD, `.qc`, `studiomdl`, instalación y verificación |
| `test_extractor.py` | — | Pruebas con PSK/PSA sintéticos (se registran en `ctest`) |

## Qué falta comprobar con los archivos reales

- Que la convención de rotaciones (conjugar las de los huesos hijos) coincide con lo que exporta UE Viewer para Mirror's Edge. Si el modelo sale con las articulaciones retorcidas en `hlmv`, la culpa está en `smd.ue_local`.
- Los nombres reales de las animaciones: hay que ajustar `anim_map.json` (con `exact`) después de `--step a1`.
- Las texturas: todavía no se convierten a VTF (`vtex.exe`), así que el modelo se verá con la textura de cuadros hasta que se haga.
