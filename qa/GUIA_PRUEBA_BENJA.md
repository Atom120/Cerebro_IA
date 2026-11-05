---
tipo: guia
proyecto: ApertureRunner
actualizado: 2026-11-05
responsable: Sebas
tags: [qa, apertureRunner, benja, prueba]
---
# Guía: probar ApertureRunner en la PC de Benja

Benja, tu PC es la única del equipo con **Portal y Mirror's Edge**. Esta guía cubre tres cosas: probar el mod en Portal (automático y a mano), grabar las trazas reales de Mirror's Edge (B2b) y subir los resultados. Sigue los pasos en orden. Si algo falla, copia el mensaje exacto en el PR.

## 0. Requisitos
- **Portal** (Steam, build **19017868**). Mira `steamapps/appmanifest_400.acf` → `"buildid"`. Si tu build es otra, avísalo antes de seguir.
- **Steam abierto.**
- **Python 3** (lo usas ya para `record_me_csv.py`).
- **Opcional:** Visual Studio 2026 con C++ y CMake, para compilar. Sin él puedes usar la DLL ya compilada del release (paso 2b).
- **Mirror's Edge GOG v1.01** solo para el paso 4.

## 1. Bajar `main`
En **GitHub Desktop**: *Current branch* → `main` → *Fetch origin* → *Pull origin*. Tiene que aparecer el merge `bf7df5d` o uno posterior.

## 2. Prueba automática en Portal (≈ 6 min, no toques el teclado)
Cierra Portal, abre PowerShell en la carpeta del repo y elige:

**2a. Con Visual Studio (compila y prueba):**
```
powershell -ExecutionPolicy Bypass -File qa\scripts\probar_todo.ps1
```
**2b. Sin Visual Studio:** descarga `ApertureRunner-2026-11-05.zip` del release **v0.3-h3** del repo (GitHub → *Releases*), extráelo y ejecuta:
```
powershell -ExecutionPolicy Bypass -File qa\scripts\probar_todo.ps1 -Dll "C:\ruta\donde\extrajiste\aperture_runner.dll"
```
Qué hace el script:
1. Crea `config/paths.local.json` si no lo tienes, buscando Portal en Steam.
2. Abre Portal varias veces y lo mueve solo por consola: sprint, salto, wallrun, wallclimb, vault, mantle, slide, portal, ascensor, FOV...
3. Al final escribe `qa/resultados/H2_auto_<tu-pc>.md` y `qa/resultados/H3_auto_<tu-pc>.md`.
4. Tus ajustes de `addons/aperture_runner.cfg` quedan como estaban.

En mi PC da **H2 9/9** y **H3 15/15**. Si en la tuya algo da FAIL, es justo lo que necesitamos saber.

## 3. Jugar a mano (lo más importante: cómo se siente)
1. Instala: abre `instalar.bat` del zip, o simplemente usa lo que dejó el paso 2 en `portal/addons`.
2. Abre Portal → *Nueva partida* (`testchmb_a_00`) → consola (`~`) → `ar_enable 1` (se recuerda para la próxima vez).
3. Prueba y apunta qué se siente raro:

| Qué probar | Cómo | Dónde |
|---|---|---|
| Sprint con inercia | adelante mantenido ~1.5 s | pasillo de la sala 1 |
| Salto, coil y Skill Roll | saltar; agacharse en el aire; agacharse justo antes de aterrizar desde alto | cualquier sitio |
| Wallrun | correr junto a una pared y saltar | paredes del pasillo (la cámara se inclina) |
| Wallclimb y giro | saltar de frente contra una pared; recargar (R) mientras sube; saltar | paredes altas |
| Vault y mantle | saltar corriendo hacia la plataforma del botón; saltar hacia la repisa junto a la cama | sala 1 y sala inicial |
| Slide | agacharse mientras esprintas | pasillo |
| Giro de 180° | recargar (R) | cualquier sitio |
| Portales con inercia | esprintar hacia un portal abierto | portal de la sala inicial |
| Desactivar | `ar_enable 0`: vuelve el movimiento de Chell | — |

Comandos útiles: `ar_status` (estado), `ar_camera 0|1` (cámara de Faith), `ar_debug 1` (registro en la consola).

## 4. Mirror's Edge: grabar trazas reales (B2b, solo tú puedes)
Con esto Peña ajusta el movimiento con datos reales (P5). Los CSV que hay ahora son sintéticos.
1. En `config/paths.local.json` pon las rutas de tu Mirror's Edge GOG (`mirrors_edge_root`, `mirrors_edge_exe`, `mirrors_edge_cooked_pc`).
2. Compila (paso 2a) para tener `build/x86/tools/me_telemetry/Release/me_telemetry_recorder.exe`.
3. Abre Mirror's Edge, entra en un nivel y deja a Faith quieta. Comprueba que se leen bien los datos:
   ```
   python mod\tools\me_telemetry\record_me_csv.py --probe
   ```
4. Graba cada movimiento con un archivo `*_live.csv` (solo lectura, no toca el juego):
   ```
   python mod\tools\me_telemetry\record_me_csv.py --record qa\telemetria\sprint_start_live.csv --duration 6 --hz 120
   ```
   Repite con `jump_live`, `wallrun_left_live`, `wallrun_right_live`, `wallclimb_live`, `ledge_grab_live`, `slide_live`, `hard_landing_live` y `skill_roll_live`. Empieza a hacer el movimiento justo al lanzar el comando.
5. Comprueba los archivos:
   ```
   python mod\tools\me_telemetry\record_me_csv.py --verify qa\telemetria
   ```

## 5. Subir los resultados
GitHub Desktop → *New branch* `benja/B3-prueba-<tu-pc>` → marca solo:
- `qa/resultados/H2_auto_*.md` y `H3_auto_*.md`;
- tus notas del paso 3 (añádelas al final de `qa/resultados/H3.md` en una sección «Prueba en la PC de Benja»);
- los `qa/telemetria/*_live.csv` del paso 4.

Luego haz *Commit*, *Push* y abre un PR. **No subas** `config/paths.local.json`, archivos de los juegos (`.upk`, `.dll`, `.bsp`...) ni la carpeta `build/`.

## Problemas comunes
- **«Portal se cerró durante la prueba»:** cerraste Portal o le quitaste el foco mientras corría. Vuelve a empezar sin tocar nada.
- **Todo FAIL desde H2-01 (autotest Z4):** probablemente tu build de Portal es otra. Pega la línea `Autotest Z4` de `portal/console.log` en el PR.
- **`mod\build.bat` falla:** usa la opción 2b con la DLL del release.
- **El antivirus bloquea la DLL:** es una DLL propia sin firmar; permítela para la carpeta `portal/addons`.
