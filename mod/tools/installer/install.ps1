# ApertureRunner: instala (o desinstala) el mod en Portal (Steam, app 400). S5, Sebas.
#   powershell -ExecutionPolicy Bypass -File install.ps1                 # instala
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall      # desinstala
#   powershell -ExecutionPolicy Bypass -File install.ps1 -PortalRoot "D:\SteamLibrary\steamapps\common\Portal"
# Busca la DLL junto al script (paquete) o en build\x86 (repositorio). No toca nada fuera de portal\addons.
param([string]$PortalRoot = "", [switch]$Uninstall)
$ErrorActionPreference = "Stop"
$TargetBuild = "19017868"

function Find-Portal {
    if ($PortalRoot) { return $PortalRoot }
    # 1) config\paths.local.json del repositorio, si se ejecuta desde el repo
    $repoCfg = Join-Path $PSScriptRoot "..\..\..\config\paths.local.json"
    if (Test-Path $repoCfg) {
        $p = (Get-Content $repoCfg -Raw | ConvertFrom-Json).paths.portal_root
        if ($p -and (Test-Path (Join-Path $p "hl2.exe"))) { return $p }
    }
    # 2) Steam: carpeta principal y bibliotecas de libraryfolders.vdf
    $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction SilentlyContinue).SteamPath
    if ($steam) {
        $libs = @($steam)
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) {
            foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' }
        }
        foreach ($lib in $libs | Select-Object -Unique) {
            if (Test-Path (Join-Path $lib "steamapps\appmanifest_400.acf")) {
                $p = Join-Path $lib "steamapps\common\Portal"
                if (Test-Path (Join-Path $p "hl2.exe")) { return $p }
            }
        }
    }
    throw "No encontre Portal. Pasa la carpeta con -PortalRoot ""...\steamapps\common\Portal""."
}

$portal = (Resolve-Path (Find-Portal)).Path
$addons = Join-Path $portal "portal\addons"
Write-Host "Portal: $portal"

if ($Uninstall) {
    foreach ($f in "aperture_runner.dll", "aperture_runner.vdf") {
        $p = Join-Path $addons $f
        if (Test-Path $p) { Remove-Item $p; Write-Host "Quitado $f" }
    }
    Write-Host "Desinstalado. (Se conserva addons\aperture_runner.cfg con tus ajustes.)"
    return
}

$manifest = Join-Path $portal "..\..\appmanifest_400.acf"
if (Test-Path $manifest) {
    $build = (Select-String -Path $manifest -Pattern '"buildid"\s+"(\d+)"').Matches[0].Groups[1].Value
    if ($build -ne $TargetBuild) {
        Write-Warning "Tu Portal es la build $build; el mod esta probado en $TargetBuild. Puede no funcionar."
    } else { Write-Host "Build de Portal ${build}: OK" }
}

$dll = @((Join-Path $PSScriptRoot "aperture_runner.dll"),
         (Join-Path $PSScriptRoot "..\..\..\build\x86\plugin\Release\aperture_runner.dll")) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $dll) { throw "No encuentro aperture_runner.dll (junto a este script o en build\x86; compila con mod\build.bat)." }

if (Get-Process hl2 -ErrorAction SilentlyContinue) { throw "Cierra Portal antes de instalar (la DLL esta en uso)." }
New-Item -ItemType Directory -Force $addons | Out-Null
Copy-Item $dll (Join-Path $addons "aperture_runner.dll") -Force
Set-Content -Path (Join-Path $addons "aperture_runner.vdf") -Encoding ascii -Value @'
"Plugin"
{
	"file"	"addons/aperture_runner"
}
'@
Write-Host "Instalado en $addons"
Write-Host ""
Write-Host "Como jugar: abre Portal, carga el primer mapa (testchmb_a_00) y escribe en la consola:"
Write-Host "  ar_enable 1     (movimiento de Faith; se recuerda para la proxima vez)"
Write-Host "  ar_status       (estado)        ar_camera 0|1   (camara de Faith)"
