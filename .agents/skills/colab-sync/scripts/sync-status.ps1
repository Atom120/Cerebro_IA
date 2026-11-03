Write-Host "=== Estado de Sincronizacion AntigravityColab ===" -ForegroundColor Cyan
Write-Host "`n[1] Usuario y Maquina actual:" -ForegroundColor Yellow
Write-Host "Computadora: $env:COMPUTERNAME | Usuario OS: $env:USERNAME"
git config user.name
git config user.email

Write-Host "`n[2] Remotos configurados:" -ForegroundColor Yellow
git remote -v

Write-Host "`n[3] Estado de la rama y arbol de trabajo:" -ForegroundColor Yellow
git status -sb

Write-Host "`n[4] Ultimos 5 commits:" -ForegroundColor Yellow
git log -n 5 --oneline --decorate
