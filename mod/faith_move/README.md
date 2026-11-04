# mod/faith_move

- **Dueño:** Peña · **Tareas:** P1–P5 (ver `PLAN_EQUIPO.md`). **P1 y P2 los escribió Sebas** (2026-11-04) porque Peña no podía; Peña sigue con P3.
- Librería de movimiento de Faith en C++ puro (sin Source ni UE3). Implementa `ar::IFaithMovement` y define `ar::CreateFaithMovement` (`mod/include/ar/faith_movement.h`). Se prueba con `ar::testing::BoxWorld`.
- Solo el dueño edita esta carpeta. Los contratos de `mod/include/ar/` no se tocan: pide cambios a Sebas.

## Archivos
- `include/faith_move/faith_params.h`: todos los valores de ajuste en unidades de Source. Los marcados `placeholder` no vienen de Mirror's Edge; P5 los cambia con trazas reales.
- `src/faith_movement.cpp`: la implementación. El estado privado (`Internal`: altura máxima de la caída, coyote time, bloqueo por caída dura…) se guarda en `FaithState::internal`, para que la predicción de Source lo pueda rebobinar.
- `tests/test_faith_move.cpp`: 104 comprobaciones. Compila con `mod\build.bat` (prueba `test_faith_move`).

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

## Pendiente
- **P3 (Peña):** wallrun, wallclimb, borde, slide (usará `ducked`), coil, Skill Roll, giro de 180°.
- **P4:** continuar el wallrun al cruzar un portal.
- **P5:** ajustar los placeholders con trazas reales.
