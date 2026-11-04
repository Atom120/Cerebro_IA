# mod/faith_move

- **Dueño:** Peña · **Tareas:** P1–P5 (ver `PLAN_EQUIPO.md`). **P1, P2 y P3 los escribió Sebas** (2026-11-04) porque Peña no podía; Peña sigue con P4/P5.
- Librería de movimiento de Faith en C++ puro (sin Source ni UE3). Implementa `ar::IFaithMovement` y define `ar::CreateFaithMovement` (`mod/include/ar/faith_movement.h`). Se prueba con `ar::testing::BoxWorld`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.

## Archivos
- `include/faith_move/faith_params.h`: todos los valores de ajuste en unidades de Source. Los marcados `placeholder` no vienen de Mirror's Edge; P5 los cambia con trazas reales.
- `src/faith_movement.cpp`: la implementación. El estado privado (`Internal`: altura máxima de la caída, coyote time, bloqueo por caída dura…) se guarda en `FaithState::internal`, para que la predicción de Source lo pueda rebobinar.
- `tests/test_faith_move.cpp`: 212 comprobaciones. Compila con `mod\build.bat` (prueba `test_faith_move`).

## Qué hace P2 (suelo)
- **Correr:** acelera a 2419 su/s² hasta 157.5 su/s (inicio del sprint).
- **Sprint con inercia:** solo corriendo hacia delante (sin `Walk` y con la dirección a menos de ~45° de la vista). Sube despacio de 157.5 a 283.5 su/s en `kSprintRampSec` (1.5 s, placeholder).
- **Giros:** la velocidad lateral se frena, así que un giro suave conserva el sprint y uno brusco lo pierde.
- **Caminar** (`Walk`): 59 su/s. Moverse de lado: máximo 157.5. Sin entrada frena hasta parar.
- **Salto:** 248 su/s hacia arriba más 39.4 su/s en la dirección de la carrera. Altura ≈ 47.6 su. Solo salta al pulsar, no al mantener. Coyote time de 0.1 s al bajar de un borde.
- **Aire:** gravedad de 646 su/s². El control en el aire solo sirve para orientar y no da más velocidad.
- **Aterrizajes:**
  - Desde 208.7 su o más (`HardLandingHeight`): caída dura (`LandHard`, evento `HardLanding`). La velocidad baja a 59 y no hay control durante 0.5 s.
  - Menos que eso: `LandSoft`.
  - Más de 393.7 su: la animación de caída es `FallUncontrolled`.
- **Colisión:** movimiento estilo `TryPlayerMove` de Source, que se desliza por paredes y esquinas. Sube escalones de hasta `step_height` (18), baja escalones sin despegarse del suelo y la caja es sólida.
- **Portales:** si el host avisa de un teletransporte (`FaithInput::portal`), gira la velocidad. El origen lo tiene que refrescar el host desde el motor. El resto es P4.

## Qué hace P3 (parkour)
Es una máquina de estados con `Internal::mode`: `Ground`, `Air`, `Slide`, `WallRun`, `WallClimb`, `WallClimbTurn`, `LedgeHang` y `LedgeClimb`. Las paredes solo valen si son `World` o `StaticProp`; la caja (`Physics`) nunca.

| Movimiento | Cómo se activa | Valores |
|---|---|---|
| **Vault** | `Up` corriendo hacia un obstáculo de 18 a 48 su | 0.35 s y conserva la velocidad |
| **Wallclimb** | `Up` mirando de frente (< ~37°) a una pared alta, a menos de 20 su; en el aire, manteniendo `Up` | sube unos 87 su. Uno por salto |
| **Giro en el wallclimb** | `Turn180` durante el wallclimb → `WallClimbTurn` (queda colgada hasta 1 s) → `Up` = `WallClimbJump` hacia fuera | evento `Turn180` |
| **Wallrun izq./der.** | `Up` corriendo junto a una pared (o mantenerlo en el aire), a ≥ 78.7 su/s, pared ≥ 75.6 de alto, sin ir de frente contra ella | 1.2 s como máximo. Con "adelante" pulsado acelera 322.8; sin él frena 196.9. Gravedad ×0.28. Cámara inclinada ±15° (`view_roll`). `Up` = salto de pared. No repite la misma pared hasta tocar el suelo |
| **Agarrar borde** | automático en el aire al acercarse a un borde a 40-96 su sobre los pies, cerca del punto más alto del salto o durante un wallclimb | `Up` = subir (0.6 s, primero arriba y luego encima). `Down` = soltarse (0.4 s sin volver a agarrar) |
| **Slide** | `Down` corriendo (≥ 157.5) | casco agachado (pasa por huecos de 36+), frena a 120 su/s² hasta 98.4. Soltar `Down` lo corta. Solo se levanta si hay sitio |
| **Agacharse** | `Down` mantenido | 63 su/s, `Crouch`/`CrouchWalk` |
| **Coil** | `Down` en el aire | encoge las piernas (los pies suben 36, la cabeza no se mueve). Eso **no** cuenta como altura de caída |
| **Skill Roll** | `Down` pulsado ≤ 0.35 s antes de aterrizar de ≥ 78.7 su | evita la caída dura y conserva la velocidad (0.6 s) |
| **Giro de 180°** | `Turn180` en el suelo | invierte la velocidad (a la mitad y como mucho 157.5). Evento `Turn180`: **el host gira la vista** |

**Contrato v2:** se añadió `kFaithEventTurn180`. `FaithState` no cambió.

## Pendiente
- **P4:** continuar el wallrun al cruzar un portal.
- **P5:** ajustar los placeholders con trazas reales.
