---
tipo: proyecto
status: activo
inicio: 2026-10-28
tags: [proyecto, apertureRunner, mod, cpp, python]
---
# ApertureRunner

## Qué es
- Mod que une Mirror's Edge (UE3) y Portal 2 (Source) por memoria compartida `Local\ApertureRunner_SHM_v1`, más un director de IA que comenta la partida.
- Equipo de 5 personas. Código en `C:\Users\Cleve\Documents\GitHub\aperture-runner-mashup` (repo git local, sin remoto todavía).
- Solo "Modalidad 1": necesita los dos juegos originales. Claude no hace la "Modalidad 2" (recrear los juegos).

## Alcance (decidido 2026-10-28)
- Solo una cámara: el mapa original de Portal 1 `testchmb_a_01` (cámaras 02 y 03), dentro del juego real. No se recrea el nivel en otro motor.
- Sin GLaDOS hablando: el director de IA queda fuera.
- Sin geles, torretas ni puentes de luz (no salen en esa cámara).

## Estado actual
- Hecho y probado: el contrato binario `protocol.h`, `SharedArena` (Win32), `math.h` (conversiones, transformación de portales, wallrun, roll, Skill Roll, geles), `HostBridge` y `GuestBridge`, `ar_tool`, y el director Python en el puerto 5055 con HUD.
- Falta: el hook de Mirror's Edge (`IGuestPawnAdapter`), el plugin de Source y la calibración entre mundos.

## Decisiones importantes
- Los offsets de UE3 no se inventan; esperan al volcado de SDK.
- La distribución de memoria es idéntica en x86 y x64, y Python la comprueba contra `ar_tool layout`.
- El director hace llamadas sin historial; las acciones del modelo pasan por lista blanca y límite de frecuencia.

## Chats relacionados
- [[20_Chats/2026-10-28 - ApertureRunner núcleo y director IA]]

## Próximos pasos
- [ ] Instalar Portal (Steam) y Mirror's Edge en el equipo (el 2026-10-28 no estaba ninguno).
- [ ] Comprobar arquitectura y versión del motor del Portal 1 instalado.
- [ ] Conseguir el volcado de SDK de `MirrorsEdge.exe` e implementar `IGuestPawnAdapter`.
- [ ] Plugin de servidor para Portal 1: portales y geometría de `testchmb_a_01`.
