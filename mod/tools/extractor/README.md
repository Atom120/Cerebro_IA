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
| `smd.py` | A2 | Convierte de UE3 a Source: ÷2.54, conjuga las rotaciones de los huesos hijos, pone el modelo de pie con una rotación (x, y, z) → (z, x, y), sin reflejar, y voltea la V |
| `anim_map.json` | A2 | Asigna cada animación de ME a una secuencia de `FaithAnimState`. Usa nombres exactos revisados contra la lista real de A1, y además qué malla va en cada modelo |
| `extract.py` | A1–A4 | Orquesta todo: UE Viewer, SMD, `.qc`, `studiomdl`, instalación y verificación |
| `test_extractor.py` | — | Pruebas con PSK/PSA sintéticos (se registran en `ctest`) |

## Estado (2026-11-09, ME GOG v1.01)

`python mod/tools/extractor/extract.py` funciona de punta a punta en el PC de Sebas, con UE Viewer en `tools/umodel/umodel_win32/`:
- **A1:** exporta 5 mallas y 281 animaciones; la lista está en `docs/contratos/faith_esqueleto.md`. La etiqueta de UE Viewer para Mirror's Edge es `-game=medge`.
- **A2:** `faith_arms` usa `SK_UpperBody` y `faith_body` usa `SK_LowerBody`; cada una tiene 74 huesos y las 24 secuencias asignadas con nombres exactos (`anim_map.json` v2).
- **A3:** `studiomdl` compila los dos `.mdl` y `vtex` convierte 8 texturas (difusa y normal) de 4 materiales.
- **Esqueleto comprobado con números:** en la pose de referencia, los vértices de los brazos quedan a 1 su de su hueso (mediana); con la convención contraria quedarían a 34 su. La convención de `smd.ue_local` es la correcta.
- **Orientación comprobada:** en el `.psk`, Faith mira hacia +Z, su izquierda está en +X y arriba es +Y. La primera versión reflejaba el eje Y, lo que la dejaba acostada y con la izquierda a la derecha; ahora se rota sin reflejar. En Source la cabeza queda a z ≈ 61, los dedos de los pies hacia +X, la mano izquierda en +Y y `CameraJoint` a 62–65 su (los ojos de Source están a 64). Se revisó también con renders del sprint.

## Faith en tercera persona (2026-11-10)

- `faith_player.mdl` (`player_3p` en el manifiesto) sale de `SK_TKY_Crim_Fixer` (108 huesos) con las animaciones de `AS_F3P_Unarmed`. Además de las 24 secuencias de Faith, lleva las 13 que pide el jugador de Portal por actividad (`portal_Idle` = `ACT_MP_STAND_IDLE`, `portal_Run` = `ACT_MP_RUN`…), que se leyeron de `models/player/chell_animations.mdl`.
- `RightWeapon` se renombra a `weapon_bone`, para que el arma de portales se enganche a la mano de Faith.
- Las mallas de UE miran hacia +Y y los modelos de Source hacia +X: `model_yaw` (`player_3p`: 90) añade `$origin 0 0 0 90` al `.qc`, que gira la malla y todas las secuencias. Sin él, Faith miraba 90° a un lado (visto a través de un portal, 2026-11-10).
- La cabeza es la de baja resolución. Sus ojos y su boca van pintados, así que se quitan los globos oculares y las pestañas de la cara de alta resolución (sobresalían). La boca (`material_0`) se pinta color piel y el pelo usa `Faith_Hair_C_shaded` con alfa.

## Revisión a ojo y pendientes

- ✅ Revisado en `hlmv` el 2026-11-10: brazos (piel, manga del codo, guante rojo y tatuaje en el brazo derecho) y piernas (pantalón blanco, top negro y zapatillas rojas) con textura; `sprint` y `slide` se animan bien.
- `MI_SHtest` (material de prueba en `SK_LowerBody`) y la transparencia de la piel (`Female_1p_Trans`) todavía no se convierten.
- Algunas asignaciones son aproximadas: `wallclimb_jump` usa `JumpTurnFly` y `ledge_climb_up` usa `HangHeaveUp`.
