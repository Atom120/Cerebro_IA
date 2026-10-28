---
tipo: tema
actualizado: 2026-10-28
tags: [tema, matematicas, portales, gamedev]
---
# Portales no euclidianos

## Resumen
- Cruzar un portal es aplicar una transformación rígida a la posición y una rotación a la velocidad. El módulo de la velocidad se conserva.

## Conceptos clave
- Marco del portal: columna 0 = normal hacia fuera, columna 2 = arriba, traslación = centro.
- Transformación: `T = M_out · R_π · M_in⁻¹`, con `R_π = diag(-1, -1, 1, 1)` (media vuelta sobre el "arriba" local).
- `pos_out = T · pos_in`; `vel_out = Ω · vel_in`, donde Ω es la parte de rotación de T.
- Cruce: el centro pasa de `d ≥ 0` a `d < 0` respecto al plano, con el punto de corte dentro de la elipse (en Source, media apertura de 32 × 54 su).
- Cambio de lateralidad (UE3 mano izquierda ↔ Source mano derecha): `S = diag(1, -1, 1)`. Un vector pasa como `S·v`, una rotación como `S·R·S`, y un cuaternión (x, y, z, w) como (-x, y, -z, w).
- Una normal lateral (paralela al plano del portal) sale como el eje Y local del portal de salida. Por eso un wallrun solo sigue si esa normal transformada es casi horizontal.
- Enderezar la cámara: `roll(t+dt) = roll(t) · exp(-λ·dt)`, girando sobre el eje de visión. Si se mira recto arriba o abajo, el roll no está definido.

## Cómo se hace (pasos / ejemplos)
- Ejemplo: un portal en el suelo (normal +Z) y uno en la pared (normal +X). Al caer a -1000 se sale a +1000 en X.
- Tras teletransportar, no detectar cruces hasta que el otro proceso confirme el teletransporte (número de secuencia). Si no, el mismo portal se dispara dos veces.
- Implementación de referencia: `aperture-runner-mashup/core/include/aperture_runner/math.h`.

## Fuentes y chats
- [[20_Chats/2026-10-28 - ApertureRunner núcleo y director IA]]
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
