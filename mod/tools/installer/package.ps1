# Crea el paquete para repartir: dist\ApertureRunner-<fecha>.zip con la DLL, el instalador y el LEEME.
#   mod\build.bat
#   powershell -ExecutionPolicy Bypass -File mod\tools\installer\package.ps1
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$dll = Join-Path $root "build\x86\plugin\Release\aperture_runner.dll"
if (-not (Test-Path $dll)) { throw "Falta $dll. Compila antes con mod\build.bat." }

$name = "ApertureRunner-" + (Get-Date -Format "yyyy-MM-dd")
$dist = Join-Path $root "dist"
$out = Join-Path $dist $name
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item $dll, (Join-Path $PSScriptRoot "install.ps1"), (Join-Path $PSScriptRoot "LEEME.md") $out
Set-Content -Path (Join-Path $out "instalar.bat") -Encoding ascii -Value "@powershell -ExecutionPolicy Bypass -File ""%~dp0install.ps1"" %*`r`n@pause"
Set-Content -Path (Join-Path $out "desinstalar.bat") -Encoding ascii -Value "@powershell -ExecutionPolicy Bypass -File ""%~dp0install.ps1"" -Uninstall`r`n@pause"
$zip = "$out.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip
Write-Host "Paquete: $zip"
Get-ChildItem $out | Select-Object Name, Length | Format-Table -AutoSize
