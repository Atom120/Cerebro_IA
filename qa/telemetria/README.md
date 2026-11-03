# Telemetría de Referencia de Mirror's Edge (`qa/telemetria/` — B2)

- **Dueño**: Benja (`B2`) · **Consumidor principal**: Peña (`P5` — `tuning-analyst`)
- **Código del grabador (C++20 / Win32 x86)**: [`mod/tools/me_telemetry/`](../../mod/tools/me_telemetry/)
- **Script de captura/verificación**: [`mod/tools/me_telemetry/record_me_csv.py`](../../mod/tools/me_telemetry/record_me_csv.py)
- **Modo de acceso a memoria**: Estrictamente **SOLO LECTURA** (`PROCESS_VM_READ | PROCESS_QUERY_INFORMATION` + `ReadProcessMemory`). Nunca escribe en `MirrorsEdge.exe`.

---

## 1. Offsets Verificados en Mirror's Edge GOG v1.01

- **Binario**: `MirrorsEdge.exe` (DRM-Free GOG v1.01)
- **PE Timestamp**: `0x4965FFF1`
- **ImageBase fijo**: `0x00400000` (`ASLR = false`)
- **Cadena de punteros al jugador**:
  $$\text{GEngine}\ (\texttt{0x01FF8B20}) \xrightarrow{+\texttt{0x02BC}} \text{GamePlayers.Data}[0] \xrightarrow{+\texttt{0x0040}} \text{ATdPlayerController} \xrightarrow{+\texttt{0x01C0}} \text{ATdPlayerPawn}$$

| Campo en `ATdPawn` / `AActor` | Offset | Tipo | Verificación en binario / `TdGame.u` |
|---|---|---|---|
| `Physics` | `+0x006B` | `uint8_t` (`EPhysics`) | `1 = PHYS_Walking`, `2 = PHYS_Falling` |
| `Location` | `+0x00E8` | `float[3]` (`x, y, z` en `uu`) | Centro de la cápsula de Faith (`HalfHeight = 90 uu`, `Radius = 30 uu`) |
| `Rotation` | `+0x00F4` | `int32_t[3]` (`URU`) | `65536 = 360°` |
| `Velocity` | `+0x0100` | `float[3]` (`vx, vy, vz` en `uu/s`) | Velocidad en espacio mundo UE3 (mano izquierda, `Z` arriba) |
| `VelocityMagnitude2D` | `+0x04BC` | `float` (`uu/s`) | Escrito en `ATdPawn::UpdateVelocityVariables` (`0x012B0B20`) |
| `MovementState` | `+0x04FE` | `uint8_t` (`EMovement`, `0..93`) | Leído en `ATdPawn::GetMove` (`0x012B1750`: `mov al, [ecx+0x4FE]; ret`) y `ATdPawn::IsInMove` (`0x012B1730`) |
| `WalkingState` | `+0x0503` | `uint8_t` (`WAS_*`, `0..6`) | Escrito en `ATdPawn::UpdateWalkingState` (`0x012B1660`: `mov [ecx+0x503], ...`) |

---

## 2. Formato de los Archivos CSV

Todos los archivos CSV en esta carpeta tienen la cabecera:

```csv
t,x,y,z,vx,vy,vz,movement_state,walking_state
```

- `t`: tiempo desde el inicio del clip en segundos (`double`, muestreado a `120 Hz`, $\Delta t \approx 0.008333\text{ s}$).
- `x, y, z`: posición del centro de la cápsula de Faith en **unidades de Unreal (`uu`)**.
- `vx, vy, vz`: velocidad de Faith en **`uu/s`**.
- **Conversión a unidades de Source (`su`) para `tuning-analyst` (`P5`)**:
  $$\text{source\_x} = \frac{x}{2.54}, \quad \text{source\_y} = -\frac{y}{2.54}, \quad \text{source\_z} = \frac{z}{2.54}$$
  (Implementada en `ar::telemetry::ConvertSampleToSource` usando `aperture_runner::Ue3ToSourceVector`).

---

## 3. Valores de `movement_state` (`EMovement` en `TdGame.u`) y `walking_state` (`WAS_*`)

### `movement_state` (`ATdPawn + 0x04FE`)

| Valor | Nombre en `TdGame.u` | Mapeo a `ar::FaithAnimState` |
|---|---|---|
| `0` | `MOVE_None` | `Idle` |
| `1` | `MOVE_Walking` | `Idle` / `Walk` / `Run` / `Sprint` (según `walking_state`) |
| `2` | `MOVE_Falling` | `Fall` |
| `3` | `MOVE_Grabbing` | `LedgeHang` |
| `4` | `MOVE_WallRunningRight` | `WallRunRight` |
| `5` | `MOVE_WallRunningLeft` | `WallRunLeft` |
| `6` | `MOVE_WallClimbing` | `WallClimb` |
| `7` | `MOVE_SpringBoarding` | `Vault` |
| `8` | `MOVE_SpeedVaulting` | `Vault` |
| `9` | `MOVE_VaultOver` | `Vault` |
| `10` | `MOVE_GrabPullUp` | `LedgeClimbUp` |
| `11` | `MOVE_Jump` | `Jump` |
| `12` | `MOVE_WallRunJump` | `WallRunJump` |
| `13` | `MOVE_GrabJump` | `LedgeClimbUp` |
| `14` | `MOVE_IntoGrab` | `LedgeHang` |
| `15` | `MOVE_Crouch` | `Crouch` / `CrouchWalk` |
| `16` | `MOVE_Slide` | `Slide` |
| `20` | `MOVE_Landing` (caída dura) | `LandHard` |
| `24` | `MOVE_180Turn` | `Turn180` |
| `25` | `MOVE_180TurnInAir` | `Turn180` |
| `49` | `MOVE_WallClimbDodgeJump` | `WallClimbTurn` |
| `50` | `MOVE_WallClimb180TurnJump` | `WallClimbJump` |
| `61` | `MOVE_Coil` | `Coil` |
| `72` | `MOVE_FallingUncontrolled` | `FallUncontrolled` |
| `78` | `MOVE_SoftLanding` | `LandSoft` |
| `91` | `MOVE_SkillRoll` | `SkillRoll` |
| `93` | `MOVE_Cutscene` | `Idle` |

### `walking_state` (`ATdPawn + 0x0503`)

| Valor | Nombre en `TdGame.u` | Significado |
|---|---|---|
| `0` | `WAS_Idle` | Quieta en el suelo |
| `1` | `WAS_Sneak` | Paso lento |
| `2` | `WAS_Walk` | Caminar |
| `3` | `WAS_Jog` | Trote ligero |
| `4` | `WAS_Run` | Correr (`< 400 uu/s` / `< 157.5 su/s`) |
| `5` | `WAS_Sprint` | Sprint con inercia (`400 → 720 uu/s` / `157.5 → 283.5 su/s`) |
| `6` | `WAS_None` | En el aire o en estado de parkour especial |

---

## 4. Catálogo de Trazas CSV (`qa/telemetria/`)

| Archivo CSV | Maniobra | Duración (`120 Hz`) | Estados `EMovement` incluidos |
|---|---|---|---|
| [`sprint_start.csv`](./sprint_start.csv) | Arrancar de `Idle` a `Sprint` (`0 → 720 uu/s` / `283.5 su/s`) | `3.00 s` (361 muestras) | `1` (`WAS_Idle=0` → `WAS_Walk=2` → `WAS_Jog=3` → `WAS_Run=4` → `WAS_Sprint=5`) |
| [`jump.csv`](./jump.csv) | Salto en sprint (`BaseJumpZ = 630 uu/s`, `JumpAddXY = 100 uu/s`, $g = 1641\text{ uu/s}^2$) | `1.50 s` (181 muestras) | `1` (`MOVE_Walking`), `11` (`MOVE_Jump`), `2` (`MOVE_Falling`), `78` (`MOVE_SoftLanding`) |
| [`wallrun_left.csv`](./wallrun_left.csv) | Wallrun en pared izquierda + salto lateral (`MOVE_WallRunJump`) | `2.00 s` (241 muestras) | `1`, `5` (`MOVE_WallRunningLeft`), `12` (`MOVE_WallRunJump`), `2` |
| [`wallrun_right.csv`](./wallrun_right.csv) | Wallrun en pared derecha + salto lateral (`MOVE_WallRunJump`) | `2.00 s` (241 muestras) | `1`, `4` (`MOVE_WallRunningRight`), `12` (`MOVE_WallRunJump`), `2` |
| [`wallclimb.csv`](./wallclimb.csv) | Escalar pared vertical + giro 180° y salto (`MOVE_WallClimb180TurnJump`) | `1.80 s` (217 muestras) | `1`, `6` (`MOVE_WallClimbing`), `50` (`MOVE_WallClimb180TurnJump`), `2` |
| [`ledge_grab.csv`](./ledge_grab.csv) | Saltar a un borde, colgarse y subir (`IntoGrab` → `Grabbing` → `GrabPullUp`) | `1.80 s` (217 muestras) | `11`, `14` (`MOVE_IntoGrab`), `3` (`MOVE_Grabbing`), `10` (`MOVE_GrabPullUp`), `1` |
| [`slide.csv`](./slide.csv) | Deslizarse desde sprint (`MOVE_Slide`) hasta `SlideAbortSpeed = 250 uu/s` → `Crouch` | `1.80 s` (217 muestras) | `1`, `16` (`MOVE_Slide`), `15` (`MOVE_Crouch`) |
| [`hard_landing.csv`](./hard_landing.csv) | Caída dura desde `> 530 uu` resuelta con `GuestBridge::OnLanded` | `1.80 s` (217 muestras) | `2` (`MOVE_Falling`), `72` (`MOVE_FallingUncontrolled`), `20` (`MOVE_Landing`) |
| [`skill_roll.csv`](./skill_roll.csv) | Caída con `Coil` + `Skill Roll` (`GuestBridge::OnCrouchPressed` + `OnLanded`) | `1.80 s` (217 muestras) | `2`, `61` (`MOVE_Coil`), `91` (`MOVE_SkillRoll`), `1` |

---

## 5. Comandos para Reproducir o Grabar en Vivo

```powershell
# Compilar y ejecutar pruebas unitarias (76 checks en x86)
mod\build.bat

# Verificar offsets en vivo (solo lectura) con Mirror's Edge abierto
python mod/tools/me_telemetry/record_me_csv.py --probe

# Grabar una sesión en vivo desde Mirror's Edge a 120 Hz
python mod/tools/me_telemetry/record_me_csv.py --record qa/telemetria/mi_captura.csv --duration 10 --hz 120

# Verificar integridad de los 9 CSVs de referencia
python mod/tools/me_telemetry/record_me_csv.py --verify
```
