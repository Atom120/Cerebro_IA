# Prueba automatica de H2 ("Corre como Faith") en Portal 19017868 / testchmb_a_00 (B3).
# Autor: Sebas (2026-11-04). Abre Portal con el mod, mueve al jugador con comandos de consola y decide
# PASS/FAIL leyendo portal/console.log. No hace falta tocar el teclado: la entrada se inyecta con
# `ar_qa_input` (Source suelta las teclas +forward si la ventana de Portal pierde el foco).
#
#   powershell -ExecutionPolicy Bypass -File qa\scripts\h2_smoke.ps1            # compila antes con mod\build.bat
#   powershell -ExecutionPolicy Bypass -File qa\scripts\h2_smoke.ps1 -Report qa\resultados\H2_auto.md
#
# Requisitos: config\paths.local.json con portal_root, Steam abierto, y la DLL compilada.
param([string]$Report = "")
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$portal = (Get-Content (Join-Path $root "config\paths.local.json") -Raw | ConvertFrom-Json).paths.portal_root
$log = Join-Path $portal "portal\console.log"
$cfg = Join-Path $portal "portal\cfg\ar_qa_tmp.cfg"

function Send([string[]]$commands) {
    # Sin Portal abierto, -hijack abriria un Portal nuevo y se quedaria esperando: mejor fallar.
    if (-not (Get-Process hl2 -ErrorAction SilentlyContinue)) { throw "Portal se cerro durante la prueba." }
    [IO.File]::WriteAllText($cfg, ($commands -join "`n") + "`n")
    Start-Process -FilePath (Join-Path $portal "hl2.exe") -WorkingDirectory $portal -Wait `
        -ArgumentList @("-hijack", "-game", "portal", "+exec", "ar_qa_tmp")
    Start-Sleep -Milliseconds 300
}
function LogFrom([int]$line) { Get-Content $log | Select-Object -Skip $line }
function Mark { (Get-Content $log).Count }
# [AR] t=.. anim=X vel2d=V vz=.. suelo=.. pos=(x y z)
function DebugSamples($lines) {
    foreach ($l in $lines) {
        if ($l -match '^\[AR\] t=[\d.]+ anim=(\S+) vel2d=([\d.-]+) vz=([\d.-]+) suelo=(\d) pos=\(([\d.-]+) ([\d.-]+) ([\d.-]+)\)') {
            [pscustomobject]@{ Anim = $Matches[1]; Speed = [double]$Matches[2]; Z = [double]$Matches[7]; X = [double]$Matches[5] }
        }
    }
}
function LastStatus($lines) {
    $s = $lines | Where-Object { $_ -match '^\[AR\] ar_enable=' } | Select-Object -Last 1
    if ($s -match 'motor=\(([\d.-]+) ([\d.-]+) ([\d.-]+)\) ticks_faith=(\d+) ticks_portal=(\d+)') {
        return [pscustomobject]@{ X = [double]$Matches[1]; Y = [double]$Matches[2]; Z = [double]$Matches[3]; Faith = [int64]$Matches[4]; Portal = [int64]$Matches[5] }
    }
}
$results = New-Object System.Collections.Generic.List[object]
function Result($id, $name, $pass, $detail) {
    $results.Add([pscustomobject]@{ Id = $id; Prueba = $name; Resultado = $(if ($pass) { "PASS" } else { "FAIL" }); Detalle = $detail })
    Write-Host ("{0,-6} {1,-4} {2} - {3}" -f $id, $(if ($pass) { "PASS" } else { "FAIL" }), $name, $detail)
}

# --- arranque -------------------------------------------------------------------------------
Get-Process hl2 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
& (Join-Path $root ".agents\skills\launch-portal-test\scripts\launch-portal-test.ps1") -NoLaunch | Out-Null
Remove-Item $log -ErrorAction SilentlyContinue
Start-Process -FilePath (Join-Path $portal "hl2.exe") -WorkingDirectory $portal -ArgumentList @(
    "-game", "portal", "-novid", "-windowed", "-w", "1280", "-h", "720", "-condebug", "+developer", "1",
    "+sv_cheats", "1", "+map", "testchmb_a_00")
for ($i = 0; $i -lt 120 -and -not ((Test-Path $log) -and (Select-String -Path $log -Pattern "Autotest Z4" -Quiet)); $i++) { Start-Sleep 1 }
Start-Sleep 20  # el despertar del principio del mapa congela al jugador

$auto = Select-String -Path $log -Pattern "Autotest Z4.*" | Select-Object -Last 1
Result "H2-01" "Plugin cargado y trazas reales (autotest Z4)" ($auto -and $auto.Line -match ': OK') ($auto.Line -replace '^.*Autotest Z4 \([^)]*\): ', '')

# --- ar_enable --------------------------------------------------------------------------------
$m = Mark; Send @("ar_enable 1"); Start-Sleep 1
Result "H2-02" "ar_enable 1 activa el movimiento de Faith" ((LogFrom $m) -match 'ar_enable = 1') ""

# --- sprint con inercia ------------------------------------------------------------------------
Send @("setpos -660 -864 140", "setang 0 180 0"); Start-Sleep 1.5
$m = Mark; Send @("ar_debug 1", "ar_qa_input 1 0 - 160"); Start-Sleep 2.8; Send @("ar_debug 0"); Start-Sleep 0.5
$s = @(DebugSamples (LogFrom $m))
$max = ($s | Measure-Object Speed -Maximum).Maximum
# Inercia: entre dos muestras seguidas en sprint (cada 33 ticks = 0.495 s) la velocidad sube ~84 su/s.
$slopes = @()
for ($k = 1; $k -lt $s.Count; $k++) {
    if ($s[$k].Anim -eq "sprint" -and $s[$k - 1].Anim -eq "sprint" -and $s[$k].Speed -gt $s[$k - 1].Speed) {
        $slopes += ($s[$k].Speed - $s[$k - 1].Speed) / 0.495
    }
}
$slope = if ($slopes.Count) { ($slopes | Measure-Object -Average).Average } else { 0 }
Result "H2-03" "Sprint con inercia (157.5 -> 283.5 su/s, rampa de ~84 su/s^2)" ($max -gt 157.5 -and $max -le 284 -and $slope -ge 70 -and $slope -le 100) ("velocidad maxima {0:N1} su/s, rampa {1:N1} su/s^2" -f $max, $slope)

# --- salto --------------------------------------------------------------------------------------
Send @("setpos -700 -864 140", "setang 0 180 0"); Start-Sleep 1.5
$m = Mark; Send @("ar_debug 2", "ar_qa_input 0 0 j 3"); Start-Sleep 1.5; Send @("ar_debug 0")
$lines = LogFrom $m
$s = DebugSamples $lines
$apex = ($s | Measure-Object Z -Maximum).Maximum - 128.0
$jumped = ($lines -match 'anim=jump vel2d=[\d.]+ vz=248').Count -gt 0
Result "H2-04" "Salto de Faith (248 su/s, ~47.6 su)" ($jumped -and $apex -ge 40 -and $apex -le 56) ("altura {0:N1} su" -f $apex)

# --- agacharse ----------------------------------------------------------------------------------
$m = Mark; Send @("ar_debug 1", "ar_qa_input 0 0 d 45"); Start-Sleep 1.0; Send @("ar_debug 0"); Start-Sleep 0.5
Result "H2-05" "Agacharse (anim crouch)" (((LogFrom $m) -match 'anim=crouch').Count -gt 0) ""

# --- portal ---------------------------------------------------------------------------------------
Send @("ent_fire relay_portal_cancel_room1 trigger"); Start-Sleep 2
Send @("setpos -591 -300 162", "setang 0 270 0"); Start-Sleep 1.2
$m = Mark; Send @("ar_qa_input 1 0 - 100"); Start-Sleep 2.0; Send @("ar_status"); Start-Sleep 0.5
$st = LastStatus (LogFrom $m)
Result "H2-06" "Cruce de portal_red_0 a portal_blue_0 (CHK-04) con ar_enable 1" ($st -and $st.X -lt -640 -and [math]::Abs($st.Y + 320) -lt 40) ("motor=({0:N0} {1:N0} {2:N0}), ticks de Portal={3}" -f $st.X, $st.Y, $st.Z, $st.Portal)

# --- ascensor intermedio -------------------------------------------------------------------------
Send @("setpos -1500 -864 170", "setang 0 0 0"); Start-Sleep 14
$m = Mark; Send @("ar_status"); Start-Sleep 0.5
$st = LastStatus (LogFrom $m)
Result "H2-07" "Subir en el ascensor intermedio (CHK-08) con ar_enable 1" ($st -and $st.Z -gt 600) ("motor z={0:N0} (sube de ~214 a ~792)" -f $st.Z)

# Ojo: ar_debug 2 escribe cada tick (66 lineas/s); solo para pruebas cortas.
# --- ar_enable 0 vuelve a Portal ------------------------------------------------------------------
$m = Mark; Send @("ar_status", "ar_enable 0", "wait 200", "ar_status"); Start-Sleep 3
$st = @((LogFrom $m) | Where-Object { $_ -match '^\[AR\] ar_enable=' } | ForEach-Object { if ($_ -match 'ticks_faith=(\d+)') { [int64]$Matches[1] } })
$off = (LogFrom $m) -match 'ar_enable = 0'
Result "H2-08" "ar_enable 0 devuelve el movimiento original" ($off -and $st.Count -ge 2 -and ($st[-1] - $st[0]) -le 2) ("ticks de Faith {0} -> {1} tras desactivar" -f $st[0], $st[-1])

Remove-Item $cfg -ErrorAction SilentlyContinue
$crash = -not (Get-Process hl2 -ErrorAction SilentlyContinue)
Result "H2-09" "Portal sigue abierto al terminar (sin cierres)" (-not $crash) ""

if ($Report) {
    $md = @("| Id | Prueba | Resultado | Detalle |", "|---|---|---|---|")
    foreach ($r in $results) { $md += "| $($r.Id) | $($r.Prueba) | **$($r.Resultado)** | $($r.Detalle) |" }
    [IO.File]::WriteAllText((Join-Path $root $Report), ($md -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
}
$failed = @($results | Where-Object { $_.Resultado -eq "FAIL" }).Count
Write-Host ("`n{0} de {1} pruebas PASS" -f ($results.Count - $failed), $results.Count)
exit $(if ($failed) { 1 } else { 0 })
