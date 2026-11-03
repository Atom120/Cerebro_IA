---
name: portal-me-lean-slice
status: archivado
description: >-
  [status: archivado — ADR-006, conservado como referencia] Flujo de trabajo rediseñado y ultraligero
  (Lean Vertical Slice en 3 Pasos) para lograr el mod
  Pass-Through de Portal dentro de Mirror's Edge en el menor tiempo posible. Combina la simplicidad
  de la azotea inicial de Edge_p con la rampa de salto a 45 grados, eliminando Source SDK, Kismet y
  hooks de D3D9 mediante el modo Ghost nativo de UE3 y un único puente externo en C++ (Cero SDKs).
---

> [!IMPORTANT]
> **Estado: ARCHIVADO (`status: archivado` por `ADR-006`, 2026-11-03).**
> El proyecto activo es **ApertureRunner** (*Mirror's Edge dentro de Portal*, mapa `testchmb_a_00`; ver [`PLAN_EQUIPO.md`](../../../PLAN_EQUIPO.md)). Esta skill se conserva sin borrarse como referencia de calibración de offsets GOG + Steam (`lean_external_bridge.cpp`). Consulta [`docs/artifacts/2026-11-03-archivado-portal-me-y-reutilizacion-adr006.md`](../../../docs/artifacts/2026-11-03-archivado-portal-me-y-reutilizacion-adr006.md).

# Flujo Rediseñado: "Lean Vertical Slice" en 3 Pasos (Máxima Velocidad y Mínima Complejidad)

Esta skill es el resultado de destilar lo mejor de los Flujos #1 (`Edge_p`) y #2 (`Stormdrain_p`), **eliminando ~75% de la complejidad técnica** para lograr un **Vertical Slice 100% funcional** en horas en lugar de semanas.

---

## 1. ¿Qué se simplificó respecto a los flujos anteriores?

| Componente | Antes (Flujos #1 y #2 — 5 Fases) | Ahora (Lean Vertical Slice — **3 Pasos**) | Ahorro de Complejidad |
| :--- | :--- | :--- | :--- |
| **Desactivar físicas de Faith en UE3** | Inyectar DLL con `MinHook` e interceptar `UObject::ProcessEvent` (`TdPlayerTick`) | **Comando nativo `Ghost` en `TdInput.ini`** (`F10 = Ghost \| FOV 90`) | **Cero C++ de hooking en UE3** |
| **Leer estado de Chell en Portal** | Compilar un plugin `VSP.dll` contra todo el **Source SDK 2007** | **Proceso externo ligero (`lean_external_bridge.cpp`)** con `ReadProcessMemory` | **Cero descarga de Source SDK 2007** |
| **Control / Input del Jugador** | Capturar teclado en UE3 -> Memoria Compartida -> Reconstruir `CUserCmd` en Source | **Input directo en Portal** (la Portal Gun, sonidos, físicas y portales funcionan de fábrica) | **Cero inyección de `CUserCmd`** |
| **Escenario y Mecánica del Slice** | O bien solo paredes de $90^\circ$ (#1), o bien ir hasta `Stormdrain_p` con Cubo y Kismet (#2) | **Azotea inicial de `Edge_p` + Su rampa roja a $45^\circ$ portaleable** (permite el salto parabólico a la azotea vecina desde el segundo 0) | **Cero Kismet, cero Level Streaming, carga en 3 s** |
| **Scripts de Automatización** | Generador `.vmf` y verificador separados | **1 solo comando unificado**: `build-and-verify-lean-slice.js` | **Validación en 1 paso** |

---

## 2. Los 3 Pasos Únicos del Flujo Rediseñado

### Paso 1: Generar, Compilar (`.bsp`) y Verificar el Mapa Mínimo de 5 Sólidos (`1 comando`)
Ejecuta el script todo-en-uno que lee [`levels/edge_p_lean_slice/slice_manifest.json`](../../../levels/edge_p_lean_slice/slice_manifest.json), construye [`levels/edge_p_lean_slice/me_edge_lean_slice.vmf`](../../../levels/edge_p_lean_slice/me_edge_lean_slice.vmf) (con solo **5 brushes**: piso, muro de la torre, **rampa roja a $45^\circ$**, azotea meta y obstáculo HVAC), valida la matemática y, si detecta *Portal* instalado en Steam, **invoca automáticamente `vbsp.exe -game ".../Portal/portal"`** para producir [`levels/edge_p_lean_slice/me_edge_lean_slice.bsp`](../../../levels/edge_p_lean_slice/me_edge_lean_slice.bsp):
```powershell
node .agents/skills/portal-me-lean-slice/scripts/build-and-verify-lean-slice.js
```
Si deseas compilarlo manualmente con `vbsp.exe` (nota el flag obligatorio `-game` apuntando a la carpeta donde reside `gameinfo.txt`):
```powershell
& "C:/Program Files (x86)/Steam/steamapps/common/Portal/bin/vbsp.exe" -game "C:/Program Files (x86)/Steam/steamapps/common/Portal/portal" levels/edge_p_lean_slice/me_edge_lean_slice.vmf
```

### Paso 2: Activar el Modo `Ghost` Nativo en *Mirror's Edge* (`0 líneas de código`)
En lugar de programar un hook sobre `UObject::ProcessEvent`, añade esta línea en `Documents/EA Games/Mirror's Edge/TdGame/Config/TdInput.ini`:
```ini
Bindings=(Name="F10",Command="Ghost | FOV 90",Control=False,Shift=False,Alt=False)
```
Al cargar el Prólogo (`Edge_p`) y presionar **`F10`**, *Mirror's Edge* apaga de forma nativa la gravedad, las colisiones y la máquina de estados de parkour de Faith, dejando su cámara lista para ser movida libremente desde fuera.

### Paso 3: Ejecutar el Puente Externo Único (`lean_external_bridge.cpp`)
Un único archivo C++ sin dependencias ([`src/lean_bridge/lean_external_bridge.cpp`](../../../src/lean_bridge/lean_external_bridge.cpp)) ya viene precalibrado con los **offsets reales extraídos de `MirrorsEdge.exe` (GOG v1.01 DRM-Free)** y de **`client.dll` de *Portal* (Steam)**:

| Binario / Motor | Símbolo / Estructura | Offset / Dirección Exacta | Notas Técnicas |
| :--- | :--- | :--- | :--- |
| `MirrorsEdge.exe` (GOG) | `ImageBase` (`ASLR = false`) | `0x00400000` | Base fija en memoria, secciones `.text`/`.data` sin DRM |
| `MirrorsEdge.exe` (GOG) | `GEngine` (`UTdGameEngine*`) | `0x01FF8B20` (VA) | `GWorld = 0x01FFBCA4`, `GObjObjects = 0x0204A35C`, `GNames = 0x0204E7D8` |
| `MirrorsEdge.exe` (GOG) | Cadena hasta `Pawn` | `+0x2CC` -> `[0]` -> `+0x3C` -> `+0x1C0` | `GEngine->GamePlayers.Data[0]->Actor (ATdPlayerController)->Pawn` |
| `MirrorsEdge.exe` (GOG) | `AActor::Location` / `Rotation` | `+0xE8` (`FVector`) / `+0xF4` (`FRotator`) | Coordenadas en UU (`float[3]`) y rotación en URU (`int32[3]`) |
| `Portal/portal/bin/client.dll` | `MainViewOrigin` + `MainViewAngles` | `client.dll + 0x0050F484` (`24 bytes`) | Bloque contiguo (`float[6]`): `Origin (X,Y,Z)` en `+0x50F484` y `Angles (P,Y,R)` en `+0x50F490` |
| `Portal/portal/bin/client.dll` | `s_pLocalPlayer` (`C_Portal_Player*`) | `client.dll + 0x004D78F0` | `m_vecVelocity = +0xF0`, `m_vecAbsOrigin = +0x25C`, `m_vecOrigin = +0x28C` |
| `Edge_p.umap` (GOG) | Checkpoints reales descompresos | `(32208.5, -4811.3, 15261.2)` (Piso `Z=15180`) | `PlayerStart` Azotea 1 (`Yaw=32768`) y `1Pt1_4PlayerStart` en `(19309.4, -3009.5, 8520.0)` |

---

## 3. El Recorrido Jugable del Vertical Slice (30 segundos de Gameplay)

1. Apareces en la azotea inicial de `Edge_p`.
2. Disparas el **Portal Azul** en la **rampa inclinada a $45^\circ$** del borde de la azotea (`rooftop_red_jump_ramp_45`).
3. Subes o disparas el **Portal Naranja** en el piso (`rooftop_floor`) tras impulsarte desde la torre del ascensor (`elevator_tower_wall`).
4. Caes verticalmente en el Portal Naranja y sales **catapultado a $45^\circ$ desde la rampa de la azotea**, cruzando toda la calle en el aire dentro del mundo visual de *Mirror's Edge* hasta aterrizar en la azotea del edificio vecino (`target_across_street_roof`).
