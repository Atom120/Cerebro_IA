---
tipo: chat
fecha: 2026-11-05
herramienta: Claude
proyecto: ApertureRunner
tags: [chat, apertureRunner, universal-modder, recon, source, ue3]
---
# ApertureRunner: recon con universal-modder

## Objetivo
- Usar por primera vez el plugin universal-modder en ApertureRunner: skill `game-recon` y la base de conocimiento.

## Resumen
- `um scan` de Portal: Source 1, `hl2.exe` x86, build 19017868, sin anticheat, ruta `addons/` (la que ya usamos). El "ASI loader" que marca es un falso positivo (archivos `.asi` de Miles Sound System).
- Mirror's Edge sigue sin estar instalado (ni Steam, Epic, Xbox, EA app ni GOG).
- En la base de conocimiento no hay notas de Portal 1 ni de Mirror's Edge 2008. Las más útiles son Portalcraft (Minecraft dentro de Portal 2) y Batman de Arkham Knight en Risk of Rain 2 (UE3 → umodel → Blender).

## Decisiones / resultados
- Plan de recon en `docs/MODDING_PLAN.md`.
- Recomendación principal: leer las variables de red por nombre (tablas de clases) en vez de los 5 offsets fijos de `mod/plugin/server/source_interfaces.h`, que se rompen si Valve actualiza Portal.
- Para Antonio (A1–A3): trucos de umodel y del importador PSK/PSA (Referer en la descarga, escala del PSA, cuaterniones de huesos sin padre, solo LOD0).

## Pendientes
- [ ] Decidir si se implementa la lectura de offsets por nombre.
- [ ] Instalar Mirror's Edge.
- [ ] En H4, escribir una nota de campo para la base de conocimiento de universal-modder (PR solo con el visto bueno del equipo).

## Relacionado
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
- [[20_Chats/2026-11-03 - Instalación de universal-modder]]
- [[Inicio]]

## Revisión de levels/ (2026-11-09)
- Los .bsp y .vmf de la ruta archivada no tienen contenido de Mirror's Edge: bloques hechos a mano con materiales de serie de Source; solo hay coordenadas de Edge_p en los manifiestos. Detalle en `docs/MODDING_PLAN.md`.
