| Valor | Medido en ME | Grabación | Antes | Ahora | Nota |
|---|---|---|---|---|---|
| Gravedad de Faith | 1597.40 uu/s² | `jump, skill_roll` | 646 su/s² (1641 uu) | 628.9 su/s² |  |
| Salto (vz inicial) | 604.18 uu/s | `jump (3 saltos)` | 248 su/s (630 uu) | 237.9 su/s |  |
| Salto: velocidad horizontal añadida | 100.03 uu/s | `jump` | 39.4 su/s (100 uu) | sin cambio |  |
| Sprint máximo | 722.50 uu/s | `sprint_start` | 283.5 su/s (720 uu) | sin cambio |  |
| Sprint: constante de tiempo | 1.86 s | `sprint_start` | rampa lineal de 1.5 s | exponencial, τ = 1.86 s | 157.5 → 281.5 su/s en ~7.7 s |
| Frenado al soltar (τ) | 0.12 s | `sprint_start` | lineal 2419 su/s² (0.12 s) | exponencial, τ = 0.12 s | 283.5 → 0 en ~0.5 s |
| Wallrun: gravedad | 660.52 uu/s² | `wallrun_right (entero)` | escala 0.28 | escala 0.413 |  |
| Wallrun: duración máxima | 1.12 s | `wallrun_right` | 1.2 s | sin cambio (sale sola por la gravedad) |  |
| Wallrun desde el suelo: vz | 514.79 uu/s | `wallrun_left` | 150 su/s | 202.7 su/s |  |
| Salto desde la pared: vz | 517.83 uu/s | `wallrun_*` | = salto (248 su/s) | 203.9 su/s | constante nueva kWallrunJumpZ |
| Wallclimb: vz inicial | 644.98 uu/s | `ledge_grab` | 260 su/s | 253.9 su/s | wallclimb.csv grabó un vault; el wallclimb está en ledge_grab.csv |
| Wallclimb: gravedad | 604.49 uu/s² | `ledge_grab` | escala 0.6 | escala 0.378 |  |
| Subir el borde (duración) | 1.52 s | `ledge_grab (2)` | 0.6 s | 1.52 s |  |
| Vault (duración) | 0.57 s | `wallclimb (2)` | 0.35 s | 0.57 s |  |
| Slide: frenado | 313.39 uu/s² | `slide (2)` | 120 su/s² | 123.4 su/s² |  |
| Slide: velocidad de salida | 255.60 uu/s | `slide` | 98.4 su/s (250 uu) | sin cambio |  |
| Caída dura: bloqueo | 1.96 s | `hard_landing` | 0.5 s, sale a 59 su/s | 1.96 s, sale a 0 |  |
| Skill Roll: duración | 1.24 s | `skill_roll` | 0.6 s | 1.24 s |  |
| Skill Roll: velocidad de salida | 240.50 uu/s | `skill_roll` | conserva la del aterrizaje | 94.7 su/s |  |
