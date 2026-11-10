# Guion del video final (B5, hito H4)

- **Qué es:** un video de demostración, jugando `testchmb_a_00` de principio a fin con el mod activado. Sirve como evidencia del hito H4 y como video para enseñar el proyecto.
- **No es** una grabación de hlmv ni de la telemetría: se graba el juego.
- **Duración:** 3 a 5 minutos, sin cortes en el recorrido. Se pueden añadir al final 30 s de "momentos" (portal con inercia, wallrun).

## Antes de grabar

1. Cierra Mirror's Edge y cualquier otro juego: si están abiertos, Portal va peor y la prueba automática falla.
2. Comprueba que todo pasa: `powershell -ExecutionPolicy Bypass -File qa\scripts\probar_todo.ps1` → H2 9/9 y H3 16/16.
3. Portal en **pantalla completa a 1920×1080**, con el HUD y los subtítulos por defecto. Cierra la consola antes de empezar.
4. En la consola: `ar_enable 1` y `ar_camera 1`. Con `ar_status` comprueba que dice `brazos (G4): activo` y `cuerpo (G4): activo`.
5. Grabador:
   - **Xbox Game Bar:** `Win + Alt + R` para empezar y parar. Graba solo la ventana del juego.
   - **OBS:** "Captura de juego", 60 fps, con el audio del juego.
6. Empieza con `map testchmb_a_00` (partida nueva).

## Recorrido (en el orden de la checklist `CHK-01…14`)

| # | Dónde | Qué mostrar | Qué se tiene que ver |
|---|---|---|---|
| 1 | Despertar (`CHK-01`) | Mirar alrededor y luego **mirar hacia abajo** | Las piernas y zapatillas de Faith; al mirar abajo, los brazos se ocultan |
| 2 | Bóveda | Caminar y luego **correr en línea recta** | El sprint tarda en llenarse (inercia de Mirror's Edge, P5); el FOV se abre y la cabeza se balancea; las manos asoman al correr |
| 3 | Bóveda | **Salto** corriendo | El salto de Faith (237.8 su/s) y el aterrizaje |
| 4 | Portal naranja → azul (`CHK-04`) | Cruzarlo **corriendo** | Se conserva la velocidad al salir (P4) |
| 5 | Sala 1, pared lateral | **Wallrun**: corre junto a la pared y salta | La vista se inclina ~15°; los brazos pasan a la pose de wallrun |
| 6 | Sala 1, de frente a una pared | **Wallclimb** | Sube unos 80 su y vuelve a caer |
| 7 | Caja y botón #1 (`CHK-05`, `CHK-06`) | Coger la caja, ponerla en el botón y salir | El juego sigue funcionando con el movimiento de Faith |
| 8 | Plataforma del botón | **Vault** sobre el borde biselado | Pasa por encima sin quedarse enganchada |
| 9 | Pasillo | **Slide**: corre y agáchate | Se desliza ~1 s y se levanta |
| 10 | Altura (pasarela o repisa) | Dejarse caer y pulsar **agacharse justo antes de tocar el suelo** | Skill Roll (rueda la cámara) en vez de caída dura |
| 11 | Repisa junto al portal cerrado | **Agarrar el borde y subir** | Cuelga y sube (unos 1.5 s, como en Mirror's Edge) |
| 12 | Ascensor intermedio (`CHK-08`) | Subir | Funciona normal |
| 13 | Cámara 01 (`CHK-09`…`CHK-11`) | Ciclo de portales de 5 s, caja y botón #2 | Cruces de portal con inercia |
| 14 | Recargar en cualquier sitio | **Giro de 180°** | La vista gira sola |
| 15 | Salida y ascensor final (`CHK-12`…`CHK-14`) | Entrar en el ascensor | Cambia a `testchmb_a_01` sin cierres |

Si algo sale mal (Faith se atasca, la vista tiembla, desaparecen los brazos), **no cortes**. Sigue jugando y apunta el minuto: sirve para corregirlo.

## Después de grabar

1. El video **no se sube a Git** (el proyecto no versiona videos). Súbelo a YouTube (oculto) o a Google Drive.
2. Pásale la ruta del `.mp4` a Claude para que saque fotogramas de cada punto del recorrido y lo revise contra esta tabla.
3. Pon el enlace en `qa/resultados/H4.md` y marca B5 como hecho en `PLAN_EQUIPO.md`.
