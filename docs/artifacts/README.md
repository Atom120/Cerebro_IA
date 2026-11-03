# Artefactos Compartidos entre Dispositivos (`docs/artifacts/`)

Por defecto, Antigravity guarda los artefactos de conversación (planes de implementación, análisis, diagramas) en la carpeta local de la máquina (`~/.gemini/antigravity/brain/<conversation-id>/`), la cual **no** se sincroniza entre distintas computadoras ni cuentas.

## Cómo usar esta carpeta
Cuando una instancia de Antigravity genere un artefacto que deba ser leído o continuado desde otra computadora o cuenta:
1. Copia o guarda el archivo Markdown/imagen dentro de `docs/artifacts/` (ej. `docs/artifacts/2026-11-01-plan-inicial.md`).
2. Referéncialo en [`.agents/memory/HANDOFF.md`](../../.agents/memory/HANDOFF.md).
3. Haz **Commit & Push** desde **GitHub Desktop**.
