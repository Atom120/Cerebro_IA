# Prueba automatica de los portales de la PISTOLA con ar_enable 1 (testchmb_a_09). Autor: Sebas (2026-11-10).
#   powershell -ExecutionPolicy Bypass -File qa\scripts\portalgun_smoke.ps1 -Report qa\resultados\portalgun_auto.md
# Los portales de la pistola se crean despues de cargar el mapa: el plugin tiene que encontrarlos (RescanPortals)
# y ceder el movimiento a Portal al llegar a ellos. Dispara azul (pared oeste x=384) y naranja (pared este x=640)
# con sv_portal_placement_never_fail, corre hacia el azul y comprueba que sale por el naranja con inercia.
# No toques el teclado mientras corre (~1.5 minutos).
param([string]$Report = "")
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$portal = (Get-Content (Join-Path $root "config\paths.local.json") -Raw | ConvertFrom-Json).paths.portal_root
$log = Join-Path $portal "portal\console.log"
$cfg = Join-Path $portal "portal\cfg\ar_qa_tmp.cfg"
$viewTool = Join-Path $PSScriptRoot "read_portal_view.py"

function Send([string[]]$commands) {
    # Sin Portal abierto, -hijack abriria un Portal nuevo y se quedaria esperando: mejor fallar.
    if (-not (Get-Process hl2 -ErrorAction SilentlyContinue)) { throw "Portal se cerro durante la prueba." }
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
# setang a veces llega tarde (la vista del cliente se aplica en otro frame): repite hasta que ar_status la confirme.
function Aim([double]$pitch, [double]$yaw) {
    for ($k = 0; $k -lt 6; $k++) {
        Send @("setang $pitch $yaw 0"); Start-Sleep 0.5
        $mm = Mark; Send @("ar_status"); Start-Sleep 0.4
        $v = LogFrom $mm | Where-Object { $_ -match '^\[AR\] vista=\(([\d.-]+) ([\d.-]+)' } | Select-Object -Last 1
        if ($v -and $v -match '^\[AR\] vista=\(([\d.-]+) ([\d.-]+)') {
            $dy = [math]::Abs(((([double]$Matches[2] - $yaw) % 360) + 540) % 360 - 180)
            if ([math]::Abs([double]$Matches[1] - $pitch) -lt 2 -and $dy -lt 2) { return $true }
        }
    }
    Write-Host "  aviso: setang $pitch $yaw no se aplico"
    return $false
}
Get-Process hl2 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
& (Join-Path $root ".agents\skills\launch-portal-test\scripts\launch-portal-test.ps1") -NoLaunch | Out-Null
Remove-Item $log -ErrorAction SilentlyContinue
Start-Process -FilePath (Join-Path $portal "hl2.exe") -WorkingDirectory $portal -ArgumentList @(
    "-game", "portal", "-novid", "-windowed", "-w", "1280", "-h", "720", "-condebug", "+developer", "1",
    "+sv_cheats", "1", "+map", "testchmb_a_09")
for ($i = 0; $i -lt 120 -and -not ((Test-Path $log) -and (Select-String -Path $log -Pattern "ServerActivate listo" -Quiet)); $i++) { Start-Sleep 1 }
Start-Sleep 12
$initial = (Select-String -Path $log -Pattern 'ServerActivate listo en mapa testchmb_a_09 \((\d+) prop_portal\)' | Select-Object -Last 1).Matches[0].Groups[1].Value
Send @("ar_enable 1", "give weapon_portalgun", "upgrade_portalgun", "sv_portal_placement_never_fail 1"); Start-Sleep 2
# Caida sobre un portal de suelo: Faith cede el movimiento a Portal para que su embudo (sv_player_funnel_into_portals)
# la centre y la meta en el portal. No debe haber caida dura ni Skill Roll de Mirror's Edge.
Send @("setpos 512 110 1"); Start-Sleep 1.0
$null = Aim 0 0
Send @("+attack2"); Start-Sleep 0.5; Send @("-attack2"); Start-Sleep 1.2
# Al suelo a ~76 su por delante (40 grados hacia abajo): si el portal se abre bajo los pies, el jugador cae por el.
Send @("setpos 600 110 1"); Start-Sleep 1.0
$null = Aim 40 180
$floor = 0
for ($try = 0; $try -lt 3 -and $floor -lt 1; $try++) {
    Send @("+attack"); Start-Sleep 0.5; Send @("-attack"); Start-Sleep 1.2
    $m = Mark; Send @("ar_status"); Start-Sleep 0.6
    $st = LogFrom $m
    $floor = @($st | Where-Object { $_ -match '^\[AR\] portal \d+: abierto=1 .* ang=\(-(89|90)' }).Count
    if ($floor -lt 1) { Write-Host "  reintento disparo al suelo"; Send @("setpos 600 110 1"); Start-Sleep 0.8; $null = Aim 40 180 }
}
$fp = $st | Where-Object { $_ -match '^\[AR\] portal \d+: abierto=1 pos=\(([\d.-]+) ([\d.-]+) ([\d.-]+)\) ang=\(-(89|90)' } | Select-Object -First 1
$fx = 524.0; $fy = 110.0
if ($fp -and $fp -match 'pos=\(([\d.-]+) ([\d.-]+)') { $fx = [double]$Matches[1]; $fy = [double]$Matches[2] }
# El embudo de Portal solo actua si se mira al portal: cae a 30 su del centro (sobre el borde) mirando abajo.
# Sin embudo se queda de pie en el borde; con el, Portal la mete y sale por la pared este.
$dropX = $fx; $dropY = $fy + 30
$m = Mark; Send @("ar_debug 1", "setpos $dropX $dropY 300", "setang 80 -90 0"); Start-Sleep 0.3
Send @("setang 80 -90 0"); Start-Sleep 2.7; Send @("ar_debug 0", "ar_status", "getpos"); Start-Sleep 0.8
$lines = LogFrom $m
$g = $lines | Where-Object { $_ -match '^setpos ([\d.-]+) ([\d.-]+)' } | Select-Object -Last 1
$moved = 0.0
if ($g -and $g -match '^setpos ([\d.-]+) ([\d.-]+)') { $moved = [math]::Sqrt([math]::Pow([double]$Matches[1] - $dropX, 2) + [math]::Pow([double]$Matches[2] - $dropY, 2)) }
$hard = @($lines | Where-Object { $_ -match '^\[AR\] t=.* eventos=0x([0-9a-f]+)' -and (([Convert]::ToInt32($Matches[1], 16) -band 0xC) -ne 0) }).Count
Result "PG-05" "Caer sobre el borde de un portal de suelo: el embudo de Portal la mete dentro" ($floor -ge 1 -and $moved -gt 40) ("portal de suelo=$floor; acaba a {0:N0} su del punto de caida (sin embudo: 0)" -f $moved)
Result "PG-06" "Sin caida dura ni Skill Roll de Mirror's Edge en esa caida" ($hard -eq 0) ("eventos de caida dura/roll: $hard")
$tf0 = [int64](($lines | Where-Object { $_ -match 'ticks_faith=(\d+)' } | Select-Object -Last 1) -replace '.*ticks_faith=(\d+).*', '$1')
Start-Sleep 1.0
$m = Mark; Send @("ar_status"); Start-Sleep 0.6
$tf1 = [int64]((LogFrom $m | Where-Object { $_ -match 'ticks_faith=(\d+)' } | Select-Object -Last 1) -replace '.*ticks_faith=(\d+).*', '$1')
Result "PG-07" "Despues de la caida Faith retoma el control" ($tf1 -gt $tf0 + 30) ("ticks_faith $tf0 -> $tf1")
# Cruces corriendo: azul a la pared oeste (x=384), el naranja sigue en la pared este (x=640).
Send @("setpos 512 110 1"); Start-Sleep 1.0
$null = Aim 0 180
Send @("+attack"); Start-Sleep 0.5; Send @("-attack"); Start-Sleep 1.0
$null = Aim 0 0
Send @("+attack2"); Start-Sleep 0.5; Send @("-attack2"); Start-Sleep 1.2
$m = Mark; Send @("ar_status"); Start-Sleep 0.6
$open = @(LogFrom $m | Where-Object { $_ -match '^\[AR\] portal \d+: abierto=1' }).Count
Result "PG-01" "El plugin encuentra los portales de la pistola (no estaban al cargar el mapa)" ($open -ge 2) ("al cargar: $initial prop_portal; abiertos ahora: $open")
Send @("setpos 560 110 1"); Start-Sleep 1.0
$null = Aim 0 180
$m = Mark; Send @("ar_debug 1", "ar_qa_input 1 0 - 200"); Start-Sleep 3.5; Send @("ar_debug 0", "ar_status"); Start-Sleep 0.8
$lines = LogFrom $m
$crossings = [int]((($lines | Where-Object { $_ -match 'cruces=(\d+)' } | Select-Object -Last 1) -replace '.*cruces=(\d+).*', '$1'))
$speeds = @($lines | Where-Object { $_ -match '^\[AR\] t=[\d.]+ anim=sprint vel2d=([\d.]+)' } | ForEach-Object { [double]($_ -replace '.*vel2d=([\d.]+).*', '$1') })
$kept = $speeds.Count -ge 2 -and (($speeds | Measure-Object -Maximum).Maximum -gt $speeds[0] + 15)
Result "PG-02" "Cruzar los portales de la pistola corriendo (azul -> naranja)" ($crossings -ge 1) ("cruces=$crossings")
Result "PG-03" "Se conserva la inercia al cruzar (la velocidad sigue subiendo tras cruzar)" $kept ("sprint: " + (($speeds | ForEach-Object { "{0:N0}" -f $_ }) -join " -> ") + " su/s")
$alive = [bool](Get-Process hl2 -ErrorAction SilentlyContinue)
Result "PG-04" "Portal sigue abierto (sin cierres)" $alive ""
Get-Process hl2 -ErrorAction SilentlyContinue | Stop-Process -Force
$pass = @($results | Where-Object { $_.Resultado -eq "PASS" }).Count
"`n$pass de $($results.Count) pruebas PASS"
if ($Report) {
    $md = @("| Id | Prueba | Resultado | Detalle |", "|---|---|---|---|") + ($results | ForEach-Object { "| $($_.Id) | $($_.Prueba) | **$($_.Resultado)** | $($_.Detalle) |" })
    [IO.File]::WriteAllText((Join-Path $root $Report), ($md -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
}
if ($pass -ne $results.Count) { exit 1 }
