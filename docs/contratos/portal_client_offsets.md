# Contrato de Interfaces, Predicción y Offsets de `client.dll` (Portal 1 — Build 19017868)

- **Tarea**: `G1` (Estudio del lado cliente: `GameMovement001`, `CreateMove`, predicción en un solo proceso y verificación de offsets de `LESSONS.md`)
- **Autor**: Gonzalo Robles (`mod/plugin/client/`, `mod/camera/`)
- **Build objetivo**: Portal 1 (Steam AppID `400`), **build `19017868`** (`2025-06-27`, rama *Source 2013 Singleplayer*, **32 bits `x86`**)
- **Módulo analizado**: `portal/bin/client.dll` (`TimeDateStamp = 0x68362D89`)
- **Estado**: Verificado contra arquitectura Source 2013 SP x86 y contrastado con `LESSONS.md` y contratos v1 (`mod/include/ar/faith_movement.h`)

---

## 1. Arquitectura en Proceso Único (`hl2.exe` x86)

En *Portal 1* (un jugador / *listen server*), tanto el servidor como el cliente corren dentro del **mismo proceso de 32 bits (`hl2.exe`)**:
1. El motor carga nuestra DLL (`portal/addons/aperture_runner.dll`) como *Valve Server Plugin* (`ISERVERPLUGINCALLBACKS003`).
2. `IServerPluginCallbacks::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)` entrega las fábricas de `engine.dll` y `server.dll`, pero **no** pasa directamente la fábrica de `client.dll`.
3. Como `client.dll` vive en el mismo espacio de direcciones del proceso `hl2.exe`, el módulo cliente de nuestra misma DLL (`mod/plugin/client/`) obtiene su fábrica dinámicamente sin necesidad de inyectar una segunda DLL:

```cpp
HMODULE hClient = GetModuleHandleA("client.dll");
auto clientFactory = reinterpret_cast<ar::source::CreateInterfaceFn>(
    GetProcAddress(hClient, "CreateInterface")
);
```

> **Nota sobre el orden de carga**: Si en el instante exacto de `IServerPluginCallbacks::Load` el módulo `client.dll` aún no ha terminado de inicializarse, la adquisición de interfaces de cliente se reintenta de forma perezosa (*lazy init*) en `LevelInit` / `ClientActive` y se desengancha limpiamente en `LevelShutdown` / `Unload`.

---

## 2. Interfaces de `client.dll` por `CreateInterface` (Prioridad 1 — Sin Offsets Fijos)

Siguiendo la regla de ingeniería inversa del proyecto (*1º interfaces por `CreateInterface`, 2º datamaps/netprops, 3º firmas de bytes, 4º RVAs fijos ligados al `buildid`*), `client.dll` expone las siguientes interfaces estables en el build `19017868`:

| Interfaz (`CreateInterface`) | Clase en `client.dll` | Uso en ApertureRunner (`G1`, `G2`, `G4`) | Estado |
|---|---|---|---|
| `"GameMovement001"` | `CPortalGameMovement` (`IGameMovement`) | **Predicción de movimiento en cliente (`G1`/`G2`)**: copia independiente de `IGameMovement` usada por `CPrediction::RunCommand`. | **VERIFICADO** (ABI idéntica a `server.dll`) |
| `"VClient017"` | `CHLClient` (`IBaseClientDLL`) | **Entrada y ciclo de frame (`G1`/`G4`)**: vtable para `CreateMove` (slot 21), `HudProcessInput` (slot 10, de donde cuelga `g_pClientMode`) y `FrameStageNotify` (slot 35). | **VERIFICADO** (Source 2013 SP) |
| `"VClientPrediction001"` | `CPrediction` (`IPrediction`) | **Estado de predicción (`G2`)**: permite consultar `InPrediction()` (slot 14) y `IsFirstTimePredicted()` (slot 15) para no duplicar efectos de cámara/sonido en re-predicciones. | **VERIFICADO** (Source 2013 SP) |
| `"VClientEntityList003"` | `CClientEntityList` (`IClientEntityList`) | **Acceso a entidades de cliente (`G1`/`G4`)**: obtiene `C_Portal_Player` (`GetClientEntity(1)`) y viewmodels/portales sin depender del RVA fijo de `cl_entitylist`. | **VERIFICADO** (Source 2013 SP) |

---

## 3. La Copia de `GameMovement001` en `client.dll` y la Predicción (`G1` $\rightarrow$ `G2`)

### 3.1. Por qué existen dos instancias de `GameMovement001`
En Source Engine existen **dos singletons separados** de `CPortalGameMovement`:
- **Servidor (`server.dll`)**: invocado desde `CPlayerMove::RunCommand` durante `SV_Physics` (enganchado por Zarek en `Z3`).
- **Cliente (`client.dll`)**: invocado desde `CPrediction::RunCommand` cada frame antes de renderizar la vista cuando `cl_predict 1` está activo (valor por defecto en Portal).

Si solo se modifica el movimiento en `server.dll`, `client.dll` predice localmente la física original de Chell (`190 su/s`, sin wallrun ni inercia de Faith) y, al recibir el estado del servidor en el siguiente tick, corrige bruscamente la posición, provocando **temblor severo (*rubberbanding*)**.

### 3.2. Layout de VTable de `IGameMovement` en `client.dll` (MSVC x86 `__thiscall`)
La interfaz `IGameMovement` en `client.dll` comparte exactamente el mismo contrato binario que en `server.dll`:

| Slot VTable | Firma C++ (`__thiscall` en MSVC x86) | Rol |
|---|---|---|
| `0` | `virtual ~IGameMovement()` | Destructor virtual. |
| `1` | `virtual void ProcessMovement(C_BasePlayer* pPlayer, CMoveData* pMove)` | **Punto de hook principal (`ClientPredictionHook`)**: ejecuta un tick de simulación sobre `pMove`. |
| `2` | `virtual void StartTrackPredictionErrors(C_BasePlayer* pPlayer)` | Inicio del bloque de predicción del jugador local. |
| `3` | `virtual void FinishTrackPredictionErrors(C_BasePlayer* pPlayer)` | Fin del bloque de predicción y comparación de errores. |
| `4` | `virtual void DiffPrint(const char* fmt, ...)` | Diagnóstico de diferencias de predicción (`cl_showerror`). |

### 3.3. Estrategia de Rebobinado (*Rewind / Replay*) para `G2`
Durante la predicción, Source puede re-ejecutar varios ticks pasados si un frame tarda más de `15 ms` (`kTickInterval = 0.015f`). Por ello:
- `ar::FaithState` (definido en `mod/include/ar/faith_movement.h`, **196 bytes**, `std::is_trivially_copyable_v<FaithState> == true`) no usa reloj del sistema ni variables globales.
- En `G2`, el cliente mantiene un buffer circular indexado por `command_number % MULTIPLAYER_BACKUP` (`90` slots en Source 2013) o sincroniza el `FaithState` base al inicio del tick confirmado y aplica `IFaithMovement::Step` de forma idéntica en servidor y cliente.

---

## 4. Hook de `CreateMove`, `CMoveData` y Mapeo de Botones (`ar::FaithButton`)

### 4.1. Dos vías complementarias para leer la entrada del jugador
1. **Desde `CMoveData*` dentro de `IGameMovement::ProcessMovement` (Recomendado para `faith_move` en `G2`)**:
   - Antes de llamar a `IGameMovement::ProcessMovement(pPlayer, pMove)`, `CPrediction::SetupMove` (en cliente) y `CPlayerMove::SetupMove` (en servidor) copian el `CUserCmd` actual dentro de la estructura `CMoveData`.
   - Leer desde `CMoveData` garantiza que tanto la primera ejecución como cualquier *replay* de predicción reciban exactamente los mismos `view_angles`, `forward_move`, `side_move`, `buttons` y `old_buttons`.
2. **Desde `CreateMove` (`IBaseClientDLL` slot 21 / `IClientMode` slot 24)**:
   - Útil cuando necesitamos modificar el `CUserCmd` **antes** de que llegue al motor (por ejemplo, aplicar el giro automático de 180° en `Turn180` / `WallClimbTurn` sobre `cmd->viewangles` o bloquear entradas durante escenas coreografiadas).
   - `g_pClientMode` (`IClientMode*`) se obtiene leyendo el puntero cargado en `ECX` en el prólogo de `IBaseClientDLL::HudProcessInput` (slot 10 de `VClient017`: `8B 0D ?? ?? ?? ??` $\rightarrow$ `mov ecx, dword ptr [g_pClientMode]`).

### 4.2. Layout de `CMoveData` en Source 2013 SP (x86, 32 bits)

| Offset | Campo | Tipo | Descripción y mapeo hacia `ar::FaithInput` |
|---|---|---|---|
| `+0x00` | `m_bFirstRunOfFunctions` | `bool` | `true` en la primera pasada de predicción del comando (`IsFirstTimePredicted`). |
| `+0x01` | `m_bGameCodeMovedPlayer` | `bool` | Indica si el código de juego teletransportó al jugador. |
| `+0x04` | `m_nPlayerHandle` | `uint32_t` | `CBaseHandle` (`EHANDLE`) de la entidad jugador. |
| `+0x08` | `m_nImpulseCommand` | `int32_t` | Comando `impulse` si existe. |
| `+0x0C` | `m_vecViewAngles` | `ar::Vec3` | Ángulos de vista `(pitch, yaw, roll)` en grados $\rightarrow$ `FaithInput::view_angles`. |
| `+0x18` | `m_vecAbsViewAngles` | `ar::Vec3` | Ángulos absolutos de vista en grados. |
| `+0x24` | `m_nButtons` | `int32_t` | Bitmask `IN_*` mantenido en este tick $\rightarrow$ `MapSourceButtonsToFaith(m_nButtons)`. |
| `+0x28` | `m_nOldButtons` | `int32_t` | Bitmask `IN_*` del tick previo $\rightarrow$ permite calcular `FaithInput::buttons_pressed`. |
| `+0x2C` | `m_flForwardMove` | `float` | Velocidad frontal deseada (`[-cl_forwardspeed, +cl_forwardspeed]`) $\rightarrow$ normalizada a `[-1, 1]` en `FaithInput::forward_move`. |
| `+0x30` | `m_flSideMove` | `float` | Velocidad lateral deseada (`[-cl_sidespeed, +cl_sidespeed]`, `+` = derecha) $\rightarrow$ normalizada a `[-1, 1]` en `FaithInput::side_move`. |
| `+0x34` | `m_flUpMove` | `float` | Velocidad vertical deseada. |
| `+0x38` | `m_flMaxSpeed` | `float` | Velocidad máxima permitida por el motor en el tick. |
| `+0x3C` | `m_flClientMaxSpeed` | `float` | Velocidad máxima reportada al cliente. |
| `+0x40` | `m_vecVelocity` | `ar::Vec3` | Velocidad actual del jugador en `su/s` $\leftrightarrow$ `FaithState::velocity`. |
| `+0x4C` | `m_vecAngles` | `ar::Vec3` | Ángulos del modelo. |
| `+0x58` | `m_vecOldAngles` | `ar::Vec3` | Ángulos previos. |
| `+0x64` | `m_outStepHeight` | `float` | Altura acumulada de escalón subido en este tick. |
| `+0x68` | `m_outWishVel` | `ar::Vec3` | Velocidad deseada resultante. |
| `+0x74` | `m_outJumpVel` | `ar::Vec3` | Impulso de salto resultante. |
| `+0x80` | `m_vecConstraintCenter` | `ar::Vec3` | Centro de restricción de movimiento. |
| `+0x8C` | `m_flConstraintRadius` | `float` | Radio de restricción. |
| `+0x90` | `m_flConstraintWidth` | `float` | Ancho de restricción. |
| `+0x94` | `m_flConstraintSpeedFactor` | `float` | Factor de velocidad en restricción. |
| `+0x98` | `m_vecAbsOrigin` | `ar::Vec3` | Posición de los pies del jugador en `su` $\leftrightarrow$ `FaithState::origin`. |

### 4.3. Tabla Confirmada de Mapeo de Botones (`Source IN_*` $\rightarrow$ `ar::FaithButton`)

Confirmamos la propuesta del contrato `mod/include/ar/faith_movement.h`:

| Bit en Source (`in_buttons.h`) | Valor Hex | Bit en `ar::FaithButton` | Acción en ApertureRunner |
|---|---|---|---|
| `IN_JUMP` (`1 << 1`) | `0x00000002` | `kFaithButtonUp` (`1 << 0`) | Saltar, iniciar/mantener wallrun, wallclimb, subir borde (*ledge climb*). |
| `IN_DUCK` (`1 << 2`) | `0x00000004` | `kFaithButtonDown` (`1 << 1`) | Agacharse, deslizarse (*slide*), encogerse en el aire (*coil*), *Skill Roll* al aterrizar. |
| `IN_RELOAD` (`1 << 13`) | `0x00002000` | `kFaithButtonTurn180` (`1 << 2`) | Giro rápido de 180° en suelo/aire y giro en pared durante *wallclimb*. |
| `IN_WALK` (`1 << 18`) | `0x00040000` | `kFaithButtonWalk` (`1 << 3`) | Caminar despacio sin entrar en sprint. |
| `IN_USE` (`1 << 5`) | `0x00000020` | *(Intacto en el motor)* | Recoger `box` (cubo) y activar interacciones en `testchmb_a_00`. |

Cálculo determinista de `buttons_pressed`:
```cpp
uint32_t cur_buttons  = MapSourceButtonsToFaith(move_data->m_nButtons);
uint32_t prev_buttons = MapSourceButtonsToFaith(move_data->m_nOldButtons);
input.buttons         = cur_buttons;
input.buttons_pressed = cur_buttons & ~prev_buttons;
```

---

## 5. Comprobación de los Offsets de `client.dll` de `LESSONS.md` (Build `19017868`)

Se revisaron todos los offsets documentados por Benja en `.agents/memory/LESSONS.md` y `src/true_ingame_v2/true_ingame_bridge_v2.cpp` para `portal/bin/client.dll` (`Timestamp = 0x68362D89`, build `19017868`):

| Símbolo / Estructura | RVA / Offset en `LESSONS.md` | Tipo y Tamaño | Estado en Build `19017868` | Recomendación para `mod/plugin/client/` |
|---|---|---|---|---|
| `MainViewOrigin` (`g_vecRenderOrigin`) | `client.dll + 0x0050F484` | `ar::Vec3` (`12 B`) | **VERIFICADO** (`0x68362D89`) | Contiguo a `MainViewAngles`. Útil para diagnóstico y lectura rápida, pero para modificar la cámara (`G3`) es preferible aplicar los offsets de `ar::camera::CameraOutput` en `OverrideView` (`IClientMode` slot 16/18) o `CalcView`. |
| `MainViewAngles` (`g_vecRenderAngles`) | `client.dll + 0x0050F490` | `ar::Vec3` (`12 B`) | **VERIFICADO** (`0x68362D89`) | Contiguo a `MainViewOrigin` (`24 B` totales desde `+0x0050F484`). |
| `s_pLocalPlayer` (`C_Portal_Player*`) | `client.dll + 0x004D78F0` | `void*` (`4 B`) | **VERIFICADO** (`0x68362D89`) | Puntero directo al `C_Portal_Player` local. **Alternativa preferida sin offset fijo**: `VClientEntityList003->GetClientEntity(1)`. |
| `cl_entitylist` | `client.dll + 0x004C6624` (en `true_ingame_bridge_v2.cpp`) / `+0x004DD00C` (nota en `B4`) | Tabla `CEntInfo` (stride `0x10`) | **REEMPLAZADO POR INTERFAZ** | **Importante**: Existía discrepancia entre la nota documental (`+0x004DD00C`) y el código (`+0x004C6624`). En `mod/plugin/client/` **no debemos usar este RVA fijo**: se usa siempre la interfaz oficial `CreateInterface("VClientEntityList003", nullptr)`. |
| `C_Prop_Portal::m_vecOrigin` | `Entity + 0x028C` | `ar::Vec3` (`12 B`) | **VERIFICADO** (`0x68362D89`) | Posición del portal en coordenadas Source (`su`). |
| `C_Prop_Portal::m_angRotation` | `Entity + 0x0298` | `ar::Vec3` (`12 B`) | **VERIFICADO** (`0x68362D89`) | Orientación `(pitch, yaw, roll)` del portal en grados. |
| `C_Prop_Portal::m_hLinkedPortal` | `Entity + 0x0E00` | `uint32_t` (`EHANDLE`) | **VERIFICADO** (`0x68362D89`) | Handle del portal enlazado (`index = handle & 0x7FF`, `0xFFFFFFFF` si no está enlazado). |
| `C_Prop_Portal::m_bIsPortal2` | `Entity + 0x0E05` | `uint8_t` (`bool`) | **VERIFICADO** (`0x68362D89`) | `0` = Portal Azul (`portal_blue_*`), `1` = Portal Naranja/Rojo (`portal_red_*`). |
| `C_Prop_Portal::m_bActivated` | `Entity + 0x0E06` | `uint8_t` (`bool`) | **VERIFICADO** (`0x68362D89`) | `1` cuando el portal está abierto y activo en `testchmb_a_00`. |

---

## 6.Cómo Verificarlo en Portal (Build `19017868`)

1. Compilar en Win32/x86 con `mod\build.bat`.
2. Ejecutar las pruebas unitarias autónomas (`ctest --test-dir build/x86 -C Release --output-on-failure`), que validan:
   - La intercepción de vtable `__thiscall` de `client.dll!GameMovement001` (`ClientPredictionHook`).
   - El mapeo de `Source IN_*` a `ar::FaithButton` y detección de flancos `buttons_pressed`.
   - La conversión bidireccional entre `CMoveData` y `ar::FaithInput` / `ar::FaithState`.
3. En el juego (`launch-portal-test` sobre `testchmb_a_00` con `-condebug`), comprobar en `portal/console.log` que `ClientPredictionHook` localiza `client.dll!GameMovement001` y `VClient017` sin errores ni caídas de FPS.
