# Instrucciones para Claude (y cualquier IA) — Cerebro_IA

Este repositorio (https://github.com/Atom120/Cerebro_IA) es la memoria a largo plazo del usuario. Ruta local: `C:\Users\Cleve\OneDrive\Documentos\Cerebro_IA_Docs\Cerebro_IA_Obsidian`.

## Al empezar un chat o proyecto
1. Lee `Inicio.md` y busca en `30_Temas/` y `10_Proyectos/` notas relacionadas antes de responder.
2. Usa ese contexto; no repitas trabajo ya documentado.

## Al terminar (o cuando el usuario diga "guárdalo en el cerebro")
1. Crea un resumen en `20_Chats/AAAA-MM-DD - <tema>.md` con la plantilla `90_Plantillas/Chat.md`.
2. Si pertenece a un proyecto, actualiza `10_Proyectos/<Proyecto>/README.md`.
3. Si se aprendió algo reutilizable, crea o amplía la nota en `30_Temas/<Tema>.md`.
4. Enlaza notas con `[[wikilinks]]` y añade `tags` en el frontmatter.
5. Guarda y sube:
   ```
   git add -A
   git commit -m "<tipo>: <resumen corto>"
   git pull --rebase
   git push
   ```
   Tipos: `chat`, `proyecto`, `tema`, `docs`.

## Reglas
- Todo en Markdown, en español, nombres de archivo legibles.
- Nunca guardes contraseñas, API keys ni datos sensibles.
- No borres notas: si algo queda obsoleto, márcalo con `status: archivado`.
