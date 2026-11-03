# Estado de Relevo Activo (Active Handoff)

> **Instrucción para Antigravity**: Lee este archivo al iniciar cualquier conversación para retomar exactamente donde se quedó la última sesión/computadora. Actualízalo al terminar una tarea relevante o cuando el usuario indique que cambiará de dispositivo/cuenta.

## Última Actualización
- **Fecha**: 2026-11-03
- **Quién**: Sebas (Claude Code) — Repositorio `Cerebro_IA` (`main`)
- **Estado general**: **Cambio de dirección (ADR-006).** El proyecto ahora es *Mirror's Edge dentro de Portal*: Faith en el primer mapa de Portal 1 (`testchmb_a_00`). **El plan, el reparto por persona, las dependencias y los agentes están en [`PLAN_EQUIPO.md`](../../PLAN_EQUIPO.md). Léelo antes de proponer cambios y trabaja solo en las tareas y carpetas de tu persona.**
- **Equipo**: Sebas (líder/integración), Zarek (plugin servidor), Peña (movimiento de Faith), Gonzalo (cliente y cámara), Antonio (assets de Faith), Benja (telemetría ME, contrato del mapa y QA).

## Próximos Pasos (semana 1)
- Sebas: S0 (agentes en `.agents/agents/`), S1 (`mod/` + CMake + CI), **S2 (contratos, prioridad máxima)**.
- Zarek: Z1, Z2. Peña: P1, P2. Gonzalo: G1, G3. Antonio: A1, A2. Benja: **B1**, B2, B4.

---

## Relevo anterior (archivado, ADR-005, solo referencia)
### Última actualización anterior
- **Fecha**: 2026-11-03
- **Dispositivo / Entorno**: Windows (`DESKTOP-RG5KOC8` / `Benjamin Sepulveda`) — Repositorio: **`Cerebro_IA`** (`main`)
- **Estado general**: Se migraron todos los archivos de conocimiento, memoria compartida (`.agents/memory/`), skills (`.agents/skills/`), niveles (`levels/`) y código (`src/`) desde `migracion-conocimiento.zip` a la raíz del repositorio `Cerebro_IA`, fusionando `.gitignore` y `README.md` para conservar intacta la bóveda de Obsidian junto con el entorno multi-dispositivo de Antigravity. Se mantiene listo el flujo de trabajo **`portal-me-true-ingame-v2`** (`.agents/skills/portal-me-true-ingame-v2/SKILL.md`) en 4 módulos.

### Resumen de la Última Sesión
- Se identificaron las 4 causas raíz por las que `primer-intento` se desvió de la intención original y las 5 brechas frente a los videos reales de internet (`libsm64` / mods de *Portal Gun* en otros motores).
- Se creó la nueva Skill [`portal-me-true-ingame-v2`](../skills/portal-me-true-ingame-v2/SKILL.md) estructurada en **4 Módulos Técnicos**:
  1. **Módulo 1**: Extracción de la geometría exacta de la azotea activa de `Edge_p` (`(19500.0, -3009.5, 8517.2)`) hacia un `.bsp` 1:1 en *Portal*.
  2. **Módulo 2**: Inyección directa de inputs en memoria (*Memory Input Injection*) desde *Mirror's Edge* hacia *Portal* sin cambiar de ventana, con audio unificado (`snd_mute_losefocus 0`).
  3. **Módulo 3**: Lectura en tiempo real de las entidades reales `C_Prop_Portal` (Portal Azul y Portal Naranja) desde `cl_entitylist` en `client.dll` de *Portal*.
  4. **Módulo 4**: Hook nativo `d3d9.dll` (`IDirect3DDevice9::EndScene`) en `Mirror's Edge/Binaries/` con ocultamiento de Faith (`DrawScale = 0`), portales 3D con profundidad (`Z-Buffer`) + vista interior + Chell, y Viewmodel en 1ª persona de la *Portal Gun* + retícula.

### Tareas en Curso (In Progress en `segundo-intento`)
- [x] Post-Mortem de `primer-intento` y diseño de la arquitectura `portal-me-true-ingame-v2` (`ADR-005`).
- [ ] **Módulo 1**: Extraer las superficies reales de la azotea de `Edge_p` y compilar el `.bsp` 1:1.
- [ ] **Módulo 2**: Implementar inyección de inputs en memoria (ME $\rightarrow$ Portal) para jugar 100% dentro de *Mirror's Edge*.
- [ ] **Módulo 3**: Localizar y leer las entidades `C_Prop_Portal` (Azul y Naranja) en `client.dll` de *Portal*.
- [ ] **Módulo 4**: Compilar e inyectar `d3d9.dll` en `Mirror's Edge/Binaries/` para renderizar Portales 3D, Chell y Viewmodel de la *Portal Gun* en `EndScene`.

### Próximos Pasos Sugeridos (Next Steps)
1. Implementar **Módulo 1 y Módulo 3**: Extraer las coordenadas exactas de `C_Prop_Portal` en `client.dll` y alinear la geometría del `.bsp` con la azotea real de `Edge_p`.
2. Implementar **Módulo 2 y Módulo 4**: Integrar el control en una sola ventana y compilar el `d3d9.dll` proxy para *Mirror's Edge*.
