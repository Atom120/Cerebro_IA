---
name: colab-sync
description: >-
  Use this skill when synchronizing context across computers or accounts with Git and GitHub Desktop,
  checking repository status at the start or end of a session, or resolving multi-device state.
---

# Sincronización Multi-Dispositivo (Colab Sync)

Esta skill guía a Antigravity para sincronizar el estado del repositorio y el contexto compartido cuando se trabaja entre varias computadoras y/o cuentas mediante **GitHub Desktop** o Git.

## 1. Al Iniciar en una Computadora (Recepción / Pull)

1. **Inspeccionar el estado del repositorio**:
   - En Windows (PowerShell): ejecuta [sync-status.ps1](./scripts/sync-status.ps1)
   - En macOS / Linux (Bash): ejecuta [sync-status.sh](./scripts/sync-status.sh)
2. **Verificar cambios remotos pendientes**:
   - Si el script muestra que la rama local está detrás de `origin/main` (o la rama activa), ejecuta `git pull --rebase` (si no hay cambios locales sin guardar) o indica al usuario que presione **Pull origin** en **GitHub Desktop**.
3. **Leer el Relevo Activo**:
   - Revisa [HANDOFF.md](../../memory/HANDOFF.md) y [DECISIONS.md](../../memory/DECISIONS.md) para confirmar qué hizo la última instancia de Antigravity y cuáles son los siguientes pasos.

## 2. Al Terminar en una Computadora (Entrega / Push)

1. **Actualizar el contexto compartido**:
   - Usa la skill `session-handoff` para actualizar [HANDOFF.md](../../memory/HANDOFF.md) y [SESSIONS.md](../../memory/SESSIONS.md).
   - Si hubo aprendizajes nuevos en la sesión, usa `cross-device-learn` para registrarlos en [LESSONS.md](../../memory/LESSONS.md).
2. **Verificar que todos los cambios relevantes estén listos**:
   - Ejecuta `git status` para confirmar que no queden archivos temporales basura ni secretos expuestos.
3. **Guardar para GitHub Desktop**:
   - Si el usuario prefiere hacer el commit visualmente en **GitHub Desktop**, proporciónale un título y descripción de commit claros (ej. `chore(context): handoff sesión [tema]`).
   - Si el usuario pide que hagas el commit directamente, ejecuta `git add -A && git commit -m "<mensaje>"` y `git push` (si el remoto ya está configurado).
