---
name: portal-mirrors-edge-passthrough
description: >-
  Flujo de trabajo modular y multi-instancia para desarrollar el mod Pass-Through que permite jugar
  con el personaje y físicas de Portal (Chell + Portal Gun en Source Engine) dentro de una sección
  única del mundo de Mirror's Edge (Azotea inicial de Edge_p en Unreal Engine 3). Úsala cuando
  trabajes en cualquier fase de extracción de geometría, mapa proxy .vmf/.bsp, plugin VSP de Portal,
  hook UE3/D3D9 de Mirror's Edge o validación del puente IPC entre distintas computadoras.
---

# Flujo de Trabajo Multi-Instancia: Mod Pass-Through (Portal en Mirror's Edge)

Esta skill define el flujo de trabajo estandarizado para construir y evolucionar el **Mod Pass-Through** donde **Portal (Source Engine)** actúa como *Backend de Simulación Física y Portales* y **Mirror's Edge (Unreal Engine 3)** actúa como *Frontend Visual y Mundo*, limitado estrictamente a **una sola sección de un solo nivel**: la **Azotea Inicial de Entrenamiento del Prólogo (`Edge_p` — `edge_p_rooftop_01`)**.

---

## 1. Arquitectura de Desacoplamiento para Múltiples Instancias de Antigravity

Para que distintas instancias de Antigravity (en diferentes PCs, sistemas operativos o cuentas de usuario) puedan avanzar en paralelo o por relevos sin bloquearse aunque una computadora no tenga ambos juegos instalados, el proyecto se divide en **5 Unidades de Trabajo (Fases) independientes** unidas por dos contratos inmutables en Git:

1. **Contrato Geométrico y de Nivel**: [`levels/edge_p_rooftop_01/slice_manifest.json`](../../../levels/edge_p_rooftop_01/slice_manifest.json)
2. **Contrato Binario de Memoria Compartida (IPC)**: [`src/common/ipc_bridge_protocol.h`](../../../src/common/ipc_bridge_protocol.h)

### Reglas Matemáticas del Puente (Congeladas en `ADR-002`)
- **Escala**: En *Mirror's Edge*, `1 UU = 1.0 cm`. En *Portal* (Source), `1 HU = 1.905 cm`.
  - $\text{UU} = \text{HU} \times 1.905$ | $\text{HU} = \text{UU} \times 0.52493438$
- **Lateralidad (Handedness)**:
  - *Mirror's Edge (UE3)* es **Left-Handed Z-Up** (`+X` Adelante, `+Y` Derecha, `+Z` Arriba).
  - *Portal (Source)* es **Right-Handed Z-Up** (`+X` Adelante, `+Y` Izquierda, `+Z` Arriba).
  - Por tanto, **el eje $Y$ se invierte** respecto al punto de anclaje (`PME_ANCHOR_ME = (-14200, 8400, 3200)`), y los ángulos **Pitch y Yaw se invierten** al convertir `QAngle` (grados) a `FRotator` ($\text{URU} = \text{grados} \times \frac{65536}{360}$).

---

## 2. Protocolo de Trabajo entre Instancias (Antes y Después de Cada Sesión)

### Paso A: Al Iniciar Sesión en Cualquier Computadora (Claim)
1. Lee [`.agents/memory/HANDOFF.md`](../../memory/HANDOFF.md) para identificar qué Fase (1 a 5) está activa o pendiente.
2. Verifica o crea [`config/paths.local.json`](../../../config/paths.example.json) a partir de `config/paths.example.json` indicando qué herramientas tiene instalada esta máquina (`full_stack`, `geometry_only`, `backend_source`, `frontend_ue3`).
3. Ejecuta el validador multiplataforma (no requiere tener los juegos instalados):
   ```powershell
   node .agents/skills/portal-mirrors-edge-passthrough/scripts/verify-bridge-pipeline.js
   ```

### Paso B: Ejecución por Fase (Unidades Modulares)

#### Fase 1: Extracción y Generación del Mapa Proxy de la Sección Única (`edge_p_rooftop_01`)
- **Objetivo**: Representar los volúmenes de colisión de la primera azotea de `Edge_p` en un archivo `.vmf` de Source Engine donde las paredes blancas de concreto sean `portalable` y las vallas/conductos metálicos sean `noportal`.
- **Herramienta automatizada en el repo**:
  ```powershell
  node .agents/skills/portal-mirrors-edge-passthrough/scripts/generate-proxy-vmf.js
  ```
- **Salida versionada en Git**: `levels/edge_p_rooftop_01/me_edge_rooftop_01.vmf`.
- **Compilación a `.bsp`** (en la instancia con Source SDK / Portal instalado):
  Ejecutar `vbsp.exe`, `vvis.exe` y `vrad.exe` sobre `me_edge_rooftop_01.vmf` y copiar el `.bsp` resultante a `Portal/portal/maps/me_edge_rooftop_01.bsp` (los binarios `.bsp` se ignoran en Git).

#### Fase 2: Backend de Simulación en Portal (Valve Server Plugin - VSP)
- **Directorio**: `src/backend_source/`
- **Objetivo**: Implementar un plugin nativo `IServerPluginCallbacks` (`portal_me_bridge.dll` + `.vdf` en `portal/addons/`) que:
  1. En `GameFrame(bool simulating)`, lea `PMEInputChannel` desde `Local\Antigravity_Portal_ME_Bridge_v1` y aplique `forward_move`, `side_move`, `mouse_dx/dy` y botones (`PME_IN_ATTACK` = Portal Azul, `PME_IN_ATTACK2` = Portal Naranja) al `CUserCmd` de `CPortal_Player`.
  2. Lea de `CPortal_Player` su posición (`GetAbsOrigin()`), velocidad (`GetAbsVelocity()`), ángulos de cámara (`EyeAngles()`) y de las entidades `prop_portal` su origen, normal y estado activo.
  3. Escriba `PMESimulationChannel` usando el contador atómico `seq` (par = listo).

#### Fase 3: Frontend Visual y Mundo en Mirror's Edge (UE3 + D3D9 Hook)
- **Directorio**: `src/frontend_me/`
- **Objetivo**: Implementar un proxy/hook (`d3d9.dll` o ASI en `MirrorsEdge/Binaries/`) que:
  1. Intercepte `UObject::ProcessEvent` durante `TdPlayerController::PlayerTick` en el nivel `Edge_p`.
  2. Desactive la máquina de estados de parkour nativa de Faith (`TdPlayerPawn->SetPhysics(PHYS_None)`, `bCollideWorld = false`) dentro del volumen de `edge_p_rooftop_01`.
  3. Capture el input crudo del jugador en la ventana de Mirror's Edge y lo publique en `PMEInputChannel`.
  4. Lea `PMESimulationChannel`, convierta coordenadas con `PME_SourceToME_Position()` y `PME_SourceToME_Rotation()`, y sobrescriba `TdPlayerPawn->Location`, `TdPlayerPawn->Velocity` y `TdPlayerController->Rotation`.

#### Fase 4: Renderizado de Portales e Interfaz en Mirror's Edge (D3D9 Overlay)
- **Directorio**: `src/frontend_me/render/`
- **Objetivo**: En el hook `IDirect3DDevice9::EndScene`, transformar `blue_portal` y `orange_portal` a coordenadas de mundo UE3 (`UU`) y dibujar los elipsoides/marcos Azul y Naranja sobre las paredes de `Edge_p` junto con la retícula de la Portal Gun.

#### Fase 5: Pruebas de Integración In-Game y Calibración de Offsets
- **Objetivo**: Ejecutar ambos procesos en paralelo, validar que disparar un portal en la torre del ascensor de `Edge_p` y otro en el piso conserve el momento físico de Chell al saltar, y ajustar cualquier offset en `slice_manifest.json`.

### Paso C: Al Terminar la Sesión (Handoff a Otra Instancia)
1. Ejecuta siempre `node .agents/skills/portal-mirrors-edge-passthrough/scripts/verify-bridge-pipeline.js` para asegurar que el contrato IPC y el `.vmf` siguen siendo válidos.
2. Usa la skill `session-handoff` para actualizar `.agents/memory/HANDOFF.md` y `.agents/memory/SESSIONS.md` indicando qué fase o subtarea queda lista y qué debe hacer la siguiente computadora.
