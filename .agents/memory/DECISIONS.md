# Registro de Decisiones (ADR)

> Documenta aquí decisiones clave de diseño, arquitectura o herramientas para que otras instancias de Antigravity en otras computadoras o cuentas respeten los acuerdos previos.

---

## ADR-001: Memoria y Personalizaciones Basadas en Git (`.agents/` + `AGENTS.md`)
- **Fecha**: 2026-11-01
- **Estado**: Aceptado
- **Contexto**: Se requiere que múltiples instancias de Antigravity (en distintas computadoras y/o cuentas) colaboren manteniendo el contexto y aprendiendo mutuamente usando GitHub Desktop.
- **Decisión**:
  1. Usar `AGENTS.md` en la raíz con inclusiones `@[...](...)` apuntando a `.agents/memory/MEMORY.md`, `.agents/memory/HANDOFF.md` y `.agents/memory/LESSONS.md` para inyección automática de contexto en cada sesión sin depender del sistema operativo.
  2. Almacenar todas las skills aprendidas en `.agents/skills/` y registrarlas en `.agents/skills.json`.
  3. Exportar artefactos importantes de conversaciones locales hacia `docs/artifacts/` cuando deban compartirse entre dispositivos.

---

## ADR-002: Arquitectura Multi-Instancia para Mod Pass-Through (Portal en Mirror's Edge - `edge_p_rooftop_01`)
- **Fecha**: 2026-11-01
- **Estado**: Aceptado
- **Contexto**: Se necesita un flujo de trabajo colaborativo entre distintas instancias de Antigravity para crear un mod Pass-Through que permita jugar con el personaje y físicas de *Portal* (Chell + Portal Gun) dentro del mundo de *Mirror's Edge*, limitado estrictamente a la primera azotea del Prólogo (`Edge_p` — `edge_p_rooftop_01`).
- **Decisión**:
  1. **Rol de Motores**: *Portal* (`hl2.exe`) corre como Backend de Simulación Física/Portales sobre el mapa proxy `me_edge_rooftop_01.bsp` con un plugin VSP (`IServerPluginCallbacks`); *Mirror's Edge* (`MirrorsEdge.exe`) corre como Frontend Visual en `Edge_p` con `TdPlayerPawn` en `PHYS_None` controlado vía hook `UObject::ProcessEvent` + overlay `D3D9`.
  2. **Contratos Inmutables en Git**: `src/common/ipc_bridge_protocol.h` (memoria compartida `Local\Antigravity_Portal_ME_Bridge_v1` con `SeqLock`) y `levels/edge_p_rooftop_01/slice_manifest.json` desacoplan el trabajo en 5 Fases independientes para que cualquier computadora pueda avanzar y validar con Node.js (`verify-bridge-pipeline.js`) sin requerir que todas las máquinas tengan ambos juegos instalados.

---

## ADR-003: Segundo Flujo de Trabajo Paralelo en `Stormdrain_p` (`stormdrain_p_sluice_01` — Rampas a 45° y Entidades Interactivas)
- **Fecha**: 2026-11-01
- **Estado**: Aceptado
- **Contexto**: Se solicitó crear un segundo flujo de trabajo similar para el mismo caso (jugar con el personaje de *Portal* en una sección de un nivel de *Mirror's Edge*) sin eliminar ni alterar el flujo anterior (`edge_p_rooftop_01`).
- **Decisión**:
  1. Mantener intacto el Flujo #1 (`portal-mirrors-edge-passthrough` en `Edge_p`) y añadir en paralelo el Flujo #2 (`portal-mirrors-edge-stormdrain`) centrado en la **Gran Esclusa Vertical del Capítulo 2 (`Stormdrain_p` — `stormdrain_p_sluice_01`)**.
  2. Incorporar en el Flujo #2 soporte para geometría oblicua en `.vmf` (`wedge_45_plus_x` para catapultas *Angled Fling* a 45°), sincronización del **Cubo de Compañía (`prop_weighted_cube`)**, un **Botón de Piso (`prop_floor_button`)** acoplado al evento Kismet `OpenSluiceExitGate_01` de UE3, y barreras **Emancipation Grill (`prop_portal_cleanser`)** mediante el contrato extendido `src/common/ipc_stormdrain_bridge_v2.h`.

---

## ADR-004: Rediseño Ultraligero "Lean Vertical Slice" en 3 Pasos (`portal-me-lean-slice`)
- **Fecha**: 2026-11-01
- **Estado**: Aceptado (Ruta Recomendada para Implementación Rápida)
- **Contexto**: A partir de la comparativa entre los Flujos #1 y #2, el usuario solicitó rediseñar el flujo para que sea lo más rápido y sencillo posible de realizar para un *Vertical Slice* funcional.
- **Decisión**:
  1. Reducir el pipeline de 5 fases a **3 pasos** en la nueva skill `.agents/skills/portal-me-lean-slice/SKILL.md`.
  2. Eliminar la dependencia del Source SDK 2007 (VSP) y de MinHook (`UObject::ProcessEvent`), reemplazándolos por:
     - El comando nativo `Ghost` de Unreal Engine 3 en `TdInput.ini` (`F10`) para desactivar gravedad/colisiones de Faith con cero código de hooking.
     - Un único ejecutable externo en un solo archivo C++ (`src/lean_bridge/lean_external_bridge.cpp`) que sincroniza `Origin`/`Angles` a $120\text{ Hz}$.
  3. Combinar la carga instantánea de la azotea inicial de `Edge_p` con su **rampa roja de salto a $45^\circ$** (`rooftop_red_jump_ramp_45`) en un mapa mínimo de solo **5 sólidos** (`levels/edge_p_lean_slice/me_edge_lean_slice.vmf`) para lograr el salto parabólico a la azotea vecina sin tocar Kismet ni *Level Streaming*.

---

## ADR-005: Post-Mortem de `primer-intento` y Arquitectura de `segundo-intento` (Opción 1: True In-Game Pass-Through en Mirror's Edge)
- **Fecha**: 2026-11-02
- **Estado**: Aceptado (Rama `segundo-intento`)
- **Contexto**: Al evaluar en vivo el resultado de `primer-intento` (`ADR-004`), se detectó que el mod se desvió de la intención original y de las referencias reales en internet: en vez de incluir al personaje/arma/portales de *Portal* dentro del mundo de *Mirror's Edge* en una sola experiencia unificada, solo copiaba coordenadas `(X, Y, Z)` obligando al usuario a jugar en la ventana de *Portal* (una caja gris) mientras miraba *Mirror's Edge* en otra ventana sin ver la Portal Gun ni los portales.
- **Fallos de Razonamiento Identificados en `primer-intento`**:
  1. **Confundir simplicidad arquitectónica (Lean) con eliminar el valor visual/jugable central**: Recortar el overlay D3D9 y el reenvío de inputs dejó al proyecto sin su característica principal (ver la Portal Gun, a Chell y los portales dentro de *Mirror's Edge*).
  2. **Sesgo de validación por consola (`--self-test` $\neq$ experiencia real)**: Optimizar para que los tests matemáticos de coordenadas pasaran en terminal hizo perder de vista la experiencia visual y de control del jugador humano.
  3. **Inversión del rol Frontend/Backend**: Obligar al jugador a controlar la ventana de *Portal* (Backend físico) en vez de jugar directamente dentro de *Mirror's Edge* (Frontend visual).
  4. **Sincronizar solo la cámara en vez de las entidades de Portal**: No se sincronizaron las posiciones/normales de los portales Azul y Naranja ni el renderizado de la Portal Gun / Chell en el viewport de *Mirror's Edge*.
- **Decisión para `segundo-intento` (Opción 1)**:
  1. **Control 100% desde la ventana de *Mirror's Edge***: El usuario juega únicamente en *Mirror's Edge*; el puente captura los inputs de teclado/ratón/clics en *Mirror's Edge* y los encamina a *Portal* en segundo plano.
  2. **Renderizado Visual In-Game en *Mirror's Edge* (D3D9 Proxy DLL / Overlay 3D sincronizado)**: Dibujar dentro del viewport de *Mirror's Edge* la **Portal Gun en 1ª persona**, la **retícula de estado azul/naranja**, los **portales Azul y Naranja proyectados en 3D** sobre las superficies reales de la azotea de `Edge_p` y la representación de **Chell**.
  3. **Supresión de conflicto con Faith**: Ocultar el cuerpo/manos nativos de Faith en 1ª persona mientras el modo Portal está activo para evitar superposición visual y temblores.
