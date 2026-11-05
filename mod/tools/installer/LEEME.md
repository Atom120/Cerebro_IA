# ApertureRunner: Mirror's Edge dentro de Portal

Juega el primer nivel de Portal (`testchmb_a_00`) moviéndote como Faith: sprint con inercia, salto, wallrun, wallclimb, agarrar bordes, slide, Skill Roll y giro de 180°. Los portales se cruzan sin perder velocidad.

## Requisitos
- **Portal (Steam)**, build 19017868. El instalador te avisa si tienes otra.
- Windows. El mod es una DLL de 32 bits que Portal carga como plugin de servidor.

## Instalar
1. Cierra Portal.
2. Doble clic en `instalar.bat`. Busca Portal solo; si no lo encuentra, ejecuta en PowerShell `install.ps1 -PortalRoot "...\steamapps\common\Portal"`.
3. Abre Portal y empieza una partida nueva (`testchmb_a_00`).
4. Abre la consola y escribe `ar_enable 1`. El ajuste se recuerda para la próxima vez.

## Controles (los de Portal)
| Tecla de Portal | Faith |
|---|---|
| Saltar | saltar, wallrun (junto a una pared), wallclimb (de frente a una pared), subir a un borde, salto de pared |
| Agacharse | agacharse, slide (corriendo), coil en el aire, Skill Roll (justo antes de aterrizar), soltarse de un borde |
| Recargar | giro de 180° |
| Caminar | caminar despacio |

## Comandos de consola
- `ar_enable 0|1`: movimiento de Faith desactivado o activado (se guarda).
- `ar_camera 0|1`: cámara de Faith (giro, inclinación, FOV, balanceo) desactivada o activada (se guarda).
- `ar_status`: estado actual.

## Desinstalar
Doble clic en `desinstalar.bat`. Tus ajustes quedan en `portal/addons/aperture_runner.cfg`.

## Aviso
Proyecto de fans sin relación con Valve ni EA. No incluye archivos de ninguno de los dos juegos.
