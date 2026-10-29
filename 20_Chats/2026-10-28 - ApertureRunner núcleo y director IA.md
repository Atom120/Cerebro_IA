---
tipo: chat
fecha: 2026-10-28
herramienta: Claude
proyecto: ApertureRunner
tags: [chat, apertureRunner, mod, cpp, python, ia]
---
# ApertureRunner: núcleo y director IA

## Objetivo
- Conectar este repo de Cerebro_IA.
- Implementar las partes de ApertureRunner (mod Mirror's Edge × Portal 2) que no dependen de offsets de los juegos.

## Resumen
- Se descargó `main` en el clon local `C:\Users\Cleve\Documents\GitHub\Cerebro_IA`, que estaba vacío.
- Claude no construye la "Modalidad 2" (recrear ambos juegos en un ejecutable). Sí ayuda con la "Modalidad 1": un mod que requiere los dos juegos originales.
- Se creó el proyecto `C:\Users\Cleve\Documents\GitHub\aperture-runner-mashup` con el núcleo C++ y el director en Python. Ver [[10_Proyectos/ApertureRunner/README|ApertureRunner]].

## Decisiones / resultados
- El hook de Mirror's Edge queda detrás de la interfaz `IGuestPawnAdapter`. Implementarla requiere el volcado de SDK de la copia del usuario; inventar offsets daría código que compila pero no funciona.
- Pruebas: 174 comprobaciones C++ en x64 y x86 (misma distribución de memoria) y 13 pruebas Python, incluida una ida y vuelta con un proceso C++ real.
- Director: `claude-opus-5-5`, esfuerzo `low`, `fallbacks: "default"`, llamadas sin historial (evita problemas de historial editado). Trampas con lista blanca y límite de frecuencia.
- Correcciones a la especificación: el portal mide 32×54 su de media apertura; #E60012 lineal es (0.791, 0, 0.006); no se fijan 82 huesos ni 42 estados.

- No se probó el director con la API real: el equipo no tiene `ANTHROPIC_API_KEY` ni perfil de `ant`. La clave solo sirve para que el director genere frases; nada más la necesita.
- El sub-prompt de Persona 1 ya estaba cubierto por el núcleo C++. No se hacen los `.cs` ni `FireBluePortal`/`FireOrangePortal`: en el mod los portales los coloca el propio juego, y reimplementarlos sería recrear Portal.
- **Cambio de alcance:** sin GLaDOS hablando y sin simular todo el juego. Solo una cámara: el mapa original de Portal 1 `testchmb_a_01` (cámaras 02 y 03), dentro del juego real (opción A). No se recrea el nivel en Unity.
- Portal y Mirror's Edge no están instalados en el equipo, así que el plugin y el hook quedan bloqueados.

## Código, comandos o archivos clave
- `core\build.bat`: compila y prueba con MSVC.
- `director\.venv\Scripts\python -m unittest test_director -v`
- `director\.venv\Scripts\python glados_parkour_director.py --dry-run`, con HUD en http://127.0.0.1:5055/

## Pendientes
- [ ] Instalar Portal (Steam) y Mirror's Edge.
- [ ] Comprobar si el Portal 1 instalado es de 32 o 64 bits y qué rama del motor usa.
- [ ] Plugin de servidor para Portal 1: leer los portales y la geometría de `testchmb_a_01`.
- [ ] Hook UE3: implementar `IGuestPawnAdapter` con el SDK de `MirrorsEdge.exe`.
- [ ] Decidir el punto de calibración entre ambos mundos.
- [ ] No ejecutar `openclaude skills install rehan-remade/universal-modder` sin revisarlo.

## Relacionado
- [[Inicio]]
- [[30_Temas/Portales no euclidianos|Portales no euclidianos]]
