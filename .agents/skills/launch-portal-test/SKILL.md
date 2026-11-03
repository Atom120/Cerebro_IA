---
name: launch-portal-test
description: Copia la DLL del mod (aperture_runner.dll) a Portal, crea el .vdf del plugin y abre Portal en testchmb_a_00 en ventana para probar. Úsala para cualquier prueba dentro del juego.
---

# launch-portal-test

## Requisitos
- Steam abierto y Portal (app 400) instalado, **build 19017868** (`steamapps/appmanifest_400.acf` → `buildid`).
- `config/paths.local.json` con `paths.portal_root` (copia de `config/paths.example.json`, no se sube).
- Haber compilado con la skill `build-x86` (debe existir `aperture_runner.dll` dentro de `build\x86\`).

## Pasos
1. Ejecuta desde la raíz del repo:
   ```
   powershell -ExecutionPolicy Bypass -File .agents/skills/launch-portal-test/scripts/launch-portal-test.ps1
   ```
   Opciones: `-Map testchmb_a_00` (por defecto), `-Insecure` (añade `-insecure`), `-NoLaunch` (solo copia).
2. El script copia la DLL a `<Portal>/portal/addons/aperture_runner.dll`, escribe `addons/aperture_runner.vdf` y abre Portal en ventana con `+developer 1 +map <Map>`.
3. En la consola de Portal comprueba con `plugin_print` que aparece el plugin.
4. Para desinstalar: borra solo esos dos archivos de `<Portal>/portal/addons/`.

## Reglas
- No toques otros archivos de la instalación de Portal.
- Anota el resultado de la prueba según `docs/contratos/testchmb_a_00_checklist.md` cuando exista.
