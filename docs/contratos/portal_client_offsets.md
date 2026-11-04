# Lado cliente de Portal (G1): predicción y offsets de `client.dll`

- **Tarea:** G1 (Gonzalo). **La hizo Sebas** el 2026-11-04 porque Gonzalo no había empezado. No modificar sin hablar con Sebas.
- **Build:** Portal 19017868, `portal/bin/client.dll`, `TimeDateStamp 0x68362D89`.
- Todo lo que sigue se midió en el juego real, no en mocks.

## 1. ¿Predice el cliente el movimiento en un jugador? **No.**
- En `testchmb_a_00`, `cl_predict` vale **0** (por defecto, en partida de un jugador).
- `ClientMovementHook` (`mod/plugin/client/`) engancha la copia de `GameMovement001` de `client.dll` (ranura 1, `ProcessMovement`).
  - Con `cl_predict 0`: **0 ticks predichos** en ~2 minutos de partida.
  - El servidor, en cambio, interceptó más de 1.500 ticks en el mismo tiempo.
- **Consecuencia para G2:** en un jugador el cliente solo muestra lo que calcula el servidor. Por eso, con `ar_enable 1` no hay predicción que corregir ni tirones por predecir a Chell. G2 **no aplica** mientras `cl_predict` sea 0.
  - El hook queda instalado y registra en la consola si algún día se predicen ticks: `Prediccion del cliente activa (N ticks predichos)`.
  - Si se activa `cl_predict 1`, el plan es ejecutar `faith_move` también en el cliente (es determinista). Haría falta una caché de `FaithState` por comando para las repeticiones de la predicción.

## 2. Cómo se engancha desde la misma DLL
- Portal es un solo proceso: la DLL del plugin de servidor puede usar `GetModuleHandleA("client.dll")` y llamar a `GetProcAddress("CreateInterface")` con `"GameMovement001"`.
- La vtable es la misma que en el servidor: `ProcessMovement` en la ranura 1. `CMoveData` tiene el mismo layout (ver `source_2013_abi.md`).
- Para el estado del movimiento no hace falta `CreateMove`. Solo haría falta para cambiar la vista desde el cliente (el giro de 180°, G4).

## 3. Offsets de Benja (`LESSONS.md`, 2026-11-01): **válidos en este build**
| Símbolo | Offset | Comprobación (2026-11-04) |
|---|---|---|
| `MainViewOrigin` (`g_vecRenderOrigin`) | `client.dll + 0x0050F484` | Se leyó `(-620.1, -1021.3, 192.0)`: suelo 128 + vista 64, coherente |
| `MainViewAngles` (`g_vecRenderAngles`) | `client.dll + 0x0050F490` | `(16.6, 115.5, 0.0)` |
| `s_pLocalPlayer` | `client.dll + 0x004D78F0` | No se comprobó (no hizo falta) |

- La lectura se hizo desde fuera, en solo lectura (`ReadProcessMemory`), con un script temporal.
- La base de `client.dll` cambia en cada arranque (ASLR): usa siempre base + offset.

## 4. Pendiente (G4, necesita el modelo de Antonio)
- Aplicar `mod/camera` (G3) a la vista: inclinación, balanceo, FOV, sacudidas y giro de 180°. Hay que enganchar el cálculo de la vista del cliente.
- Dibujar los brazos y el cuerpo de Faith.
