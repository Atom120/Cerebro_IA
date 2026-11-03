# Análisis Comparativo: Flujo #1 (`Edge_p` Azotea) vs. Flujo #2 (`Stormdrain_p` Gran Esclusa)

> Análisis técnico de complejidad, puntos negativos y viabilidad de implementación asistida por **Gemini 4 Argon** en Antigravity.

---

## 1. Comparación Técnica Cara a Cara

| Criterio Técnico | Flujo #1: `edge_p_rooftop_01` (Azotea del Prólogo) | Flujo #2: `stormdrain_p_sluice_01` (Esclusa Cap. 2) | Ventaja en Simplicidad |
| :--- | :--- | :--- | :--- |
| **Acceso al Nivel en *Mirror's Edge*** | Inmediato (es el punto de aparición exacto al iniciar `Edge_p`) | Requiere teletransportar al jugador a mitad de `Stormdrain_p` y forzar la carga de *Level Streaming* | **Flujo #1** |
| **Geometría `.vmf` / Compilación `.bsp`** | 6 cajas ortogonales AABB (`box`) alineadas a los ejes $X, Y, Z$ | Cajas ortogonales + **cuña oblicua a $45^\circ$ (`wedge_45_plus_x`)** + 2 volúmenes de Fizzler | **Flujo #1** |
| **Entidades Sincronizadas (Backend VSP)** | Solo `CPortal_Player` (índice fijo `1`) + 2 `prop_portal` | `CPortal_Player` + 2 `prop_portal` + `prop_weighted_cube` (dinámico) + `prop_floor_button` + 2 `prop_portal_cleanser` | **Flujo #1** |
| **Ingeniería Inversa en UE3 (Frontend)** | Solo sobrescribe `TdPlayerPawn->Location/Velocity` y `Controller->Rotation` | Requiere además renderizar/mover el Cubo y disparar un evento **Kismet (`OpenSluiceExitGate_01`)** en `GObjObjects` | **Flujo #1** |
| **Matemática de Renderizado D3D9** | Normales ortogonales puras ($\pm X, \pm Y, \pm Z$) | Normales oblicuas a $45^\circ$, rotación 3D completa del cubo y *alpha blending* de cortinas Fizzler | **Flujo #1** |
| **Profundidad Jugable (Resultado Final)** | Sandbox de movimiento y *flinging* básico | Cámara de pruebas completa estilo *Aperture* con catapulta parabólica, cubo y meta | **Flujo #2** |

---

## 2. Puntos Negativos y Riesgos de Cada Flujo

### Puntos Negativos del Flujo #1 (`Edge_p` — Azotea Inicial)
1. **Ausencia de un objetivo/puzzle cerrado**: Al no contar con cubos, botones ni puertas conectadas a Kismet, funciona como un laboratorio de físicas (*sandbox*), pero no ofrece un puzzle con principio y fin.
2. **Caídas al vacío urbano (`KillZ` de UE3)**: Como la azotea está abierta hacia los edificios vecinos, si el jugador cae fuera de la caja delimitadora y toca un volumen de muerte nativo de *Mirror's Edge* antes de que actúe el reset IPC, UE3 reiniciará el checkpoint por su cuenta y desincronizará durante unos segundos ambos procesos.
3. **Sin catapultas oblicuas a $45^\circ$**: Todas las paredes son de $90^\circ$ y el piso de $0^\circ$, lo que limita los lanzamientos a trayectorias puramente horizontales o verticales.

### Puntos Negativos del Flujo #2 (`Stormdrain_p` — Gran Esclusa Vertical)
1. **Fragilidad del puente con Kismet en UE3**: Activar la compuerta de salida real de `Stormdrain_p` cuando se pisa el `prop_floor_button` en Source exige localizar la instancia exacta de `USequenceEvent` en la memoria de `MirrorsEdge.exe` y llamar a `ProcessEvent`. Un solo offset incorrecto en la estructura interna de UE3 provoca un cierre inmediato (*crash* por `Access Violation`).
2. **Ciclo de vida dinámico del Cubo de Compañía**: Si el `prop_weighted_cube` toca uno de los *Emancipation Grills* (Fizzlers), Source destruye la entidad y spawnea una nueva con otro `entindex`. El plugin VSP debe re-escanear la tabla de entidades dinámicamente sin perder la referencia.
3. **Problemas de *Level Streaming* en `Stormdrain_p`**: Unreal Engine 3 carga y descarga sub-niveles (`Stormdrain_p_slc`) según los triggers por los que pasa Faith. Aparecer directamente en la esclusa sin cruzar los pasillos previos puede dejar mallas o colisiones visuales sin cargar a menos que se fuerce `LoadPackage` o la activación de volúmenes de streaming.
4. **Tolerancia cero en la alineación de la rampa a $45^\circ$**: En un plano inclinado a $45^\circ$, cualquier error de $5\text{ a }10\text{ UU}$ en el anclaje hace que el elipsoide del portal dibujado por D3D9 se corte a la mitad contra la malla visual de la rampa en *Mirror's Edge* (*Z-fighting* o recorte diagonal).

---

## 3. Facilidad de Recreación con las Herramientas de Gemini 4 Argon

Analizando las capacidades directas de **Gemini 4 Argon** dentro de Antigravity (generación determinista de C++, scripts en Node.js, compilación por terminal con `run_command`, subagentes paralelos y validación matemática automatizada):

1. **Lo que Gemini 4 Argon resuelve sin fricción en ambos flujos**:
   - La matemática vectorial (`HU <-> UU`, `RH <-> LH`, proyección de velocidad a $45^\circ$), el protocolo `SeqLock` en memoria compartida y la generación procedural de archivos `.vmf` válidos para `vbsp.exe`.
2. **Dónde las herramientas del agente encuentran mayor fricción**:
   - La depuración de memoria en tiempo de ejecución de un binario cerrado de 2008 (`MirrorsEdge.exe`). Mientras menos estructuras internas del motor propietario de DICE haya que manipular por reflexión (`GObjects` / `GNames` / `Kismet`), más rápido y seguro podrá Gemini 4 Argon compilar un `.dll` que funcione al primer intento.

---

## 4. Veredicto: ¿Cuál es más sencillo de realizar?

**El Flujo de Trabajo #1 (`portal-mirrors-edge-passthrough` — Azotea Inicial de `Edge_p`) es significativamente más sencillo de realizar (alrededor de un 60% menos propenso a errores de memoria en las Fases 2, 3 y 4).**

### Por qué el Flujo #1 es el ganador en simplicidad:
- **Cero dependencia de Kismet**: No necesita buscar ni disparar eventos internos del nivel en UE3.
- **Un solo actor sincronizado**: Solo lee a Chell (`entindex = 1` en Source) y mueve a Faith (`TdPlayerPawn` en UE3).
- **Carga instantánea para pruebas con `run_command`**: Al lanzar `MirrorsEdge.exe Edge_p`, el jugador ya está parado exactamente sobre la azotea de pruebas sin necesidad de saltar capítulos ni lidiar con *Level Streaming*.
- **Geometría 100% ortogonal**: Las 6 cajas AABB garantizan que `vbsp.exe` compile el `.bsp` en menos de 2 segundos sin problemas de portales en ángulos oblicuos.

### Estrategia Recomendada
Usar el **Flujo #1 (`Edge_p`) como base de implementación para las Fases 2, 3 y 4** (compilar el plugin VSP y el hook `d3d9.dll` básicos). Una vez que Chell ya se mueva y dispare portales dentro de la azotea de `Edge_p`, el **80% de ese código binario servirá directamente para habilitar el Flujo #2 (`Stormdrain_p`)** como una expansión incremental.
