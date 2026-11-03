#!/usr/bin/env python3
"""
record_me_csv.py — Grabador y validador de solo lectura de telemetría de Mirror's Edge (B2).
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
"""

from __future__ import annotations

import argparse
import csv
import ctypes
import ctypes.wintypes as wt
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

EXPECTED_CLIPS = [
    "sprint_start.csv",
    "jump.csv",
    "wallrun_left.csv",
    "wallrun_right.csv",
    "wallclimb.csv",
    "ledge_grab.csv",
    "slide.csv",
    "hard_landing.csv",
    "skill_roll.csv",
]


def load_local_paths() -> dict:
    cfg = REPO_ROOT / "config" / "paths.local.json"
    if cfg.is_file():
        with cfg.open("r", encoding="utf-8") as f:
            return json.load(f).get("paths", {})
    return {}


def verify_csv_directory(target_dir: Path) -> int:
    print(f"=== Verificando CSVs de telemetría en {target_dir.relative_to(REPO_ROOT)} ===")
    print(f"{'Archivo':<20} {'Filas':>6} {'Dur(s)':>7} {'MaxVel(uu/s)':>13} {'MaxVel(su/s)':>13} {'MovementStates'}")
    print("-" * 86)

    missing = 0
    for name in EXPECTED_CLIPS:
        p = target_dir / name
        if not p.is_file():
            print(f"{name:<20} [FALTA]")
            missing += 1
            continue
        with p.open("r", encoding="utf-8", newline="") as f:
            reader = csv.DictReader(f)
            if reader.fieldnames != CSV_COLUMNS:
                print(f"{name:<20} [HEADER INVALIDO: {reader.fieldnames}]")
                missing += 1
                continue
            rows = list(reader)
        if not rows:
            print(f"{name:<20} [VACIO]")
            missing += 1
            continue
        dur = float(rows[-1]["t"]) - float(rows[0]["t"])
        max_v_uu = max(
            math.hypot(float(r["vx"]), float(r["vy"]), float(r["vz"])) for r in rows
        )
        max_v_su = max_v_uu * UU_TO_SU
        states = sorted({int(r["movement_state"]) for r in rows})
        print(f"{name:<20} {len(rows):>6d} {dur:>7.2f} {max_v_uu:>13.1f} {max_v_su:>13.1f} {states}")

    return 0 if missing == 0 else 1


def run_cpp_tool(args: list[str]) -> int:
    if not RECORDER_EXE.is_file():
        print(f"[ERROR] No se encontró {RECORDER_EXE}. Compila primero con mod\\build.bat.", file=sys.stderr)
        return 1
    proc = subprocess.run([str(RECORDER_EXE), *args], cwd=str(REPO_ROOT))
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Grabador de solo lectura de telemetría de Mirror's Edge (B2)")
    parser.add_argument("--probe", action="store_true", help="Verifica offsets en vivo (solo lectura) en MirrorsEdge.exe")
    parser.add_argument("--record", type=str, default="", help="Ruta del CSV a grabar en vivo desde MirrorsEdge.exe")
    parser.add_argument("--duration", type=float, default=5.0, help="Duración de grabación en segundos (defecto: 5.0)")
    parser.add_argument("--hz", type=int, default=120, help="Frecuencia de muestreo 60..120 Hz (defecto: 120)")
    parser.add_argument("--generate-reference", action="store_true", help="Genera las 9 trazas CSV base en qa/telemetria/")
    parser.add_argument("--verify", type=str, nargs="?", const=str(DEFAULT_TELEMETRY_DIR), help="Verifica los CSV de qa/telemetria/")
    args = parser.parse_args()

    if args.probe:
        return run_cpp_tool(["--probe"])

    if args.generate_reference:
        DEFAULT_TELEMETRY_DIR.mkdir(parents=True, exist_ok=True)
        rc = run_cpp_tool(["--generate-reference", str(DEFAULT_TELEMETRY_DIR), "--hz", str(args.hz)])
        if rc != 0:
            return rc
        return verify_csv_directory(DEFAULT_TELEMETRY_DIR)

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

    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
