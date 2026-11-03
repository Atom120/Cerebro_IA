#!/usr/bin/env bash
echo "=== Estado de Sincronizacion AntigravityColab ==="
echo ""
echo "[1] Usuario y Maquina actual:"
echo "Computadora: $(hostname) | Usuario OS: $(whoami)"
git config user.name
git config user.email

echo ""
echo "[2] Remotos configurados:"
git remote -v

echo ""
echo "[3] Estado de la rama y arbol de trabajo:"
git status -sb

echo ""
echo "[4] Ultimos 5 commits:"
git log -n 5 --oneline --decorate
