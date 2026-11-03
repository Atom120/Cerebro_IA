# Copies the mod DLL into Portal, writes the plugin .vdf and starts Portal windowed on a map.
param(
    [string]$Map = "testchmb_a_00",
    [switch]$Insecure,
    [switch]$NoLaunch
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path

$cfgPath = Join-Path $root "config\paths.local.json"
if (-not (Test-Path $cfgPath)) { throw "Falta config\paths.local.json (copia config\paths.example.json y pon tus rutas)." }
$portal = (Get-Content $cfgPath -Raw | ConvertFrom-Json).paths.portal_root
if (-not (Test-Path (Join-Path $portal "hl2.exe"))) { throw "portal_root no apunta a la carpeta de Portal: $portal" }

$manifest = Join-Path $portal "..\..\appmanifest_400.acf"
if (Test-Path $manifest) {
    $build = (Select-String -Path $manifest -Pattern '"buildid"\s+"(\d+)"').Matches[0].Groups[1].Value
    if ($build -ne "19017868") { Write-Warning "Portal build $build; el mod apunta a 19017868. Las firmas pueden no coincidir." }
}

$dll = Get-ChildItem (Join-Path $root "build\x86") -Recurse -Filter "aperture_runner.dll" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $dll) { throw "No hay build\x86\...\aperture_runner.dll. Compila primero con mod\build.bat." }

$addons = Join-Path $portal "portal\addons"
New-Item -ItemType Directory -Force $addons | Out-Null
Copy-Item $dll.FullName (Join-Path $addons "aperture_runner.dll") -Force
Set-Content -Path (Join-Path $addons "aperture_runner.vdf") -Encoding ascii -Value @'
"Plugin"
{
	"file"	"addons/aperture_runner"
}
'@
Write-Host "Copiado $($dll.FullName) -> $addons"

if ($NoLaunch) { return }
$gameArgs = @("-game", "portal", "-novid", "-windowed", "-w", "1280", "-h", "720", "+developer", "1", "+map", $Map)
if ($Insecure) { $gameArgs = @("-insecure") + $gameArgs }
Start-Process -FilePath (Join-Path $portal "hl2.exe") -ArgumentList $gameArgs -WorkingDirectory $portal
Write-Host "Portal lanzado en $Map"
