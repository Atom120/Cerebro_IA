| Id | Prueba | Resultado | Detalle |
|---|---|---|---|
| H2-01 | Plugin cargado y trazas reales (autotest Z4) | **PASS** | OK  frac=0.1998 z=160.0 nz=1.00 kind=1 |
| H2-02 | ar_enable 1 activa el movimiento de Faith | **PASS** |  |
| H2-03 | Sprint con inercia (157.5 -> 283.5 su/s, rampa exponencial tau 1.86 s) | **PASS** | velocidad maxima 205.2 su/s, rampa 48.3 su/s^2 |
| H2-04 | Salto de Faith (237.8 su/s, ~45 su) | **PASS** | altura 46.8 su |
| H2-05 | Agacharse (anim crouch) | **PASS** |  |
| H2-06 | Cruce de portal_red_0 a portal_blue_0 (CHK-04) con ar_enable 1 | **PASS** | motor=(-670 -320 128), ticks de Portal=48 |
| H2-07 | Subir en el ascensor intermedio (CHK-08) con ar_enable 1 | **PASS** | motor z=725 (sube de ~214 a ~792) |
| H2-08 | ar_enable 0 devuelve el movimiento original | **PASS** | ticks de Faith 4668 -> 4668 tras desactivar |
| H2-09 | Portal sigue abierto al terminar (sin cierres) | **PASS** |  |
