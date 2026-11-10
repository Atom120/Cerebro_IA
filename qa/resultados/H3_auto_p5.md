| Id | Prueba | Resultado | Detalle |
|---|---|---|---|
| H3-01 | Plugin y trazas reales (autotest Z4) | **PASS** |  |
| H3-02 | Wallrun en una pared real del mapa | **PASS** | 28 ticks en wallrun_right |
| H3-03 | La vista se inclina en el wallrun (camara G3/G4) | **PASS** | roll minimo -13.6 grados (objetivo -15) |
| H3-04 | Wallclimb en una pared real | **PASS** | sube 56 su |
| H3-05 | Giro de 180 grados en la vista (Turn180 + camara) | **PASS** | yaw 0.0 -> 180.0 |
| H3-06 | Agacharse baja la vista del motor (64 -> 28) | **FAIL** | vista z 192.1 -> 192.1 |
| H3-07 | Slide desde el sprint | **PASS** |  |
| H3-08 | Cruzar portal_red_0 -> portal_blue_0 conserva la velocidad (P4) | **PASS** | 183.8 su/s al entrar, 184.6 al salir, cruces=1 |
| H3-09 | Vault en una plataforma real (borde biselado) | **PASS** | 37 ticks en vault, termina en z=146.2 |
| H3-10 | Mantle a una repisa de 65 su (salto + subir) | **PASS** | termina en z=224.3 (repisa en 225.8) |
| H3-11 | El FOV se abre al esprintar (75 -> hasta 85) | **FAIL** | fov=75 |
| H3-12 | Balanceo de cabeza al correr | **FAIL** | vista z varia 0.00 su en 2 muestras |
| H3-13 | ar_enable 1 queda guardado en addons/aperture_runner.cfg | **PASS** |  |
| H3-14 | Sin 'Restoring player view height' en la consola | **FAIL** | 1 mensajes |
| H3-15 | Portal sigue abierto (sin cierres) | **PASS** |  |
