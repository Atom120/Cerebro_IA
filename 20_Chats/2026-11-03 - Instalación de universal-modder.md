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

## Pendientes
- [ ] Reiniciar Claude Code para que carguen las skills, el hook y el MCP.
- [ ] Instalar **ffmpeg** (falta; lo usan vídeos y grabación). Blender solo si se renderiza 3D → sprites.
- [ ] Si se quieren assets con fal: crear API key y guardarla con `um fal key set` (no en el repo).

## Relacionado
- [[Inicio]]
- [[10_Proyectos/ApertureRunner/README|ApertureRunner]]
