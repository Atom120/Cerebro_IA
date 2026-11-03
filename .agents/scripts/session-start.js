#!/usr/bin/env node
/**
 * Hook SessionStart multiplataforma para AntigravityColab.
 * Lee el estado de Git y el resumen de .agents/memory/HANDOFF.md al iniciar una nueva conversación
 * e inyecta un mensaje de contexto inicial.
 */
const fs = require('fs');
const path = require('path');
const os = require('os');
const { execSync } = require('child_process');

function runGit(cmd, cwd) {
  try {
    return execSync(cmd, { cwd, encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
  } catch {
    return '';
  }
}

function main() {
  try {
    // Determinar la raíz del repositorio (hooks.json se ejecuta desde .agents/)
    const repoRoot = fs.existsSync(path.join(process.cwd(), 'memory', 'HANDOFF.md'))
      ? path.resolve(process.cwd(), '..')
      : process.cwd();

    const hostname = os.hostname();
    const platform = `${os.platform()} (${os.release()})`;
    const branch = runGit('git branch --show-current', repoRoot) || 'main';
    const gitStatus = runGit('git status -sb', repoRoot);
    const lastCommit = runGit('git log -n 1 --oneline', repoRoot);

    const msgLines = [
      `[AntigravityColab Multi-Device Sync Activo]`,
      `- Dispositivo actual: ${hostname} | SO: ${platform}`,
      `- Rama Git: ${branch} | Último commit: ${lastCommit || 'N/A'}`,
      `- Estado Git:\n${gitStatus || 'Limpio'}`,
      `- Recuerda actualizar .agents/memory/HANDOFF.md y .agents/memory/LESSONS.md para mantener el contexto y aprendizaje sincronizados entre dispositivos.`
    ];

    const output = {
      injectSteps: [
        {
          systemMessage: {
            systemMessage: msgLines.join('\n')
          }
        }
      ]
    };

    process.stdout.write(JSON.stringify(output));
  } catch {
    // Fallback seguro: nunca bloquear el inicio de sesión
    process.stdout.write(JSON.stringify({}));
  }
}

main();
