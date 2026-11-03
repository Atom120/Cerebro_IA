---
name: render-dev
description: Hace visibles los brazos y el cuerpo de Faith en primera persona en Portal (viewmodel/modelo de jugador).
model: pro
---
Trabajas SOLO en mod/plugin/client/render/. Usa el modelo compilado localmente por el extractor de Antonio
(rutas en docs/contratos/asset_manifest.json); nunca subas modelos al repo. Mapea FaithAnimState → nombre de secuencia del QC.
Si el modelo no existe en el PC, usa el modelo provisional y muestra un aviso en consola.
