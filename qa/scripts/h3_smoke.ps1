# Prueba automatica de H3 ("Parkour completo") en Portal 19017868 / testchmb_a_00 (B3).
# Autor: Sebas (2026-11-04). Igual que h2_smoke.ps1: abre Portal con el mod, inyecta la entrada con
# ar_qa_input y decide PASS/FAIL leyendo portal/console.log (y la vista con read_portal_view.py).
#
#   powershell -ExecutionPolicy Bypass -File qa\scripts\h3_smoke.ps1 -Report qa\resultados\H3_auto.md
param([string]$Report = "")
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$portal = (Get-Content (Join-Path $root "config\paths.local.json") -Raw | ConvertFrom-Json).paths.portal_root
$log = Join-Path $portal "portal\console.log"
$cfg = Join-Path $portal "portal\cfg\ar_qa_tmp.cfg"
$viewTool = Join-Path $PSScriptRoot "read_portal_view.py"

function Send([string[]]$commands) {
    [IO.File]::WriteAllText($cfg, ($commands -join "`n") + "`n")
    Start-Process -FilePath (Join-Path $portal "hl2.exe") -WorkingDirectory $portal -Wait `
        -ArgumentList @("-hijack", "-game", "portal", "+exec", "ar_qa_tmp")
    Start-Sleep -Milliseconds 300
}
function Mark { (Get-Content $log).Count }
function LogFrom([int]$line) { @(Get-Content $log | Select-Object -Skip $line) }
# [AR] t=.. anim=X vel2d=V vz=.. suelo=.. pos=(x y z) eventos=0x.. yaw=Y roll=R
function Samples($lines) {
    foreach ($l in $lines) {
        if ($l -match '^\[AR\] t=[\d.]+ anim=(\S+) vel2d=([\d.-]+) vz=\S+ suelo=\d pos=\(([\d.-]+) ([\d.-]+) ([\d.-]+)\) eventos=0x([0-9a-f]+) yaw=([\d.-]+) roll=([\d.-]+)') {
            [pscustomobject]@{ Anim = $Matches[1]; Speed = [double]$Matches[2]; X = [double]$Matches[3]; Y = [double]$Matches[4]
                               Z = [double]$Matches[5]; Events = [Convert]::ToInt32($Matches[6], 16); Yaw = [double]$Matches[7]; Roll = [double]$Matches[8] }
        }
    }
}
function View { $v = (& python -I $viewTool) -split ' '; [pscustomobject]@{ Z = [double]$v[2]; Yaw = [double]$v[4] } }
$results = New-Object System.Collections.Generic.List[object]
function Result($id, $name, $pass, $detail) {
    $results.Add([pscustomobject]@{ Id = $id; Prueba = $name; Resultado = $(if ($pass) { "PASS" } else { "FAIL" }); Detalle = $detail })
    Write-Host ("{0,-6} {1,-4} {2} - {3}" -f $id, $(if ($pass) { "PASS" } else { "FAIL" }), $name, $detail)
}

Get-Process hl2 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
& (Join-Path $root ".agents\skills\launch-portal-test\scripts\launch-portal-test.ps1") -NoLaunch | Out-Null
Remove-Item $log -ErrorAction SilentlyContinue
Start-Process -FilePath (Join-Path $portal "hl2.exe") -WorkingDirectory $portal -ArgumentList @(
    "-game", "portal", "-novid", "-windowed", "-w", "1280", "-h", "720", "-condebug", "+developer", "1",
    "+sv_cheats", "1", "+map", "testchmb_a_00")
for ($i = 0; $i -lt 120 -and -not ((Test-Path $log) -and (Select-String -Path $log -Pattern "Autotest Z4" -Quiet)); $i++) { Start-Sleep 1 }
Start-Sleep 20
$auto = Select-String -Path $log -Pattern "Autotest Z4.*" | Select-Object -Last 1
Result "H3-01" "Plugin y trazas reales (autotest Z4)" ($auto -and $auto.Line -match ': OK') ""
Send @("ar_enable 1"); Start-Sleep 1

# Wallrun en la pared sur del pasillo (y = -992): corre +X con la pared a la derecha y pulsa Up.
Send @("setpos -1100 -970 129", "setang 0 0 0"); Start-Sleep 1.5
$m = Mark; Send @("ar_debug 2", "ar_qa_input 1 0 j 100 25"); Start-Sleep 2.0; Send @("ar_debug 0"); Start-Sleep 0.4
$s = @(Samples (LogFrom $m))
$wr = @($s | Where-Object { $_.Anim -eq "wallrun_right" })
$minRoll = ($wr | Measure-Object Roll -Minimum).Minimum
Result "H3-02" "Wallrun en una pared real del mapa" ($wr.Count -ge 10 -and @($s | Where-Object { $_.Events -band 0x10 }).Count -ge 1) ("{0} ticks en wallrun_right" -f $wr.Count)
Result "H3-03" "La vista se inclina en el wallrun (camara G3/G4)" ($wr.Count -gt 0 -and $minRoll -le -8) ("roll minimo {0:N1} grados (objetivo -15)" -f $minRoll)

# Wallclimb: de frente a la pared sur.
Send @("setpos -1000 -900 129", "setang 0 270 0"); Start-Sleep 1.5
$m = Mark; Send @("ar_debug 1", "ar_qa_input 1 0 j 110 40"); Start-Sleep 2.0; Send @("ar_debug 0"); Start-Sleep 0.4
$s = @(Samples (LogFrom $m))
$climb = @($s | Where-Object { $_.Events -band 0x40 })
$top = ($s | Measure-Object Z -Maximum).Maximum - 128.0
Result "H3-04" "Wallclimb en una pared real" ($climb.Count -ge 1 -and $top -gt 40) ("sube {0:N0} su" -f $top)

# Giro de 180 grados: la vista gira 180.
Send @("setpos -1000 -864 129", "setang 0 0 0"); Start-Sleep 1.5
$before = View
Send @("ar_qa_input 0 0 t 2"); Start-Sleep 1.0
$after = View
$turn = [math]::Abs((($after.Yaw - $before.Yaw) + 540) % 360 - 180)
Result "H3-05" "Giro de 180 grados en la vista (Turn180 + camara)" ([math]::Abs($turn - 180) -lt 2) ("yaw {0:N1} -> {1:N1}" -f $before.Yaw, $after.Yaw)

# Agacharse baja la vista del motor (casco agachado + FL_DUCKING).
$stand = View
Send @("ar_qa_input 0 0 d 200"); Start-Sleep 1.2
$crouch = View
Start-Sleep 2.5
Result "H3-06" "Agacharse baja la vista del motor (64 -> 28)" ([math]::Abs(($stand.Z - $crouch.Z) - 36) -lt 2) ("vista z {0:N1} -> {1:N1}" -f $stand.Z, $crouch.Z)

# Slide desde el sprint.
Send @("setpos -660 -864 129", "setang 0 180 0"); Start-Sleep 1.5
$m = Mark; Send @("ar_debug 1", "ar_qa_input 1 0 d 90 45"); Start-Sleep 1.8; Send @("ar_debug 0"); Start-Sleep 0.4
$s = @(Samples (LogFrom $m))
Result "H3-07" "Slide desde el sprint" (@($s | Where-Object { $_.Events -band 0x100 }).Count -ge 1) ""

# Cruzar un portal conserva la inercia (P4).
Send @("ent_fire relay_portal_cancel_room1 trigger"); Start-Sleep 2
Send @("setpos -591 -290 162", "setang 0 270 0"); Start-Sleep 1.2
$m = Mark; Send @("ar_debug 2", "ar_qa_input 1 0 - 150"); Start-Sleep 1.4; Send @("ar_debug 0", "ar_status"); Start-Sleep 0.6
$lines = LogFrom $m
$s = @(Samples $lines)
$in = $null; $out = $null
for ($k = 1; $k -lt $s.Count; $k++) {
    if ([math]::Abs($s[$k].X - $s[$k - 1].X) + [math]::Abs($s[$k].Y - $s[$k - 1].Y) -gt 100) { $in = $s[$k - 1]; $out = $s[$k]; break }
}
$crossings = (($lines | Where-Object { $_ -match 'cruces=(\d+)' } | Select-Object -Last 1) -replace '.*cruces=(\d+).*', '$1')
Result "H3-08" "Cruzar portal_red_0 -> portal_blue_0 conserva la velocidad (P4)" ($in -and $out -and $out.Speed -ge 0.9 * $in.Speed -and [int]$crossings -ge 1) ("{0:N1} su/s al entrar, {1:N1} al salir, cruces={2}" -f $in.Speed, $out.Speed, $crossings)

Remove-Item $cfg -ErrorAction SilentlyContinue
Result "H3-09" "Portal sigue abierto (sin cierres)" ([bool](Get-Process hl2 -ErrorAction SilentlyContinue)) ""

if ($Report) {
    $md = @("| Id | Prueba | Resultado | Detalle |", "|---|---|---|---|")
    foreach ($r in $results) { $md += "| $($r.Id) | $($r.Prueba) | **$($r.Resultado)** | $($r.Detalle) |" }
    [IO.File]::WriteAllText((Join-Path $root $Report), ($md -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
}
$failed = @($results | Where-Object { $_.Resultado -eq "FAIL" }).Count
Write-Host ("`n{0} de {1} pruebas PASS" -f ($results.Count - $failed), $results.Count)
exit $(if ($failed) { 1 } else { 0 })
