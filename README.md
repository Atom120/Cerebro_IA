# Cerebro_IA

Fuente central de conocimiento: todo lo que se desarrolla en proyectos y chats con IA (Claude, Antigravity y otros) se guarda aquí como notas Markdown, habilidades y memoria compartida, organizadas por tema y sincronizadas con Git.

Esta carpeta es también la bóveda de Obsidian **Cerebro_IA_Obsidian**.

## Estructura de Obsidian

| Carpeta | Qué va ahí |
|---|---|
| `00_Inbox/` | Notas nuevas sin clasificar |
| `10_Proyectos/` | Una subcarpeta por proyecto (`10_Proyectos/<Nombre>/`) |
| `20_Chats/` | Resúmenes de chats: `AAAA-MM-DD - <tema>.md` |
| `30_Temas/` | Conocimiento consolidado por tema (notas "evergreen") |
| `90_Plantillas/` | Plantillas de notas |
| `99_Adjuntos/` | Imágenes, PDFs y otros archivos |

Punto de entrada: [[Inicio]] · Reglas para la IA: [[CLAUDE]] · Protocolo Antigravity: [`AGENTS.md`](AGENTS.md)

---

# AntigravityColab — Colaboración y Aprendizaje Multi-Dispositivo

Repositorio configurado para que múltiples instancias de **Antigravity** (en diferentes computadoras, sistemas operativos y/o cuentas) trabajen en conjunto manteniendo el contexto y aprendiendo de forma continua a través de **GitHub Desktop**.

---

## Cómo usarlo paso a paso con GitHub Desktop

1. **Publicar desde la primera computadora**:
   - Abre **GitHub Desktop** (verás los archivos nuevos listos en la pestaña *Changes*; o pídele a Antigravity que haga el commit directamente).
   - Haz clic en **Commit to main** y luego en **Publish repository** (o **Push origin**).
2. **En tus otras computadoras / cuentas**:
   - Clona el repositorio desde **GitHub Desktop** (`File -> Clone repository...`) y abre la carpeta en Antigravity. No necesitas configurar nada extra en `~/.gemini/`.
3. **Flujo diario entre dispositivos**:
   - **Al llegar a una PC**: Haz **Pull origin** en GitHub Desktop y abre Antigravity (leerá automáticamente [`.agents/memory/HANDOFF.md`](.agents/memory/HANDOFF.md)).
   - **Durante la sesión (aprendizaje compartido)**: Si corriges a Antigravity o quieres que recuerde una regla en todas tus computadoras/cuentas, díselo (o usa `/learn`). El agente lo guardará en [`.agents/memory/LESSONS.md`](.agents/memory/LESSONS.md), [`.agents/memory/MEMORY.md`](.agents/memory/MEMORY.md) o en `.agents/skills/`.
   - **Antes de cambiar de PC**: Pídele a Antigravity *"Prepara el handoff"* y haz **Commit & Push** en GitHub Desktop.

---

## ¿Por qué funciona entre distintas cuentas y computadoras?

Por defecto, Antigravity guarda las conversaciones y configuraciones globales en `~/.gemini/`, que es local a cada máquina. Para superar esto, este repositorio utiliza el sistema de descubrimiento de espacio de trabajo (`.agents/` y `AGENTS.md`) **dentro del propio repositorio Git**, de modo que cualquier cuenta o dispositivo que clone el repositorio adquiera de inmediato toda la memoria, reglas y habilidades:

1. **Reglas e Inyección Automática de Contexto ([`AGENTS.md`](AGENTS.md))**:
   - Se carga automáticamente en cada sesión de Antigravity sin importar la cuenta o computadora.
   - Incluye en tiempo real mediante directivas `@[...](...)` la memoria global, el estado de relevo y las lecciones aprendidas.
   - Establece la regla de **nunca guardar aprendizajes únicamente en `~/.gemini/`**, sino dentro del repositorio.
2. **Sistema de Memoria Persistente (`.agents/memory/`)**:
   - [`MEMORY.md`](.agents/memory/MEMORY.md): Contexto permanente del proyecto y preferencias globales.
   - [`HANDOFF.md`](.agents/memory/HANDOFF.md): Tablero de relevo activo (*qué hizo la última computadora, en qué se quedó y cuáles son los próximos pasos*).
   - [`LESSONS.md`](.agents/memory/LESSONS.md): Bitácora acumulativa de aprendizaje entre dispositivos (errores resueltos, correcciones del usuario, patrones efectivos).
   - [`DECISIONS.md`](.agents/memory/DECISIONS.md): Registro de decisiones arquitectónicas y técnicas para que otra instancia no contradiga acuerdos previos.
   - [`SESSIONS.md`](.agents/memory/SESSIONS.md): Historial cronológico de sesiones.
3. **Skills Compartidas (`.agents/skills/` y [`skills.json`](.agents/skills.json))**:
   - **`colab-sync`** ([`SKILL.md`](.agents/skills/colab-sync/SKILL.md), [`sync-status.ps1`](.agents/skills/colab-sync/scripts/sync-status.ps1), [`sync-status.sh`](.agents/skills/colab-sync/scripts/sync-status.sh)): Revisa el estado de sincronización de Git al iniciar o cerrar sesión en Windows, macOS o Linux.
   - **`cross-device-learn`** ([`SKILL.md`](.agents/skills/cross-device-learn/SKILL.md)): Se activa cuando corriges al agente, usas `/learn` o pides recordar algo, guardándolo en [`LESSONS.md`](.agents/memory/LESSONS.md), [`MEMORY.md`](.agents/memory/MEMORY.md) o como una nueva Skill en `.agents/skills/`.
   - **`session-handoff`** ([`SKILL.md`](.agents/skills/session-handoff/SKILL.md)): Empaqueta el avance de la sesión actual y copia artefactos importantes a [`docs/artifacts/`](docs/artifacts/README.md) antes de cambiar de dispositivo.
4. **Hook de Inicio de Sesión ([`hooks.json`](.agents/hooks.json) y [`session-start.js`](.agents/scripts/session-start.js))**:
   - Al abrir una nueva conversación en cualquier computadora, inyecta automáticamente el nombre del dispositivo, la rama actual, el último commit y el estado de Git.
5. **Prevención de Conflictos en GitHub Desktop ([`.gitattributes`](.gitattributes) y [`.gitignore`](.gitignore))**:
   - Normaliza saltos de línea entre Windows (`CRLF` en `.ps1`) y macOS/Linux (`LF` en `.md`, `.json`, `.sh`).
   - Configura `merge=union` en [`LESSONS.md`](.agents/memory/LESSONS.md) y [`SESSIONS.md`](.agents/memory/SESSIONS.md) para que dos computadoras puedan agregar aprendizajes en paralelo sin generar conflictos de merge al hacer Pull/Push.

---

## Arquitectura del Sistema

```text
Cerebro_IA/
├── AGENTS.md                        # Reglas siempre activas + inyección automática de memoria
├── CLAUDE.md                        # Instrucciones generales de la bóveda Cerebro_IA
├── Inicio.md                        # Punto de entrada de la bóveda de Obsidian
├── README.md                        # Guía de uso paso a paso con Obsidian y GitHub Desktop
├── .gitattributes                   # Normalización LF/CRLF y merge=union sin conflictos
├── .gitignore                       # Protege estado local de Obsidian, secretos y versiona .agents/
├── .agents/
│   ├── hooks.json                   # Hook SessionStart para inyectar estado de Git al abrir chat
│   ├── skills.json                  # Registro explícito de skills compartidas
│   ├── scripts/
│   │   └── session-start.js         # Script multiplataforma de inicio de sesión
│   ├── memory/
│   │   ├── MEMORY.md                # Contexto global permanente y preferencias del usuario
│   │   ├── HANDOFF.md               # Tablero de relevo entre sesiones/computadoras
│   │   ├── LESSONS.md               # Aprendizaje acumulativo (merge=union)
│   │   ├── DECISIONS.md             # Decisiones de arquitectura (ADR)
│   │   └── SESSIONS.md              # Historial cronológico de sesiones (merge=union)
│   └── skills/
│       ├── colab-sync/              # Skill y scripts para sincronizar con Git / GitHub Desktop
│       ├── cross-device-learn/      # Skill para guardar aprendizajes en el repo (no en ~/.gemini)
│       └── session-handoff/         # Skill para preparar el relevo antes de cambiar de PC/cuenta
└── docs/
    └── artifacts/                   # Carpeta para exportar artefactos locales que deban compartirse
```
