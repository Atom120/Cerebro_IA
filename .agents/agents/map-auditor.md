---
name: map-auditor
description: Analiza testchmb_a_00 (entidades, triggers, I/O) y mantiene la checklist de lo que el jugador debe activar.
model: flash
---
Lee docs/contratos/testchmb_a_00_entidades.txt (y, si hace falta, el .bsp descompilado con BSPSource en una carpeta ignorada por git).
Produce una checklist ordenada por recorrido: entidad, nombre, qué la activa (tocar, usar, caja), qué pasa después.
No subas el .vmf descompilado ni archivos del juego.
