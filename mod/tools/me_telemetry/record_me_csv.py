#!/usr/bin/env python3
"""
record_me_csv.py — Grabador y validador de solo lectura de telemetría de Mirror's Edge (B2 / B2b).
Dueño: Benja (mod/tools/me_telemetry/).

Reglas:
- Estrictamente SOLO LECTURA (PROCESS_VM_READ = 0x0010, ReadProcessMemory). Jamás escribe en memoria.
- Lee rutas desde config/paths.local.json si existe (sin rutas personales fijadas).
- Formato CSV de salida: t,x,y,z,vx,vy,vz,movement_state,walking_state

Uso:
  python mod/tools/me_telemetry/record_me_csv.py --generate-reference
  python mod/tools/me_telemetry/record_me_csv.py --verify qa/telemetria
  python mod/tools/me_telemetry/record_me_csv.py --probe
  python mod/tools/me_telemetry/record_me_csv.py --record qa/telemetria/live.csv --duration 8 --hz 120
  python mod/tools/me_telemetry/record_me_csv.py --record-clip sprint_start
  python mod/tools/me_telemetry/record_me_csv.py --suite
  python mod/tools/me_telemetry/record_me_csv.py --compare
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_TELEMETRY_DIR = REPO_ROOT / "qa" / "telemetria"
RECORDER_EXE = REPO_ROOT / "build" / "x86" / "tools" / "me_telemetry" / "Release" / "me_telemetry_recorder.exe"

# Offsets verificados en Mirror's Edge GOG v1.01 (Timestamp PE 0x4965FFF1, ImageBase 0x00400000)
ME_IMAGE_BASE = 0x00400000
ME_PE_TIMESTAMP = 0x4965FFF1
ME_GENGINE_VA = 0x01FF8B20
OFFSET_GAMEPLAYERS_DATA = 0x02BC
OFFSET_LOCALPLAYER_ACTOR = 0x0040
OFFSET_CONTROLLER_PAWN = 0x01C0
OFFSET_ACTOR_LOCATION = 0x00E8
OFFSET_ACTOR_VELOCITY = 0x0100
OFFSET_TDPAWN_MOVEMENT_STATE = 0x04FE
OFFSET_TDPAWN_WALKING_STATE = 0x0503

UU_TO_SU = 1.0 / 2.54
CSV_COLUMNS = ["t", "x", "y", "z", "vx", "vy", "vz", "movement_state", "walking_state"]

# 9 clips de referencia exigidos por el contrato B2 / B2b
CLIPS_METADATA = [
    {
        "id": "sprint_start",
        "ref": "sprint_start.csv",
        "live": "sprint_start_live.csv",
        "desc": "Arrancar a correr desde reposo hasta sprint máximo (~720 uu/s)",
        "duration": 3.5,
        "primary_state": 1,  # MOVE_Walking
        "primary_walk": 5,   # WAS_Sprint
    },
    {
        "id": "jump",
        "ref": "jump.csv",
        "live": "jump_live.csv",
        "desc": "Salto horizontal corriendo en suelo plano (~248 uu/s vertical)",
        "duration": 2.0,
        "primary_state": 11, # MOVE_Jump
        "primary_walk": 0,
    },
    {
        "id": "wallrun_left",
        "ref": "wallrun_left.csv",
        "live": "wallrun_left_live.csv",
        "desc": "Wallrun sobre pared a tu izquierda",
        "duration": 2.5,
        "primary_state": 5,  # MOVE_WallRunningLeft
        "primary_walk": 0,
    },
    {
        "id": "wallrun_right",
        "ref": "wallrun_right.csv",
        "live": "wallrun_right_live.csv",
        "desc": "Wallrun sobre pared a tu derecha",
        "duration": 2.5,
        "primary_state": 4,  # MOVE_WallRunningRight
        "primary_walk": 0,
    },
    {
        "id": "wallclimb",
        "ref": "wallclimb.csv",
        "live": "wallclimb_live.csv",
        "desc": "Escalar pared frontal (wallclimb)",
        "duration": 2.5,
        "primary_state": 6,  # MOVE_WallClimbing
        "primary_walk": 0,
    },
    {
        "id": "ledge_grab",
        "ref": "ledge_grab.csv",
        "live": "ledge_grab_live.csv",
        "desc": "Saltar y agarrar borde (ledge hang y subir)",
        "duration": 2.5,
        "primary_state": 3,  # MOVE_Grabbing
        "primary_walk": 0,
    },
    {
        "id": "slide",
        "ref": "slide.csv",
        "live": "slide_live.csv",
        "desc": "Deslizarse (slide agachándose en carrera)",
        "duration": 2.2,
        "primary_state": 16, # MOVE_Slide
        "primary_walk": 0,
    },
    {
        "id": "hard_landing",
        "ref": "hard_landing.csv",
        "live": "hard_landing_live.csv",
        "desc": "Caída dura desde altura sin amortiguar",
        "duration": 2.5,
        "primary_state": 20, # MOVE_Landing
        "primary_walk": 0,
    },
    {
        "id": "skill_roll",
        "ref": "skill_roll.csv",
        "live": "skill_roll_live.csv",
        "desc": "Caída rodando con amortiguación (Skill Roll al caer)",
        "duration": 2.5,
        "primary_state": 91, # MOVE_SkillRoll
        "primary_walk": 0,
    },
]

EXPECTED_CLIPS = [item["ref"] for item in CLIPS_METADATA]


def load_local_paths() -> dict:
    cfg = REPO_ROOT / "config" / "paths.local.json"
    if cfg.is_file():
        with cfg.open("r", encoding="utf-8") as f:
            return json.load(f).get("paths", {})
    return {}


def run_cpp_tool(args: list[str]) -> int:
    if not RECORDER_EXE.is_file():
        print(f"[ERROR] No se encontró {RECORDER_EXE}. Compila primero con mod\\build.bat.", file=sys.stderr)
        return 1
    proc = subprocess.run([str(RECORDER_EXE), *args], cwd=str(REPO_ROOT))
    return proc.returncode


def verify_csv_directory(target_dir: Path) -> int:
    target_dir = target_dir.resolve()
    print(f"=== Verificando CSVs de telemetría en {target_dir.relative_to(REPO_ROOT)} ===")
    print(f"{'Archivo':<24} {'Filas':>6} {'Dur(s)':>7} {'MaxVel(uu/s)':>13} {'MaxVel(su/s)':>13} {'MovementStates'}")
    print("-" * 92)

    missing = 0
    all_clips = EXPECTED_CLIPS + [item["live"] for item in CLIPS_METADATA]
    # Check present files
    files_to_check = [f for f in all_clips if (target_dir / f).is_file()]
    if not files_to_check:
        print("  [AVISO] No se encontraron archivos de telemetría.")
        return 1

    for name in sorted(set(files_to_check)):
        p = target_dir / name
        with p.open("r", encoding="utf-8", newline="") as f:
            reader = csv.DictReader(f)
            if reader.fieldnames != CSV_COLUMNS:
                print(f"{name:<24} [HEADER INVALIDO: {reader.fieldnames}]")
                missing += 1
                continue
            rows = list(reader)
        if not rows:
            print(f"{name:<24} [VACIO]")
            missing += 1
            continue
        dur = float(rows[-1]["t"]) - float(rows[0]["t"])
        max_v_uu = max(
            math.hypot(float(r["vx"]), float(r["vy"]), float(r["vz"])) for r in rows
        )
        max_v_su = max_v_uu * UU_TO_SU
        states = sorted({int(r["movement_state"]) for r in rows})
        kind = "(LIVE)" if "_live" in name else "(SINTETICO)"
        print(f"{name:<24} {len(rows):>6d} {dur:>7.2f} {max_v_uu:>13.1f} {max_v_su:>13.1f} {states} {kind}")

    return 0 if missing == 0 else 1


def analyze_csv(p: Path) -> dict:
    if not p.is_file():
        return {}
    with p.open("r", encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return {}
    dur = float(rows[-1]["t"]) - float(rows[0]["t"])
    max_v_uu = max(math.hypot(float(r["vx"]), float(r["vy"]), float(r["vz"])) for r in rows)
    max_v_2d = max(math.hypot(float(r["vx"]), float(r["vy"])) for r in rows)
    max_vz = max(float(r["vz"]) for r in rows)
    min_vz = min(float(r["vz"]) for r in rows)
    states = sorted({int(r["movement_state"]) for r in rows})
    return {
        "rows": len(rows),
        "dur": dur,
        "max_v_uu": max_v_uu,
        "max_v_su": max_v_uu * UU_TO_SU,
        "max_v_2d_uu": max_v_2d,
        "max_v_2d_su": max_v_2d * UU_TO_SU,
        "max_vz": max_vz,
        "min_vz": min_vz,
        "states": states,
    }


def compare_reference_and_live(target_dir: Path) -> int:
    target_dir = target_dir.resolve()
    print("=== Comparación Telemetría: Sintética (Referencia) vs Real en Vivo (B2b) ===")
    print(f"{'Movimiento':<16} {'Tipo':<10} {'Filas':>6} {'Dur(s)':>7} {'Max2D(su)':>10} {'Vz Max':>8} {'Estados'}")
    print("-" * 88)

    pairs_found = 0
    for item in CLIPS_METADATA:
        p_ref = target_dir / item["ref"]
        p_live = target_dir / item["live"]

        d_ref = analyze_csv(p_ref)
        d_live = analyze_csv(p_live)

        name = item["id"]
        if d_ref:
            print(f"{name:<16} {'REF':<10} {d_ref['rows']:>6d} {d_ref['dur']:>7.2f} {d_ref['max_v_2d_su']:>10.1f} {d_ref['max_vz']:>8.1f} {d_ref['states']}")
        else:
            print(f"{name:<16} {'REF [FALTA]':<10}")

        if d_live:
            pairs_found += 1
            print(f"{'':<16} {'LIVE (B2b)':<10} {d_live['rows']:>6d} {d_live['dur']:>7.2f} {d_live['max_v_2d_su']:>10.1f} {d_live['max_vz']:>8.1f} {d_live['states']}")
        else:
            print(f"{'':<16} {'LIVE [PEND]':<10} (grabar con --record-clip {name})")
        print("." * 88)

    print(f"\nTrazas en vivo capturadas: {pairs_found}/{len(CLIPS_METADATA)}")
    return 0


def record_interactive_suite(target_dir: Path, hz: int) -> int:
    target_dir = target_dir.resolve()
    target_dir.mkdir(parents=True, exist_ok=True)

    print("=================================================================")
    print("   ASISTENTE DE GRABACIÓN EN VIVO DE TELEMETRÍA (B2b) — MIRROR'S EDGE")
    print("=================================================================")
    print("Requisitos:")
    print("1. Mirror's Edge (GOG v1.01) debe estar ABIERTO con una partida cargada.")
    print("   (Ej. Azotea de inicio en Edge_p o Prólogo).")
    print("2. Faith debe estar viva y controlable en pantalla.")
    print("-----------------------------------------------------------------")

    # Probar conexión
    print("[1/2] Probando conexión de memoria de solo lectura con MirrorsEdge.exe...")
    probe_rc = run_cpp_tool(["--probe"])
    if probe_rc != 0:
        print("\n[ERROR] No se pudo leer la memoria de Faith en MirrorsEdge.exe.")
        print("Asegúrate de:")
        print(" - Iniciar Mirror's Edge.")
        print(" - Cargar un nivel (ej. presiona F7 para 'start Edge_p' o juega el prólogo).")
        print(" - Vuelve a ejecutar este comando.")
        return probe_rc

    print("\n[2/2] ¡Conexión establecida exitosamente con el proceso de Faith!")
    print(f"Se grabarán {len(CLIPS_METADATA)} movimientos a {hz} Hz en {target_dir.relative_to(REPO_ROOT)}/.\n")

    for idx, item in enumerate(CLIPS_METADATA, start=1):
        out_file = target_dir / item["live"]
        print(f"\n>>> [{idx}/{len(CLIPS_METADATA)}] {item['id'].upper()} ({item['live']})")
        print(f"    Descripción : {item['desc']}")
        print(f"    Duración    : {item['duration']} segundos")

        resp = input("    ¿Listo para grabar? Presiona ENTER para iniciar cuenta atrás (o 's' para saltar): ").strip().lower()
        if resp == 's':
            print("    [SALTADO]")
            continue

        for cd in [3, 2, 1]:
            print(f"    Iniciando en {cd}...", end="\r", flush=True)
            time.sleep(1.0)
        print("    *** ¡GRABANDO! REALIZA EL MOVIMIENTO AHORA EN MIRROR'S EDGE ***")

        rc = run_cpp_tool([
            "--record", str(out_file),
            "--duration", str(item["duration"]),
            "--hz", str(hz),
        ])

        if rc != 0 or not out_file.is_file():
            print(f"    [FALLO] No se pudo completar la grabación de {item['live']}.")
            continue

        stat = analyze_csv(out_file)
        print(f"    [OK] Guardado {item['live']} ({stat['rows']} muestras, {stat['dur']:.2f}s).")
        print(f"         Velocidad máxima: {stat['max_v_uu']:.1f} uu/s ({stat['max_v_su']:.1f} su/s).")
        print(f"         Estados detectados: {stat['states']}")

    print("\n=================================================================")
    print("Suite de grabación finalizada.")
    compare_reference_and_live(target_dir)
    return 0


def record_single_clip(clip_id: str, target_dir: Path, hz: int) -> int:
    target_dir = target_dir.resolve()
    target_dir.mkdir(parents=True, exist_ok=True)
    meta = next((c for c in CLIPS_METADATA if c["id"] == clip_id or c["ref"] == clip_id or c["live"] == clip_id), None)
    if not meta:
        print(f"[ERROR] Movimiento '{clip_id}' no reconocido.")
        print("Opciones válidas:", ", ".join(c["id"] for c in CLIPS_METADATA))
        return 1

    out_file = target_dir / meta["live"]
    print(f"=== Grabando clip en vivo: {meta['id']} -> {out_file.name} ===")
    print(f"Descripción : {meta['desc']}")
    print(f"Duración    : {meta['duration']} s @ {hz} Hz")

    for cd in [3, 2, 1]:
        print(f"Iniciando en {cd}...", end="\r", flush=True)
        time.sleep(1.0)
    print("¡GRABANDO AHORA!")

    rc = run_cpp_tool([
        "--record", str(out_file),
        "--duration", str(meta["duration"]),
        "--hz", str(hz),
    ])

    if rc == 0 and out_file.is_file():
        stat = analyze_csv(out_file)
        print(f"[OK] Guardado: {out_file.name} ({stat['rows']} muestras, {stat['dur']:.2f}s).")
        print(f"     Velocidad máx: {stat['max_v_uu']:.1f} uu/s ({stat['max_v_su']:.1f} su/s).")
        print(f"     Estados: {stat['states']}")
    return rc


def main() -> int:
    parser = argparse.ArgumentParser(description="Grabador y validador de solo lectura de telemetría de Mirror's Edge (B2 / B2b)")
    parser.add_argument("--probe", action="store_true", help="Verifica offsets en vivo (solo lectura) en MirrorsEdge.exe")
    parser.add_argument("--record", type=str, default="", help="Ruta del CSV a grabar en vivo desde MirrorsEdge.exe")
    parser.add_argument("--record-clip", type=str, default="", help="Graba uno de los 9 movimientos de B2b (ej. sprint_start, jump, wallrun_left)")
    parser.add_argument("--suite", action="store_true", help="Asistente interactivo paso a paso para grabar los 9 clips de B2b")
    parser.add_argument("--duration", type=float, default=5.0, help="Duración de grabación en segundos (defecto: 5.0)")
    parser.add_argument("--hz", type=int, default=120, help="Frecuencia de muestreo 60..120 Hz (defecto: 120)")
    parser.add_argument("--generate-reference", action="store_true", help="Genera las 9 trazas CSV base en qa/telemetria/")
    parser.add_argument("--verify", type=str, nargs="?", const=str(DEFAULT_TELEMETRY_DIR), help="Verifica los CSV de qa/telemetria/")
    parser.add_argument("--compare", action="store_true", help="Compara trazas de referencia sintéticas con trazas reales en vivo")
    args = parser.parse_args()

    if args.probe:
        return run_cpp_tool(["--probe"])

    if args.generate_reference:
        DEFAULT_TELEMETRY_DIR.mkdir(parents=True, exist_ok=True)
        rc = run_cpp_tool(["--generate-reference", str(DEFAULT_TELEMETRY_DIR), "--hz", str(args.hz)])
        if rc != 0:
            return rc
        return verify_csv_directory(DEFAULT_TELEMETRY_DIR)

    if args.suite:
        return record_interactive_suite(DEFAULT_TELEMETRY_DIR, args.hz)

    if args.record_clip:
        return record_single_clip(args.record_clip, DEFAULT_TELEMETRY_DIR, args.hz)

    if args.record:
        out_path = Path(args.record)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        return run_cpp_tool([
            "--record", str(out_path),
            "--duration", str(args.duration),
            "--hz", str(args.hz),
        ])

    if args.verify:
        return verify_csv_directory(Path(args.verify))

    if args.compare:
        return compare_reference_and_live(DEFAULT_TELEMETRY_DIR)

    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
