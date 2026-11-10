# ApertureRunner: instala (o desinstala) el mod en Portal (Steam, app 400). S5, Sebas.
#   powershell -ExecutionPolicy Bypass -File install.ps1                 # instala
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall      # desinstala
#   powershell -ExecutionPolicy Bypass -File install.ps1 -PortalRoot "D:\SteamLibrary\steamapps\common\Portal"
#   powershell -ExecutionPolicy Bypass -File install.ps1 -MirrorsEdge "D:\Games\Mirror's Edge"   # S5: + modelo de Faith
# Busca la DLL junto al script (paquete) o en build\x86 (repositorio). Escribe en portal\addons y, con el
# extractor, en portal\custom\aperture_runner (el modelo de Faith sale de TU copia de Mirror's Edge).
param([string]$PortalRoot = "", [string]$MirrorsEdge = "", [string]$UModel = "", [switch]$SkipModel, [switch]$Uninstall)
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
    $custom = Join-Path $portal "portal\custom\aperture_runner"
    if (Test-Path $custom) { Remove-Item -Recurse -Force $custom; Write-Host "Quitado custom\aperture_runner (modelo de Faith)" }
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

# S5: modelo de Faith (brazos y piernas) desde la copia de Mirror's Edge del jugador (A4).
function Find-MirrorsEdge {
    if ($MirrorsEdge) { return $MirrorsEdge }
    $repoCfg = Join-Path $PSScriptRoot "..\..\..\config\paths.local.json"
    if (Test-Path $repoCfg) {
        $p = (Get-Content $repoCfg -Raw | ConvertFrom-Json).paths.mirrors_edge_root
        if ($p -and (Test-Path (Join-Path $p "Binaries\MirrorsEdge.exe"))) { return $p }
    }
    $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction SilentlyContinue).SteamPath
    if ($steam) {
        $libs = @($steam)
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) {
            foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value -replace '\\\\', '\' }
        }
        foreach ($lib in $libs | Select-Object -Unique) {
            $p = Join-Path $lib "steamapps\common\mirrors edge"
            if (Test-Path (Join-Path $p "Binaries\MirrorsEdge.exe")) { return $p }
        }
    }
    return $null
}
$extractor = @((Join-Path $PSScriptRoot "extractor\extract.py"),
               (Join-Path $PSScriptRoot "..\extractor\extract.py")) | Where-Object { Test-Path $_ } | Select-Object -First 1
# Python 3.10+ que de verdad arranque (el lanzador "py" puede apuntar a una instalacion borrada).
$python = $null
foreach ($cand in @(@("python"), @("py", "-3"))) {
    if (-not (Get-Command $cand[0] -ErrorAction SilentlyContinue)) { continue }
    $pyArgs = @($cand | Select-Object -Skip 1) + @("-c", "import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)")
    & $cand[0] @pyArgs 2>$null
    if ($LASTEXITCODE -eq 0) { $python = $cand; break }
}
$me = Find-MirrorsEdge
if ($SkipModel) {
    Write-Host "Sin modelo de Faith (-SkipModel)."
} elseif (-not $extractor) {
    Write-Warning "No encuentro el extractor: Faith se movera igual, pero sin brazos ni piernas."
} elseif (-not $python) {
    Write-Warning "Para los brazos y piernas de Faith hace falta Python 3.10+ (python.org). El movimiento funciona igual."
} elseif (-not $me) {
    Write-Warning "No encontre Mirror's Edge. Para los brazos y piernas de Faith, vuelve a ejecutar con -MirrorsEdge ""...\Mirror's Edge"" (la carpeta con Binaries y TdGame)."
} else {
    Write-Host ""
    Write-Host "Creando el modelo de Faith desde tu Mirror's Edge ($me). Tarda unos minutos..."
    $extArgs = @($python | Select-Object -Skip 1) + @($extractor, "--portal-root", $portal, "--me-root", $me)
    if ($UModel) { $extArgs += @("--umodel", $UModel) }
    $env:PYTHONIOENCODING = "utf-8"
    & $python[0] @extArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "El extractor no termino (mira el mensaje de arriba). El movimiento de Faith funciona igual."
    }
}
Write-Host ""
Write-Host "Como jugar: abre Portal, carga el primer mapa (testchmb_a_00) y escribe en la consola:"
Write-Host "  ar_enable 1     (movimiento de Faith; se recuerda para la proxima vez)"
Write-Host "  ar_status       (estado)        ar_camera 0|1   (camara de Faith)"
