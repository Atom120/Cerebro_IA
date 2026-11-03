---
name: portal-mirrors-edge-stormdrain
description: >-
  Segundo flujo de trabajo modular y multi-instancia (complementario a portal-mirrors-edge-passthrough)
  para jugar con el personaje y físicas de Portal (Chell + Portal Gun + Cubo de Compañía) dentro de la
  Gran Esclusa Vertical del Capítulo 2 de Mirror's Edge (Stormdrain_p — stormdrain_p_sluice_01).
  Incluye soporte para rampas inclinadas a 45 grados (Angled Flings), sincronización de objetos físicos
  interactivos (prop_weighted_cube + prop_floor_button -> Kismet) y barreras Emancipation Grill.
---

# Flujo de Trabajo #2 Multi-Instancia: Mod Pass-Through Avanzado en `Stormdrain_p` (Esclusa Vertical + Catapultas a 45° + Cubo de Compañía)

Esta skill define el **Segundo Flujo de Trabajo** del repositorio para jugar con el personaje de **Portal (Chell + Portal Gun)** dentro de una sección única de **Mirror's Edge**, coexistiendo intactamente con el Flujo #1 (`portal-mirrors-edge-passthrough` en `Edge_p`).

---

## 1. Diferencias Clave entre el Flujo #1 y el Flujo #2

| Característica | Flujo #1 (`portal-mirrors-edge-passthrough`) | Flujo #2 (`portal-mirrors-edge-stormdrain`) |
| :--- | :--- | :--- |
| **Nivel y Sección Única** | Prólogo (`Edge_p` — Azotea Inicial `edge_p_rooftop_01`) | Cap. 2 *Jacknife* (`Stormdrain_p` — Gran Esclusa Vertical `stormdrain_p_sluice_01`) |
| **Geometría Proxy `.vmf`** | Cajas ortogonales AABB (`box`) | Cajas ortogonales + **Cuñas/Paneles inclinados a 45° (`wedge_45_plus_x`)** |
| **Mecánica Principal** | Salto simple desde la torre del ascensor (*Vertical-to-Horizontal Fling*) | **Catapulta Parabólica a 45° (*Angled Long-Jump Fling*)** a través de un abismo de 40 m + **Puzzle con Cubo de Compañía** |
| **Entidades Sincronizadas** | `CPortal_Player` + 2 Portales (`prop_portal`) | `CPortal_Player` + 2 Portales Oblicuos + **`prop_weighted_cube`** + **`prop_floor_button` -> Evento Kismet UE3** + **Fizzlers** |
| **Contrato Binario C/C++** | [`src/common/ipc_bridge_protocol.h`](../../../src/common/ipc_bridge_protocol.h) (`PME1`) | [`src/common/ipc_stormdrain_bridge_v2.h`](../../../src/common/ipc_stormdrain_bridge_v2.h) (`PME2`, SharedMemory + UDP `27099`) |

---

## 2. Protocolo de Trabajo entre Instancias (5 Fases Independientes)

### Paso A: Al Iniciar Sesión en Cualquier Computadora
1. Revisa [`.agents/memory/HANDOFF.md`](../../memory/HANDOFF.md) para ver qué Fase del Flujo #1 o del Flujo #2 está activa.
2. Ejecuta el validador offline del Flujo #2 (no requiere tener los juegos instalados):
   ```powershell
   node .agents/skills/portal-mirrors-edge-stormdrain/scripts/verify-stormdrain-pipeline.js
   ```

### Paso B: Las 5 Fases Modulares del Flujo #2 (`stormdrain_p_sluice_01`)

#### Fase 1: Generación de Geometría Oblicua (45°) y Entidades de Puzzle (`.vmf`)
- **Manifiesto de entrada**: [`levels/stormdrain_p_sluice_01/slice_manifest.json`](../../../levels/stormdrain_p_sluice_01/slice_manifest.json)
- **Generador automatizado**:
  ```powershell
  node .agents/skills/portal-mirrors-edge-stormdrain/scripts/generate-stormdrain-vmf.js
  ```
- **Salida en Git**: [`levels/stormdrain_p_sluice_01/me_stormdrain_sluice_01.vmf`](../../../levels/stormdrain_p_sluice_01/me_stormdrain_sluice_01.vmf)
- **Qué construye**:
  - Pozo profundo de hormigón blanco de `Stormdrain_p` (`sluice_basin_floor`) y muros de `24 m` de altura.
  - **Rampa monolítica a 45° (`sluice_angled_fling_ramp_45`)** con vector normal $\vec{n} = (\frac{\sqrt{2}}{2}, 0, \frac{\sqrt{2}}{2})$ para redirigir caídas verticales en vuelos parabólicos.
  - Entidades nativas de Portal: `prop_weighted_cube`, `prop_floor_button` y dos barreras `prop_portal_cleanser` (*Emancipation Grills*) que sellan la sección única al norte y al sur.

#### Fase 2: Backend de Simulación VSP v2 en Portal (`portal_stormdrain_vsp_v2.cpp`)
- **Archivo base**: [`src/backend_source/portal_stormdrain_vsp_v2.cpp`](../../../src/backend_source/portal_stormdrain_vsp_v2.cpp)
- **Objetivo**:
  1. Leer el canal de entrada `PMEInputChannel` (incluyendo la tecla `E` / `PME_IN_USE` para recoger o soltar el Cubo de Compañía).
  2. Rastrear en el servidor de Source la entidad `prop_weighted_cube` y el estado de `prop_floor_button`.
  3. Publicar `PME2StormdrainSimChannel` hacia `Local\Antigravity_Portal_ME_Stormdrain_v2` (o vía socket UDP `127.0.0.1:27099`).

#### Fase 3: Frontend UE3 Hook v2 en Mirror's Edge (`me_stormdrain_hook_v2.cpp`)
- **Archivo base**: [`src/frontend_me/me_stormdrain_hook_v2.cpp`](../../../src/frontend_me/me_stormdrain_hook_v2.cpp)
- **Objetivo**:
  1. Al entrar en la Gran Esclusa de `Stormdrain_p`, poner a `TdPlayerPawn` en `PHYS_None` y aplicar la posición/velocidad/cámara de Chell convertida desde `PME2_ANCHOR_ME = (-4500, 12800, -1600)`.
  2. Actualizar la malla estática/dinámica del Cubo de Compañía en el mundo de *Mirror's Edge*.
  3. Cuando `floor_button_pressed == 1`, invocar el evento de Kismet `OpenSluiceExitGate_01` en `Stormdrain_p` para abrir la compuerta de salida.

#### Fase 4: Renderizado D3D9 de Portales Oblicuos (45°), Cubo y Fizzlers
- **Objetivo**:
  1. Renderizar los elipsoides Azul y Naranja orientados según la normal 3D completa (incluyendo la inclinación de `45°` sobre la rampa `sluice_angled_fling_ramp_45`).
  2. Renderizar las cortinas semitransparentes cian de los dos *Emancipation Grills* (`fizzler_north_boundary` y `fizzler_south_boundary`) y el indicador visual del botón pesado en la plataforma este.

#### Fase 5: Prueba Integrada del Puzzle "Stormdrain Angled Fling"
- **Recorrido de validación in-game**:
  1. Iniciar en la cornisa oeste (`west_spawn_ledge`, $Z = 1143\text{ UU}$) junto al Cubo de Compañía.
  2. Disparar el **Portal Azul** en la rampa inclinada a 45° (`sluice_angled_fling_ramp_45`) y el **Portal Naranja** en el fondo del pozo (`sluice_basin_floor`).
  3. Recoger el Cubo de Compañía con `E` (`PME_IN_USE`), saltar al vacío hacia el Portal Naranja y salir catapultado a 45° desde la rampa para aterrizar en la pasarela superior este (`upper_exit_catwalk_east`), depositando el cubo sobre el botón para abrir la esclusa.
