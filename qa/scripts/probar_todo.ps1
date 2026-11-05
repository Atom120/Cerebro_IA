# Prueba ApertureRunner de punta a punta en cualquier PC con Portal (B3). Autor: Sebas (2026-11-05).
#   powershell -ExecutionPolicy Bypass -File qa\scripts\probar_todo.ps1                 # compila y prueba
#   powershell -ExecutionPolicy Bypass -File qa\scripts\probar_todo.ps1 -Dll <ruta>     # sin compilar: DLL del release
# Hace: 1) config\paths.local.json si falta (busca Portal en Steam), 2) DLL (compila con mod\build.bat o usa -Dll),
# 3) pruebas automaticas H2 y H3 en Portal, 4) guarda los informes como qa\resultados\H2_auto_<pc>.md y H3_auto_<pc>.md.
# No toques el teclado ni el raton mientras corre (~6 minutos): Portal se abre y se cierra solo varias veces.
param([string]$Dll = "", [string]$Pc = $env:COMPUTERNAME)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $root
$Pc = ($Pc -replace '[^A-Za-z0-9_-]', '').ToLower()

Write-Host "== 1/4 Configuracion" -ForegroundColor Cyan
$cfgPath = Join-Path $root "config\paths.local.json"
if (-not (Test-Path $cfgPath)) {
    $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction SilentlyContinue).SteamPath
    $portal = $null
    if ($steam) {
        $libs = @($steam)
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) { foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' } }
        foreach ($lib in $libs | Select-Object -Unique) {
            $p = Join-Path $lib "steamapps\common\Portal"
            if (Test-Path (Join-Path $p "hl2.exe")) { $portal = $p; break }
        }
    }
    if (-not $portal) { throw "No encontre Portal. Copia config\paths.example.json a config\paths.local.json y pon tu portal_root." }
    $example = Get-Content (Join-Path $root "config\paths.example.json") -Raw | ConvertFrom-Json
    $example.paths.portal_root = $portal -replace '\\', '/'
    $example.paths.portal_exe = (Join-Path $portal "hl2.exe") -replace '\\', '/'
    $example | ConvertTo-Json -Depth 5 | Set-Content -Path $cfgPath -Encoding utf8
    Write-Host "Creado config\paths.local.json con portal_root = $portal (revisa las rutas de Mirror's Edge si las usas)."
}
$portalRoot = (Get-Content $cfgPath -Raw | ConvertFrom-Json).paths.portal_root
if (-not (Test-Path (Join-Path $portalRoot "hl2.exe"))) { throw "portal_root no apunta a Portal: $portalRoot" }
$manifest = Join-Path $portalRoot "..\..\appmanifest_400.acf"
$build = if (Test-Path $manifest) { (Select-String -Path $manifest -Pattern '"buildid"\s+"(\d+)"').Matches[0].Groups[1].Value } else { "?" }
Write-Host "Portal: $portalRoot (build $build)"
if ($build -ne "19017868") { Write-Warning "El mod esta probado en la build 19017868; la tuya es $build. Si algo falla, avisa con esto." }

Write-Host "== 2/4 DLL" -ForegroundColor Cyan
$target = Join-Path $root "build\x86\plugin\Release\aperture_runner.dll"
if ($Dll) {
    if (-not (Test-Path $Dll)) { throw "No existe $Dll" }
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    Copy-Item $Dll $target -Force
    Write-Host "Usando la DLL de $Dll (sin compilar)."
} else {
    cmd /c "mod\build.bat"
    if ($LASTEXITCODE -ne 0) { throw "mod\build.bat fallo. Instala Visual Studio con C++ y CMake, o usa -Dll con la DLL del release." }
}
if (-not (Test-Path $target)) { throw "No hay $target" }

Write-Host "== 3/4 Pruebas en Portal (no toques el teclado)" -ForegroundColor Cyan
if (Get-Process hl2 -ErrorAction SilentlyContinue) { throw "Cierra Portal antes de empezar." }
$h2 = "qa\resultados\H2_auto_$Pc.md"
$h3 = "qa\resultados\H3_auto_$Pc.md"
powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "h2_smoke.ps1") -Report $h2
$h2ok = $LASTEXITCODE -eq 0
powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "h3_smoke.ps1") -Report $h3
$h3ok = $LASTEXITCODE -eq 0

Write-Host "== 4/4 Resultado" -ForegroundColor Cyan
Write-Host ("H2: {0}   H3: {1}" -f $(if ($h2ok) { "PASS" } else { "FALLOS" }), $(if ($h3ok) { "PASS" } else { "FALLOS" }))
Write-Host "Informes: $h2 y $h3"
Write-Host "Subelos con GitHub Desktop en una rama (p. ej. benja/B3-prueba-$Pc) y abre un PR. Si algo fallo, adjunta tambien portal\console.log."
exit $(if ($h2ok -and $h3ok) { 0 } else { 1 })
