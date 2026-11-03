---
tipo: proyecto
status: activo
inicio: 2026-10-28
tags: [proyecto, apertureRunner, mod, cpp, python]
---
# ApertureRunner

## Qué es
- Mod que une Mirror's Edge (UE3) y Portal 2 (Source) por memoria compartida `Local\ApertureRunner_SHM_v1`, más un director de IA que comenta la partida.
- Equipo de 6 personas (líder con Claude Code + 5 con Antigravity/Gemini). Código en `C:\Users\Cleve\Documents\GitHub\aperture-runner-mashup` (repo git local, sin remoto todavía).
- Solo "Modalidad 1": necesita los dos juegos originales. Claude no hace la "Modalidad 2" (recrear los juegos).

## Alcance (decidido 2026-10-28)
- Solo una cámara: el mapa original de Portal 1 `testchmb_a_01` (cámaras 02 y 03), dentro del juego real. No se recrea el nivel en otro motor.
- Sin GLaDOS hablando: el director de IA queda fuera.
- Sin geles, torretas ni puentes de luz (no salen en esa cámara).

## Plan de equipo
- Reparto, dependencias y agentes: [[PLAN_EQUIPO]] (ADR-006 en `.agents/memory/DECISIONS.md`).
- Equipo: Sebas (líder), Zarek, Peña, Gonzalo, Antonio, Benja.
- Código nuevo en `mod/` dentro de este repo (núcleo copiado de `aperture-runner-mashup`, que queda solo como copia local antigua). Contratos v1 en `mod/include/ar/`; compilar con `mod\build.bat`. Trabajo anterior de Benja (Portal dentro de ME) archivado como referencia.

## Investigación 2026-11-03
- El primer mapa real de Portal 1 es `testchmb_a_00` (cámaras 00-01, sin pistola de portales); falta confirmar si sustituye a `testchmb_a_01`.
- Recomendado: ruta B "Faith dentro de Source" (una DLL x86 en Portal + modelo de Faith generado desde la copia propia de Mirror's Edge). La ruta A (dos juegos a la vez) queda como fase posterior.
- Portal instalado: build 19017868, 32 bits. Mirror's Edge aún no.
- Reparto y agentes por persona: [[20_Chats/2026-11-03 - ApertureRunner flujo profesional y reparto para 6]] · informe en Notion: https://app.notion.com/p/40d1d50b074681eda477ce985f070297

## Estado actual
- Hecho y probado: el contrato binario `protocol.h`, `SharedArena` (Win32), `math.h` (conversiones, transformación de portales, wallrun, roll, Skill Roll, geles), `HostBridge` y `GuestBridge`, `ar_tool`, y el director Python en el puerto 5055 con HUD.
- Falta: el hook de Mirror's Edge (`IGuestPawnAdapter`), el plugin de Source y la calibración entre mundos.

## Decisiones importantes
- Los offsets de UE3 no se inventan; esperan al volcado de SDK.
- La distribución de memoria es idéntica en x86 y x64, y Python la comprueba contra `ar_tool layout`.
- El director hace llamadas sin historial; las acciones del modelo pasan por lista blanca y límite de frecuencia.

## Chats relacionados
- [[20_Chats/2026-10-28 - ApertureRunner núcleo y director IA]]
- [[20_Chats/2026-11-03 - ApertureRunner flujo profesional y reparto para 6]]

## Próximos pasos
- [x] Instalar Portal (Steam): build 19017868, 32 bits.
- [ ] Instalar Mirror's Edge.
- [x] Comprobar arquitectura y versión del motor del Portal 1 instalado (x86, Source 2013 SP).
- [ ] Decidir ruta (B recomendada) y mapa (`testchmb_a_00` vs `testchmb_a_01`).
- [ ] Conseguir el volcado de SDK de `MirrorsEdge.exe` e implementar `IGuestPawnAdapter`.
- [ ] Plugin de servidor para Portal 1: portales y geometría de `testchmb_a_01`.
