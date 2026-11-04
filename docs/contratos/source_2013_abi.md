# ABI de Source 2013 en Portal 19017868 (Z2 + Z4 + Z5)

- **Tareas:** Z2, Z4 y Z5 (Zarek). **Las cerró Sebas** el 2026-11-04. No modificar sin hablar con Sebas.
- Cada dato se verificó en el juego real con una sonda temporal (`__try/__except` y log en `ServerActivate` o en el hook). Las estructuras están en `mod/plugin/server/source_interfaces.h`, con `static_assert`.
- **Regla:** casi todo lo que hay en internet es de CS:GO y **no** coincide con Portal (2013).

## Interfaces
| Interfaz | De dónde | Uso |
|---|---|---|
| `GameMovement001` | `gameServerFactory` (server.dll) | hook de `ProcessMovement` (ranura 1) |
| `GameMovement001` | `client.dll` `CreateInterface` | hook de predicción (G1); con `cl_predict 0` no se usa |
| `EngineTraceServer003` | `interfaceFactory` (engine.dll) | `TraceRay` en la **ranura 4** (la 5 cuelga Portal) |

No hicieron falta `PlayerInfoManager002` ni `VEngineServer021`: los datos del jugador llegan en `CMoveData`, y las entidades en la lista de edicts de `ServerActivate`.

## `CMoveData` (lo que usa Z5)
| Campo | Offset | Notas |
|---|---|---|
| `m_vecViewAngles` | 12 | grados (pitch, yaw, roll) |
| `m_nButtons` / `m_nOldButtons` | 36 / 40 | `IN_JUMP` = 2, `IN_DUCK` = 4, `IN_FORWARD` = 8, `IN_RELOAD` = 8192, `IN_WALK` = 262144 |
| `m_flForwardMove` / `m_flSideMove` | 44 / 48 | ±450 (`cl_forwardspeed`); + = adelante y derecha |
| `m_flMaxSpeed` | 56 | 150 en Portal |
| `m_vecVelocity` | 64 | su/s |
| `m_vecAbsOrigin` | 152 | pies del jugador |

## `Ray_t`, `trace_t`
- `Ray_t`: 4 `VectorAligned` (64 bytes), luego `m_IsRay` (64) y `m_IsSwept` (65). **Sin** `m_pWorldAxisTransform`.
- `trace_t`: `fraction` en 44, `allsolid` en 54, `m_pEnt` en 76.
- `m_pEnt` **nunca es null** al chocar con el mundo: es la entidad `worldspawn`.

## Entidades
- `edict_t`: 20 bytes. `IServerUnknown* m_pUnk` en +12.
- `IServerUnknown`: ranura 3 `GetCollideable`, ranura 4 `GetNetworkable`.
- `IServerNetworkable`: ranura 3 `GetClassName`.
- `ICollideable`: ranura 10 `GetCollisionOrigin`, ranura 11 `GetCollisionAngles`.
- En `testchmb_a_00` se ven los 8 `prop_portal` en las posiciones del contrato B1 (p. ej. `portal_blue_0` en (-832, -320, 180) con yaw 0).
- Clasificación medida: `worldspawn` → World, `prop_physics` → Physics, `prop_dynamic`/`func_brush` → StaticProp, `npc_security_camera` → Physics.

## Comandos (`CCommand`)
- `m_nArgc` en +0, `m_ppArgv[64]` en +1032.
- Los comandos desconocidos que se escriben en la consola llegan al plugin por `IServerPluginCallbacks::ClientCommand`. Así funcionan `ar_enable`, `ar_status` y `ar_debug`, sin crear cvars.

## Pruebas automáticas en el juego
- `hl2.exe -hijack -game portal +exec <cfg>` manda comandos al Portal abierto.
- Los argumentos que empiezan por `-` (p. ej. `setpos -700 ...`) hay que pasarlos por un `.cfg`, porque en la línea de comandos se leen como opciones.
- Hay que arrancar con `+sv_cheats 1` para usar `setpos`/`setang`/`ent_fire`.
- Pasillo recto para probar: `setpos -660 -864 140` y `setang 0 180 0` (suelo z = 128, unos 600 su hasta x = -1300).
