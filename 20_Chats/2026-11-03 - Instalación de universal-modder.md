---
tipo: chat
fecha: 2026-11-03
herramienta: Claude
proyecto:
tags: [claude-code, plugin, skills, modding, herramientas]
---
# Instalación de universal-modder

## Objetivo
- Implementar en Claude Code las skills del repo https://github.com/rehan-remade/universal-modder.

## Resumen
- Es un plugin de Claude Code (marketplace propio) para moddear juegos de PC con un agente IA: recon del motor, ingeniería inversa, assets con fal, pruebas en el juego, vídeos y una base de conocimiento compartida.
- Se revisó el contenido antes de instalar: hook `SessionStart` que solo añade `bin/` al PATH, un servidor MCP HTTP de fal (`https://mcp.fal.ai/mcp`, usa `FAL_KEY`) y el CLI `um` (Python, vía `uv`).

## Decisiones / resultados
- Instalado a nivel usuario (`scope: user`), habilitado: `universal-modder@universal-modder`.
- 13 skills: `mod-any-game` (punto de entrada), `game-recon`, `game-research-websearch`, `reverse-engineering`, `fal-assets`, `asset-pipeline`, `game-automation`, `showcase-video`, `mashup-mods`, `publish-mod`, `port-a-mod`, `share-field-notes`, `blitz3d-decomp`.
- `um --help` funciona (git, uv y python presentes).

## Código, comandos o archivos clave
```
claude plugin marketplace add rehan-remade/universal-modder
claude plugin install universal-modder@universal-modder
claude plugin update universal-modder@universal-modder   # para actualizar
```
- Ruta instalada: `C:\Users\Cleve\.claude\plugins\cache\universal-modder\universal-modder\<versión>`.
- Ejemplo de uso: «Mod Terraria: añade un lanzamisiles teledirigido…» o «¿Qué motor usa `C:\Games\Foo`?».

## Estado del equipo (2026-11-03)
- OK: git, uv, Python 3.13, Node, .NET SDK 10, Visual Studio 2022 y 18 (Community), Blender 3.6 y 5.0, **ffmpeg 9.0.2** (instalado con `winget install Gyan.FFmpeg`).
- Blender no está en el PATH y `um render3d` solo busca `C:\Program Files\Blender Foundation\Blender\blender.exe` → hay que definir `BLENDER=C:\Program Files\Blender Foundation\Blender 5.0\blender.exe`.
- Faltan (solo según el juego): Java JDK (mods de Minecraft Java), ILSpy (juegos .NET/Unity), Ghidra (código nativo).
- Juegos detectados con `um scan --list`: ARC Raiders, Deadlock (anticheat, online: no moddear), Minecraft Bedrock y Java.

## Pendientes
- [x] Reiniciar Claude Code para que carguen las skills, el hook y el MCP.
- [x] Instalar **ffmpeg**.
- [ ] Definir la variable `BLENDER` (ver arriba).
- [ ] Si se quieren assets con fal: crear API key y definir `FAL_KEY` (o `um fal key set`), nunca en el repo.

## Relacionado
- [[Inicio]]
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
