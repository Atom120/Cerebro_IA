---
name: build-x86
description: Compila el mod ApertureRunner (carpeta mod/) para Win32/x86 y ejecuta todas las pruebas. Úsala antes de cada PR, después de cambiar código C++ o cuando el usuario pida compilar o probar.
---

# build-x86

Portal 1 es de 32 bits: una DLL x64 no carga. Este es el único build válido.

## Pasos
1. Desde la raíz del repo ejecuta (Windows, cmd o PowerShell):
   ```
   mod\build.bat
   ```
   Usa `mod\build.bat Debug` para depurar. El script busca `cmake` en el PATH o el que trae Visual Studio.
2. Resultado en `build\x86\` (ignorado por git). Debe terminar con `OK: build\x86` y `100% tests passed`.
3. Si falla la compilación, lee el primer error (no el último) y arréglalo solo en la carpeta del autor (ver `PLAN_EQUIPO.md`, sección 1). Si el error está en `mod/include/ar/` o `mod/core/`, no lo cambies: avisa a Sebas.

## Reglas
- Nunca cambies a x64 para "arreglar" un error.
- `/W4 /WX`: los avisos cuentan como errores en las pruebas del núcleo y de contratos; mantén el mismo nivel en tus módulos.
- No borres `build\` con comandos recursivos fuera de esa carpeta.
