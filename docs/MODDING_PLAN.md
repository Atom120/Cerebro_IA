# ApertureRunner: plan de modding (recon de universal-modder)

> Generado con la skill `game-recon` de universal-modder el 2026-11-05 (Sebas, Claude Code). Complementa a
> [`PLAN_EQUIPO.md`](../PLAN_EQUIPO.md) y a `.agents/memory/HANDOFF.md`; no los sustituye.

## Portal (anfitrión)
- Instalación: `C:\Program Files (x86)\Steam\steamapps\common\Portal` (Steam 400), build 19017868.
- Motor: Source 1 (Source 2013 Singleplayer), `hl2.exe` x86. Host: Windows 10.0.26200 x64 (nativo, sin capa de compatibilidad).
- Anticheat / online: ninguno. Un jugador, sin riesgo de baneo.
- `um scan` marca "ASI loader": es un falso positivo (`bin/mssmp3.asi` y `bin/mssvoice.asi` son de Miles Sound System, no un cargador de mods).
- Ruta de la comunidad: `addons/` (plugins de servidor VSP) y `custom/`. Es la que usamos: `portal/addons/aperture_runner.{dll,vdf,cfg}`.
- Ruta elegida: plugin VSP x86 que sustituye `IGameMovement::ProcessMovement` por `faith_move` (ruta B, ADR-006).

## Mirror's Edge (invitado)
- **No está instalado** en este PC (ni Steam, Epic, Xbox, EA app ni GOG; comprobado el 2026-11-05).
- Motor: Unreal Engine 3 (2008). Bloquea A1–A4 (assets de Faith), B2b (trazas reales), P5 (ajuste) y los brazos de G4.

## Precedentes en la base de conocimiento de universal-modder
No hay ninguna nota de Portal 1, de Mirror's Edge (2008) ni de movimiento de otro juego dentro de Source.
Esto es lo más cercano y lo que aporta:

1. **Portalcraft: Minecraft dentro de Portal 2** (`knowledge/games/portal-2/portalcraft-minecraft-inside-portal-2.md`).
   - **Leer las variables de red por nombre, nunca con offsets fijos**: recorrer la lista de clases y sus tablas
     (`RecvTable` en el cliente; en el servidor, `ServerClass`/`SendTable`). Nosotros tenemos 5 offsets fijos para
     la build 19017868 en `mod/plugin/server/source_interfaces.h` (`m_iFOV` 0xB38, `m_vecViewOffset` 0x320,
     `m_bActivated` 0x56A…). Si Valve actualiza Portal, se rompen sin avisar.
   - La cámara que sale en pantalla suele ser la del fotograma anterior. Tenerlo en cuenta en G4 si algo "tiembla".
   - El índice de entidad de un handle es `handle & 0xFFF` (útil para `m_hLinkedPortal`).
2. **Batman de Arkham Knight en Risk of Rain 2** (`knowledge/games/risk-of-rain-2/arkham-knight-batman-survivor.md`).
   Es UE3 → UEViewer (umodel) → Blender con PSK/PSA, el mismo camino de A1–A3 (Antonio):
   - La descarga directa de umodel devuelve una página HTML de 9 KB: hay que enviar el `Referer` de gildor.org.
   - El importador PSK/PSA (io_scene_psk_psa) aplica la escala como escala de objeto: la escala de traslación del
     PSA debe coincidir con la del rig, o las animaciones "explotan" o se hunden.
   - Huesos cuyo padre falta en el PSA reciben la convención de cuaternión del hueso raíz: corregirlo a mano.
   - umodel solo exporta LOD0: si hace falta un LOD menor, reducirlo en Blender.
3. Técnica general **"motor invitado reconstruido dentro de un anfitrión"**
   (`knowledge/techniques/rebuilt-guest-engine-inside-a-host.md`): es la familia de nuestra ruta B.

## Revisión de `levels/` (2026-11-09)
- Los 4 `.vmf` y los 2 `.bsp` de la ruta archivada **no contienen contenido de Mirror's Edge**. Son 11–15 bloques
  hechos a mano con materiales de serie de Source (`CONCRETE_MODULAR_*`, `METALWALL048B`, `TOOLSSKYBOX`).
- El pakfile de cada `.bsp` solo tiene lo que genera el compilador: cubemaps por defecto y un `.vmt` parcheado.
- Lo único que viene de Mirror's Edge son coordenadas (puntos de inicio y anclas de `Edge_p`) en los `slice_manifest.json`.
  Son medidas, no assets. Sin acción necesaria.

## Pendiente que sale del recon
- [ ] Resolver los offsets de `CPortal_Player`/`CProp_Portal` por nombre al cargar (dejando los actuales como
      comprobación). Propuesto, no hecho.
- [ ] Instalar Mirror's Edge (o que Antonio/Benja hagan A1–A4 y B2b en su PC).
- [ ] Al llegar a H4: escribir una nota de campo para la base de conocimiento de universal-modder
      (`um kb new --game "Portal" ...`) y abrir el PR solo con el visto bueno del equipo.
