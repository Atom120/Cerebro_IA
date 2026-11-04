# Walkthrough - Tarea Z4: `ServerTraceProvider` (`EngineTraceServer003`)

**Responsable:** Zarek  
**Rama:** `zarek/Z4-trace-provider`  
**Estado:** Implementado y verificado (5/5 tests Win32/x86 aprobados)

---

## 1. Objetivo de la Tarea Z4
Implementar el proveedor de trazas espaciales real del servidor (`ar::ITraceProvider`) en `mod/plugin/server/server_trace_provider.h/.cpp` conectándolo con la interfaz del motor Source `EngineTraceServer003` (`ar::source::IEngineTrace`) obtenida mediante `interfaceFactory` de `engine.dll`.

Esto permite que el motor de físicas y parkour de Faith (`mod/faith_move/`, implementado en P1/P2 por Sebas) realice consultas de colisión (`TraceHull` y `TraceRay`) contra la geometría real del mapa de Portal (`testchmb_a_00`), detectando piso, paredes, techos y escalones con la máscara exacta `MASK_PLAYERSOLID`.

---

## 2. Cambios Implementados

### A. Definición binaria de estructuras Source Engine
En `mod/plugin/server/source_interfaces.h`:
- `VectorAligned`: Vector con alineación estricta de 16 bytes (`alignas(16)`) requerida por las instrucciones SSE (`movaps`) de `engine.dll`.
- `Ray_t`: Estructura para rayos y swept bounding boxes (`Init(start, end)` y `Init(start, end, mins, maxs)`), incluyendo `m_pWorldAxisTransform` inicializado a `nullptr`.
- `cplane_t`, `csurface_t`, `CBaseTrace`, `trace_t`: Estructuras con layout binario idéntico a Source 2013 / Portal 1 (build 19017868).
- `ITraceFilter`: Interfaz virtual (`ShouldHitEntity`, `GetTraceType`).
- `IEngineTrace`: Interfaz de Source (`EngineTraceServer003`) con ranura 5 (`TraceRay`).
- `kMaskPlayerSolid`: Máscara exacta `0x201400Bu` (`CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE`).

### B. Módulo `ServerTraceProvider`
En `mod/plugin/server/server_trace_provider.h` y `mod/plugin/server/server_trace_provider.cpp`:
- Implementa `ar::ITraceProvider`:
  - `TraceHull(start, end, mins, maxs)`: Configura el swept box con `Ray_t`, filtra la entidad del jugador con `TraceFilterPlayerOnly`, ejecuta `engine_trace_->TraceRay` y mapea los resultados a `ar::TraceResult` (`fraction`, `end_pos`, `normal`, `start_solid`, `all_solid`, `kind`).
  - `TraceRay(start, end)`: Traza de rayo punto a punto optimizada.
- `TraceFilterPlayerOnly`: Filtro que compara contra `ignore_player_` para evitar que Faith colisione consigo misma.

### C. Integración en `ApertureRunnerServerPlugin`
En `mod/plugin/server/server_plugin.h` y `mod/plugin/server/server_plugin.cpp`:
- En `Load()`, adquiere `EngineTraceServer003` mediante `interfaceFactory` de `engine.dll` y lo asigna al `ServerTraceProvider`.
- En `Unload()`, desvincula el puntero de manera segura.
- En `movement_hook.cpp`, actualiza `ignore_player` con el puntero `pPlayer` recibido en cada tick interceptado.

### D. Ampliación del Arnés de Pruebas Autónomo
En `mod/plugin/tests/test_plugin_load.cpp`:
- Se implementó `MockEngineTrace` y `MockEngineFactory` simulando piso en $Z=0$ y pared vertical en $X=100$.
- 6/6 comprobaciones automatizadas:
  1. `TraceRay` hacia abajo impactando en $Z=0$ (`fraction = 0.5`, `normal = (0, 0, 1)`, `kind = World`).
  2. `TraceHull` con bounding box de jugador impactando en $Z=0$ (`fraction = 0.5`, `normal = (0, 0, 1)`, `kind = World`).
  3. `TraceHull` horizontal impactando pared en $X=100$ (`fraction = 0.42`, `normal = (-1, 0, 0)`, `end_pos.x = 84.0`).
  4. `TraceRay` libre sin colisión (`fraction = 1.0`, `kind = None`).
  5. `TraceHull` iniciando dentro de un sólido (`start_solid = true`, `fraction = 0.0`).
  6. Filtrado de entidad del jugador mediante `SetIgnorePlayer`.

---

## 3. Resultados de Compilación y Pruebas

Ejecutado con `mod\build.bat Release` y `mod\build.bat Debug`:
```
Test project D:/Proyecto-AI/Cerebro_IA/build/x86
    Start 1: test_contracts
1/5 Test #1: test_contracts ...................   Passed    0.08 sec
    Start 2: test_core
2/5 Test #2: test_core ........................   Passed    0.08 sec
    Start 3: test_faith_move
3/5 Test #3: test_faith_move ..................   Passed    0.06 sec
    Start 4: test_plugin_load
4/5 Test #4: test_plugin_load .................   Passed    1.10 sec
    Start 5: test_me_telemetry
5/5 Test #5: test_me_telemetry ................   Passed    0.07 sec

100% tests passed, 0 tests failed out of 5
OK: build\x86 (Release, Win32)
```

Salida de `test_plugin_load.exe`:
```
[test_plugin_load] Cargando dinamicamente: build\x86\plugin\Release\aperture_runner.dll
[ApertureRunner] Plugin VSP cargado con exito.
[ApertureRunner] EngineTraceServer003 adquirido exitosamente (Z4).
[ApertureRunner] IGameMovement::ProcessMovement hookeado exitosamente en passthrough (Hito H1).
[test_plugin_load] Verificando consultas espaciales de ServerTraceProvider (Z4)...
[test_plugin_load] 6/6 comprobaciones de ServerTraceProvider aprobadas.
[ApertureRunner] LevelInit: testchmb_a_00
[ApertureRunner] ServerActivate listo en mapa testchmb_a_00
[ApertureRunner] LevelShutdown en mapa testchmb_a_00
[ApertureRunner] VTable de IGameMovement restaurada al original.
[ApertureRunner] Plugin descargado limpiamente.
[test_plugin_load] Exito rotundo: Z4 ServerTraceProvider y Z3 Hook validados al 100%.
```
