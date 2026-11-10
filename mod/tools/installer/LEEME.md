# ApertureRunner: Mirror's Edge dentro de Portal

Juega el primer nivel de Portal (`testchmb_a_00`) moviéndote como Faith: sprint con inercia, salto, wallrun, wallclimb, agarrar bordes, slide, Skill Roll y giro de 180°. Los portales se cruzan sin perder velocidad. Si tienes Mirror's Edge, además verás los brazos y las piernas de Faith en primera persona.

## Requisitos
- **Portal (Steam)**, build 19017868. El instalador te avisa si tienes otra.
- Windows. El mod es una DLL de 32 bits que Portal carga como plugin de servidor.
- **Para ver los brazos y las piernas de Faith (opcional):**
  - Tu propia copia de **Mirror's Edge** para PC (probado con GOG v1.01; Steam y EA usan los mismos archivos).
  - **Python 3.10 o superior** ([python.org](https://www.python.org/downloads/)).
  - **UE Viewer** (`umodel_win32.zip`, de https://www.gildor.org/en/projects/umodel), descomprimido en `extractor\umodel\`. Tiene que quedar `extractor\umodel\umodel.exe`.
  - El paquete no trae nada de Mirror's Edge: el modelo se crea en tu PC a partir de tu juego.

## Instalar
1. Cierra Portal.
2. Haz doble clic en `instalar.bat`. Busca Portal y Mirror's Edge (de Steam) por su cuenta. Si no los encuentra, ejecuta en PowerShell:
   `install.ps1 -PortalRoot "...\steamapps\common\Portal" -MirrorsEdge "...\Mirror's Edge"`
   La carpeta de Mirror's Edge es la que contiene `Binaries` y `TdGame`.
   Crear el modelo de Faith tarda unos minutos. Si falta algo, el mod se instala igual y te dice qué falta.
3. Abre Portal y empieza una partida nueva (`testchmb_a_00`).
4. Abre la consola y escribe `ar_enable 1`. El ajuste se recuerda para la próxima vez.

## Controles (los de Portal)
| Tecla de Portal | Faith |
|---|---|
| Saltar | saltar, wallrun (junto a una pared), wallclimb (de frente a una pared), subir a un borde, salto de pared |
| Agacharse | agacharse, slide (corriendo), coil en el aire, Skill Roll (justo antes de aterrizar), soltarse de un borde |
| Recargar | giro de 180° |
| Caminar | caminar despacio |

Si miras hacia abajo, se ocultan los brazos y ves tus piernas.

## Comandos de consola
- `ar_enable 0|1`: activa o desactiva el movimiento de Faith (se guarda).
- `ar_camera 0|1`: activa o desactiva la cámara de Faith: giro, inclinación, FOV y balanceo (se guarda).
- `ar_status`: muestra el estado actual, incluido si están activos los brazos y el cuerpo de Faith.

## Desinstalar
Haz doble clic en `desinstalar.bat`. Quita la DLL y el modelo de Faith (`portal/custom/aperture_runner`). Tus ajustes se quedan en `portal/addons/aperture_runner.cfg`.

## Aviso
Proyecto de fans sin relación con Valve ni con EA. No incluye archivos de ninguno de los dos juegos.
