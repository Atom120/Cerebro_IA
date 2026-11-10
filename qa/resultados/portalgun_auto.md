| Id | Prueba | Resultado | Detalle |
|---|---|---|---|
| PG-05 | Caer sobre el borde de un portal de suelo: el embudo de Portal la mete dentro | **PASS** | portal de suelo=1; acaba a 127 su del punto de caida (sin embudo: 0) |
| PG-06 | Sin caida dura ni Skill Roll de Mirror's Edge en esa caida | **PASS** | eventos de caida dura/roll: 0 |
| PG-07 | Despues de la caida Faith retoma el control | **PASS** | ticks_faith 1855 -> 2085 |
| PG-01 | El plugin encuentra los portales de la pistola (no estaban al cargar el mapa) | **PASS** | al cargar: 0 prop_portal; abiertos ahora: 2 |
| PG-02 | Cruzar los portales de la pistola corriendo (azul -> naranja) | **PASS** | cruces=9 |
| PG-03 | Se conserva la inercia al cruzar (la velocidad sigue subiendo tras cruzar) | **PASS** | sprint: 169 -> 196 -> 216 -> 232 su/s |
| PG-04 | Portal sigue abierto (sin cierres) | **PASS** |  |
