# Contrato del Mapa `testchmb_a_00` — Checklist de Verificación (B1)

- **Proyecto**: ApertureRunner (*Mirror's Edge dentro de Portal*)
- **Mapa**: `maps/testchmb_a_00.bsp` (VBSP v20, 323 entidades, Cámaras 00 y 01)
- **Build objetivo de Portal**: `19017868` (Win32 / x86)
- **Responsable**: Benja (`B1`) · **Consumidores**: Zarek (`Z3` — Hito H1), Peña (`P4`), Gonzalo (`G2`), Benja (`B3` — QA por hito)
- **Fuente de datos**: [`docs/contratos/testchmb_a_00_entidades.txt`](./testchmb_a_00_entidades.txt) y lump de entidades (`Lump 0`) de `testchmb_a_00.bsp`.

---

## 1. Propósito de este Contrato

Al sustituir o interceptar `IGameMovement::ProcessMovement` con nuestra DLL (`aperture_runner.dll`), **toda la lógica del mapa `testchmb_a_00` debe seguir funcionando de principio a fin** tanto con `ar_enable 0` (movimiento original de Chell) como con `ar_enable 1` (movimiento de Faith).

Esta checklist ordena cronológicamente los **14 puntos de control (`CHK-01` a `CHK-14`)** desde que el jugador aparece en la Bóveda de Relajación (Cámara 00) hasta el cambio de nivel hacia `testchmb_a_01` al final de la Cámara 01.

---

## 2. Resumen Rápido de Entidades Críticas (323 en total)

| Categoría | Cantidad | Entidades clave (`targetname` / `model`) |
|---|---|---|
| **Aparición y cámara de despertar** | 3 | `info_player_start`, `blackout_viewcontroller` (`point_viewcontrol`), `blackout` (`models/blackin.mdl`), `trigger_knockout_teleport` (`*2` → `destination_knockout_1`) |
| **Portales fijos (`prop_portal`)** | 8 | `portal_blue_start`, `portal_red_start` (precarga); `portal_blue_0`, `portal_red_0` (Cámara 00); `portal_red_1`, `portal_blue_1`, `portal_blue_2`, `portal_blue_3` (Cámara 01) |
| **Temporizadores (`logic_timer`)** | 8 | `timer_clock_minutes`, `timer_clock_seconds`, `timer_clock_centiseconds`, `timer_clock_milliseconds`, `timer_looptick_room1`, `cycle_hudhints_timer`, `redisplay_hint_timer`, `portal_cycle_timer` |
| **Cubo y generadores** | 4 | `box` (`prop_physics`, `models/props/metal_box.mdl`), `box_template_rm1` (`point_template`), `entity_box_maker_rm1` (Cámara 00), `entity_box_maker_rm1a` (Cámara 01) |
| **Botones de piso (`trigger_multiple` + `func_door`)** | 6 | Cámara 00: `floor_button_trigger_player_a00` (`*8`), `floor_button_trigger_box_a00` (`*7`), `floor_button_door_a00` (`*9`). Cámara 01: `floor_button_trigger_player_a00a` (`*5`), `floor_button_trigger_box_a00a` (`*4`), `floor_button_door_a00a` (`*6`) |
| **Puertas (`func_door`)** | 9 | Inodoro (`*55`), `door_left_a00` (`*15`) / `door_right_a00` (`*14`), `exit_door_left_a00` (`*17`) / `exit_door_right_a00` (`*16`), `door_left_00` (`*18`) / `door_right_00` (`*19`), más los 2 discos de botón (`*9`, `*6`) |
| **Desintegradores (`trigger_portal_cleanser`)** | 2 | `trigger_fryer_a00` (`*1`, salida Cámara 00), `a00-a01_trigger_fryer_rm2` (`*27`, salida Cámara 01) |
| **Detectores de portal (`func_portal_detector`)** | 4 | `*10`, `*11`, `*12`, `*13` (conectados a `cam1_tca00`..`cam4_tca00`) |
| **Escenas coreografiadas (`logic_choreographed_scene`)** | 5 | `choreo_AI_00_A_entry`, `choreo_AI_00_A_success`, `choreo_AI_Box_Fried`, `choreo_AI_00_B_entry`, `choreo_AI_00_B_success` |
| **Ascensores (`func_tracktrain`) y fin de nivel** | 3 | `elevator_body_middle` (`*37`, entre Cámara 00 y 01), `body` (`*42`, ascensor final), `a00-a01_change_level_a01_02` (`trigger_changelevel` `*41` → `testchmb_a_01`) |

---

## 3. Checklist Ordenada por Recorrido de Partida

### Fase 1: Bóveda de Relajación y Salida Inicial (Cámara 00 — Parte 1)

| ID | Paso | Entidad(es) (`classname` / `targetname`) | Qué la activa | Qué pasa después (I/O esperado) | Criterio de éxito (`ar_enable 0` y `1`) |
|---|---|---|---|---|---|
| **CHK-01** | Despertar en la cama y liberación de cámara | `info_player_start` `(-607.8, -346.7, 161)`, `logic_auto`, `relay_start_map`, `e3_start`, `point_viewcontrol` (`blackout_viewcontroller`), `prop_dynamic` (`blackout`, `bed_cover`), `trigger_teleport` (`trigger_knockout_teleport` `*2` → `destination_knockout_1` `(-544, -368.8, 160)`) | Automático (`OnMapSpawn` a `t = 0.2 s`) + contacto del jugador con `trigger_knockout_teleport` cuando termina la animación `exit1` de `blackout` (`t ≈ 5 s`). | `bed_cover` se abre (`t = 5.0 s`), `blackout` termina `exit1` y habilita `trigger_knockout_teleport`. Al tocarlo (`OnStartTouch`), se teletransporta al jugador a `destination_knockout_1`, se deshabilita y destruye (`Kill`) `blackout_viewcontroller` y `blackout`, y se cierra `bed_cover`. | El jugador recupera el control total de vista y movimiento fuera de la cama sin quedarse atrapado en la geometría ni con la cámara bloqueada. |
| **CHK-02** | Interacción `+use` en la bóveda (inodoro y objetos físicos) | `func_door` (`*55`, `spawnflags 4364`), `logic_relay` (`relay_toiletflush`), `prop_physics` (portapapeles `bts_clipboard.mdl`, taza `garbage_coffeemug001a_forevergibs.mdl`) | Jugador pulsa `+use` (`E`) mirando al inodoro o a la taza/portapapeles. | `*55` dispara `OnOpen -> relay_toiletflush,Trigger` (sonidos `toilet_flush` y `toilet_thank`). Los `prop_physics` se recogen y sueltan con `+use`. | `+use` interactúa con puertas usables y recoge/suelta props físicos sin interferencia del movimiento. |
| **CHK-03** | Escena inicial, parpadeo de luces, reloj de 60 s y cartel `00` | `logic_choreographed_scene` (`choreo_AI_00_A_entry`), `relay_dim_lights`, `relay_up_lights`, `trigger_look` (`trigger_look_sign` `*56`), `relay_sign00`, `sign_00`, `timer_clock_minutes`, `timer_clock_seconds`, `timer_clock_centiseconds`, `timer_clock_milliseconds`, `counter_clock` | `relay_start_map` en `t = 6.0 s` inicia `choreo_AI_00_A_entry` y los 4 `logic_timer`. Mirar hacia el frente activa `trigger_look_sign` (`OnTrigger`). | Durante la escena parpadean las luces (`relay_dim_lights`), se habilita `trigger_look_sign` (`+1.5 s`), y al mirar hacia el cartel se anima e ilumina `sign_00`. El reloj descuenta desde `60 s` hasta `0`. | `trigger_look_sign` detecta la dirección de vista (`view_angles`) del jugador y enciende `sign_00`. |
| **CHK-04** | Apertura del primer portal fijo y salida de la bóveda | `logic_relay` (`relay_portal_cancel_room1`), `prop_portal` (`portal_red_0` en `(-591, -412, 212)` `Yaw 90°` y `portal_blue_0` en `(-832, -320, 180)` `Yaw 0°`) | `relay_start_map` dispara `relay_portal_cancel_room1` en `t = 66.5 s`. El jugador camina/esprinta a través de `portal_red_0`. | Se activan `portal_red_0` y `portal_blue_0` (`SetActivatedState 1`), tiembla la cámara (`shake_portal_spawn_room1`) y se apaga `timer_looptick_room1`. El jugador entra por `portal_red_0` y sale por `portal_blue_0` fuera de la bóveda. | Cruce limpio de `prop_portal` fijo: posición, velocidad y ángulos se transforman sin tirones ni pérdida de estado en el suelo. |

---

### Fase 2: Sala del Cubo, Botón y Ascensor Intermedio (Cámara 00 — Parte 2)

| ID | Paso | Entidad(es) (`classname` / `targetname`) | Qué la activa | Qué pasa después (I/O esperado) | Criterio de éxito (`ar_enable 0` y `1`) |
|---|---|---|---|---|---|
| **CHK-05** | Entrada a la sala del botón, cierre de puerta de entrada y caída del cubo | `func_door` (`door_left_a00` `*15`, `door_right_a00` `*14`, empiezan abiertas), `trigger_once` (`trigger_door_close_rm1` `*30` en `(-544, -788, 188)`), `logic_autosave` (`autosave_rm1`), `logic_relay` (`logic_drop_box`), `prop_dynamic` (`box_dropper_model_cover`), `trigger_multiple` (`trigger_drop_new_box` `*45`), `env_entity_maker` (`entity_box_maker_rm1`), `prop_physics` (`box`) | El jugador cruza la puerta abierta y toca `trigger_door_close_rm1` (`OnStartTouch`). | Se cierran `door_left_a00` y `door_right_a00`, se disuelven (`Fizzle`) `portal_red_0` y `portal_blue_0`, se guarda partida (`autosave_rm1`) y tras `2.0 s` se dispara `logic_drop_box`: se abre `box_dropper_model_cover`, cae `box` a la cámara y `trigger_drop_new_box` hace `ForceSpawn` del siguiente cubo en reserva. | El casco del jugador dispara `trigger_door_close_rm1` al pasar y el cubo `box` cae al suelo listo para recogerse. |
| **CHK-06** | Activación del botón de piso #1 (con el jugador y con `box`) | `trigger_multiple` (`floor_button_trigger_player_a00` `*8`, `spawnflags 1`; `floor_button_trigger_box_a00` `*7`, `spawnflags 8`, `filtername = filter_boxes`), `func_door` (`floor_button_door_a00` `*9`), `func_door` (`exit_door_left_a00` `*17`, `exit_door_right_a00` `*16`), `logic_relay` (`relay_boxplacementsuccess`), `logic_choreographed_scene` (`choreo_AI_00_A_success`) | 1) El jugador pisa el botón `(-511.9, -928.5, 151.4)` y luego se baja.<br>2) El jugador recoge `box` con `+use` y la deposita sobre el botón. | 1) Al pisar el jugador: baja `floor_button_door_a00`, cambia `texturetoggle_exit_doorstate_a00` a `1` y se abren `exit_door_left_a00` y `exit_door_right_a00`. Al salir (`OnEndTouchAll`), se cierran.<br>2) Con `box`: las puertas quedan abiertas y tras `1.0 s` arranca `choreo_AI_00_A_success`. | El botón responde tanto al peso del jugador (`OnTrigger` / `OnEndTouchAll`) como al cubo `box`, manteniendo abierta la salida. |
| **CHK-07** | Desintegrador (Emancipation Grill) de Cámara 00 y apertura de ascensor | `trigger_once` (`trigger_exit_room1` `*28` en `(-1152, -864, 192.15)`), `trigger_portal_cleanser` (`trigger_fryer_a00` `*1` en `(-1264, -864, 192)`), `logic_relay` (`logic_play_cleanser_dialog`, `logic_no_play_cleanser_dialog`), `logic_choreographed_scene` (`choreo_AI_Box_Fried`) | 1) El jugador cruza `exit_door_*_a00`, toca `trigger_exit_room1` y atraviesa `trigger_fryer_a00`.<br>2) *(Opcional QA)*: lanzar o llevar `box` contra `trigger_fryer_a00`. | 1) `trigger_exit_room1` dispara `elevator_door_model_open` (abre la puerta del ascensor). El jugador cruza `trigger_fryer_a00` sin daño.<br>2) Si `box` toca `trigger_fryer_a00` (`OnDissolveBox`), se desintegra, suena `choreo_AI_Box_Fried` y se suelta un nuevo cubo (`logic_drop_box`). | El jugador atraviesa el cleanser conservando su velocidad/estado, y la puerta del ascensor intermedio se abre al tocar `trigger_exit_room1`. |
| **CHK-08** | Viaje en el ascensor intermedio (Cámara 00 → Cámara 01) | `trigger_once` (`trigger_elevator_move` `*35` en `(-1500, -864, 186.5)`), `logic_relay` (`elevator_door_model_close`, `elevator_door_model_open`, `elevator_logic_stop`), `prop_dynamic` (`elevator_door_model_middle`), `func_tracktrain` (`elevator_body_middle` `*37`), `path_track` (`track_elevator_p2_middle` → `track_elevator_p4_middle`) | El jugador entra al ascensor y toca `trigger_elevator_move` (`OnStartTouch`). | Se cierra `elevator_door_model_middle`. A los `5.0 s`, `elevator_body_middle` sube (`StartForward`) desde `Z = 214` hasta `Z = 792` (`track_elevator_p4_middle`), donde `OnPass` dispara `elevator_logic_stop` y abre la puerta superior (`+1.0 s`). | El jugador sube sobre el `func_tracktrain` en movimiento sin atravesar el piso del ascensor, sin quedarse flotando ni sufrir temblores de predicción. |

---

### Fase 3: Portales Rotativos, Segundo Botón y Fin de Nivel (Cámara 01)

| ID | Paso | Entidad(es) (`classname` / `targetname`) | Qué la activa | Qué pasa después (I/O esperado) | Criterio de éxito (`ar_enable 0` y `1`) |
|---|---|---|---|---|---|
| **CHK-09** | Llegada a Cámara 01, cartel `01` y activación del ciclo de portales | `trigger_once` (`*50` en `(-1176, -864, 768)`), `relay_sign01`, `sign_01`, `trigger_once` (`trigger_section2_start` `*33` en `(-1088, -752, 768)`), `trigger_once` (`*32` en `(-1088, -512, 592)`), `logic_choreographed_scene` (`choreo_AI_00_B_entry`) | El jugador camina por el pasillo superior tras salir del ascensor, tocando `*50`, `trigger_section2_start` y `*32`. | `*50` enciende `sign_01`. `trigger_section2_start` cierra la puerta del ascensor, guarda partida (`autosave_rm1`) y ajusta el bloom. `*32` inicia `choreo_AI_00_B_entry`, activa `portal_red_1` y arranca `portal_cycle_timer`. | Todos los triggers de paso se disparan en orden y se inicia el ciclo de portales de la Cámara 01. |
| **CHK-10** | Ciclo de portales cada 5 s y obtención del cubo en la fosa | `logic_timer` (`portal_cycle_timer`, `RefireTime = 5`), `math_counter` (`count_portal_chambers`, `1..4`), `logic_case` (`case_spawn_portals`), `logic_relay` (`logic_make_blue_1`, `logic_make_blue_2`, `logic_make_blue_3`), `prop_portal` (`portal_red_1` fijo en `(-1088, -632, 628)` y azules rotativos: `portal_blue_1` botón, `portal_blue_2` salida, `portal_blue_3` fosa del cubo), `env_entity_maker` (`entity_box_maker_rm1a`) | Cada `5.0 s`, `portal_cycle_timer` alterna el portal azul activo (`portal_blue_1` → `portal_blue_2` → `portal_blue_3`). El jugador cruza `portal_red_1` hacia `portal_blue_3`, recoge `box` con `+use` y regresa cuando se activa `portal_blue_1`. | Cada cambio hace `Fizzle` del portal azul anterior y `SetActivatedState 1` del nuevo. El jugador puede cruzar repetidamente entre las 3 salas transportando `box`. | El cruce por `prop_portal` funciona en ambos sentidos y sosteniendo `box`, incluso si el jugador entra corriendo o haciendo parkour. |
| **CHK-11** | Activación del botón de piso #2 en Cámara 01 | `trigger_multiple` (`floor_button_trigger_player_a00a` `*5`, `floor_button_trigger_box_a00a` `*4`), `func_door` (`floor_button_door_a00a` `*6`), `logic_relay` (`trigger_open_door_00`, `trigger_close_door_00`), `func_door` (`door_left_00` `*18`, `door_right_00` `*19`), `logic_choreographed_scene` (`choreo_AI_00_B_success`) | Colocar `box` (o pararse) sobre el botón de la sala izquierda `(-1376, -572, 600)`. | Se abre `floor_button_door_a00a`, cambia `texturetoggle_exit_doorstate_a00_b` a `1`, `trigger_open_door_00` abre `door_left_00` y `door_right_00`, y con `box` (`OnStartTouch`) se reproduce `choreo_AI_00_B_success`. | El botón #2 abre `door_left_00`/`door_right_00` y las mantiene abiertas mientras `box` esté encima. |
| **CHK-12** | Cruce a la salida de Cámara 01 y segundo desintegrador | `trigger_once` (`*43` en `(-718.81, -256, 640)`), `trigger_portal_cleanser` (`a00-a01_trigger_fryer_rm2` `*27` en `(-595.51, -256, 638.48)`), `logic_relay` (`a00-a01_elevator_door_model_open`) | Con la puerta abierta, el jugador cruza hacia la sala de salida (`portal_blue_2`), pasa la puerta y toca `*43`, luego atraviesa `a00-a01_trigger_fryer_rm2`. | `*43` apaga `portal_cycle_timer`, disuelve `portal_blue_2` y abre la puerta del ascensor final (`a00-a01_elevator_door_model_open`). El jugador cruza `a00-a01_trigger_fryer_rm2`. | El ciclo de portales se detiene al tocar `*43`, la puerta del ascensor final se abre y el jugador cruza el cleanser sin problemas. |
| **CHK-13** | Integridad de detectores de portal y cámaras de seguridad | `func_portal_detector` (`*10`, `*11`, `*12`, `*13`), `npc_security_camera` (`cam1_tca00`, `cam2_tca00`, `cam3_tca00`, `cam4_tca00`) | Verificación pasiva durante el recorrido por Cámara 00 y Cámara 01. | En `testchmb_a_00` no hay Portal Gun, por lo que `OnStartTouchPortal1/2 -> cam*_tca00,Ragdoll` no debe dispararse accidentalmente. Las 4 cámaras siguen al jugador desde la pared. | Ninguna `npc_security_camera` cae en ragdoll por error ni genera errores en consola al pasar cerca de `*10..*13`. |
| **CHK-14** | Ascensor final y transición de nivel (`testchmb_a_01`) | `trigger_once` (`a00-a01_trigger_elevator_move` `*38` en `(-356, -256, 634.5)`), `logic_relay` (`a00-a01_elevator_door_model_close`, `a00-a01_elevator_logic_stop`), `func_tracktrain` (`body` `*42`), `path_track` (`a00-a01_track_elevator_p2` → `a00-a01_track_elevator_p4`), `info_landmark` / `trigger_transition` (`a00-a01_landmark_elevator`), `trigger_changelevel` (`a00-a01_change_level_a01_02` `*41`) | El jugador entra al ascensor final y toca `a00-a01_trigger_elevator_move` (`OnStartTouch`). | Se cierra `a00-a01_elevator_door_model`. A los `5.0 s`, el ascensor `body` sube (`StartForward`), y al llegar a `a00-a01_track_elevator_p4` (`Z = 1240`), `OnPass` ejecuta `a00-a01_change_level_a01_02,ChangeLevel` hacia `testchmb_a_01`. | El ascensor sube con el jugador dentro y dispara limpiamente el cambio de nivel a `testchmb_a_01` sin colgar el juego ni corromper el estado del plugin. |

---

## 4. Plantilla de Ejecución para QA (`B3` — `qa/resultados/H1.md`, `H2.md`, `H3.md`)

Cuando se ejecuten las pruebas de cada hito en Portal (`map testchmb_a_00` con `plugin_load addons/aperture_runner`), copia y rellena esta tabla en `qa/resultados/<Hito>.md`:

```markdown
| ID | Descripción corta | `ar_enable 0` (Pass/Fail) | `ar_enable 1` (Pass/Fail) | Demo (`.dem`) | Notas / Incidencias |
|---|---|---|---|---|---|
| CHK-01 | Despertar y salida de `blackout_viewcontroller` | [ ] | [ ] | | |
| CHK-02 | `+use` en inodoro y props físicos de la bóveda | [ ] | [ ] | | |
| CHK-03 | Escena `choreo_AI_00_A_entry`, reloj 60 s y `sign_00` | [ ] | [ ] | | |
| CHK-04 | Apertura y cruce de `portal_red_0` → `portal_blue_0` | [ ] | [ ] | | |
| CHK-05 | `trigger_door_close_rm1` y caída de `box` (`logic_drop_box`) | [ ] | [ ] | | |
| CHK-06 | Botón #1 (`floor_button_trigger_player/box_a00`) y puertas | [ ] | [ ] | | |
| CHK-07 | `trigger_exit_room1` y cleanser `trigger_fryer_a00` | [ ] | [ ] | | |
| CHK-08 | Subida en ascensor intermedio (`elevator_body_middle`) | [ ] | [ ] | | |
| CHK-09 | Pasillo Cámara 01 (`sign_01`, `trigger_section2_start`, `*32`) | [ ] | [ ] | | |
| CHK-10 | Ciclo de 5 s (`portal_red_1` ↔ `portal_blue_1..3`) y `box` | [ ] | [ ] | | |
| CHK-11 | Botón #2 (`floor_button_trigger_player/box_a00a`) y puertas | [ ] | [ ] | | |
| CHK-12 | Salida `*43` y cleanser final `a00-a01_trigger_fryer_rm2` | [ ] | [ ] | | |
| CHK-13 | Detectores `*10..*13` y cámaras `cam1..4_tca00` intactas | [ ] | [ ] | | |
| CHK-14 | Ascensor final (`body`) y `ChangeLevel` a `testchmb_a_01` | [ ] | [ ] | | |
```
