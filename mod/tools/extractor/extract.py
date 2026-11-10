"""Extract Faith from the user's own copy of Mirror's Edge and build her models for Portal (A1-A4).

    python mod/tools/extractor/extract.py              # everything (A4): A1 -> A2 -> A3 -> verify
    python mod/tools/extractor/extract.py --step a1    # only export with UE Viewer + write the bone/anim list
    python mod/tools/extractor/extract.py --step a2    # only PSK/PSA -> SMD
    python mod/tools/extractor/extract.py --step a3    # only .qc + studiomdl
    python mod/tools/extractor/extract.py --check      # only check paths and tools, change nothing

Reads config/paths.local.json. Work files go to %TEMP%/aperture_runner_extract (never into the repo); the
compiled models go to <Portal>/portal/custom/aperture_runner/. Contract: docs/contratos/asset_manifest.json.
Owner: Antonio. Standard library only (plus umodel.exe and Portal's studiomdl.exe).
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import smd  # noqa: E402
import ue_psk  # noqa: E402

REPO = HERE.parents[2]
MANIFEST = json.loads((REPO / "docs" / "contratos" / "asset_manifest.json").read_text(encoding="utf-8"))
ANIM_MAP = json.loads((HERE / "anim_map.json").read_text(encoding="utf-8"))
WORK = Path(tempfile.gettempdir()) / "aperture_runner_extract"
GOG_101_TIMESTAMP = 0x4965FFF1
MODEL_DIR = "aperture_runner"  # models/<MODEL_DIR>/faith_*.mdl, as in the manifest


class ExtractError(RuntimeError):
    pass


def load_paths() -> dict[str, Path]:
    cfg = REPO / "config" / "paths.local.json"
    if not cfg.exists():
        raise ExtractError("Falta config/paths.local.json: copia config/paths.example.json y pon tus rutas.")
    raw = json.loads(cfg.read_text(encoding="utf-8-sig"))["paths"]
    paths = {k: Path(v) if Path(v).is_absolute() else REPO / v for k, v in raw.items()}
    return paths


def pe_timestamp(exe: Path) -> int:
    with open(exe, "rb") as f:
        head = f.read(4096)
    pe = struct.unpack_from("<I", head, 0x3C)[0]
    return struct.unpack_from("<I", head, pe + 8)[0]


def check(paths: dict[str, Path]) -> list[str]:
    """Returns the list of problems (empty = ready)."""
    problems = []
    exe = paths.get("mirrors_edge_exe")
    if not exe or not exe.exists():
        problems.append(f"No encuentro MirrorsEdge.exe en {exe}")
    else:
        ts = pe_timestamp(exe)
        build = "GOG v1.01" if ts == GOG_101_TIMESTAMP else f"desconocida (timestamp 0x{ts:08X})"
        print(f"Mirror's Edge: {exe} ({build})")
        if ts != GOG_101_TIMESTAMP:
            print("  Aviso: solo se probó con GOG v1.01. Steam/EA deberían servir (mismos .upk), pero avisa si falla.")
    cooked = paths.get("mirrors_edge_cooked_pc")
    for key in ("body_1p", "anims_1p"):
        f = cooked / MANIFEST["source_files_me"][key] if cooked else None
        if not f or not f.exists():
            problems.append(f"No encuentro {MANIFEST['source_files_me'][key]} en {cooked}")
    umodel = paths.get("umodel_exe")
    if not umodel or not umodel.exists():
        problems.append(f"No encuentro UE Viewer (umodel.exe) en {umodel}. Descárgalo de https://www.gildor.org/en/projects/umodel "
                        "y ponlo en tools/umodel/ (o cambia umodel_exe en config/paths.local.json).")
    studiomdl = paths.get("source_sdk_bin", Path()) / "studiomdl.exe"
    if not studiomdl.exists():
        problems.append(f"No encuentro studiomdl.exe en {studiomdl.parent}")
    portal = paths.get("portal_root")
    if not portal or not (portal / "portal" / "gameinfo.txt").exists():
        problems.append(f"portal_root no apunta a Portal: {portal}")
    return problems


def run(cmd: list[str], cwd: Path | None = None) -> str:
    print("  >", " ".join(f'"{c}"' if " " in str(c) else str(c) for c in cmd))
    p = subprocess.run([str(c) for c in cmd], cwd=cwd, capture_output=True, text=True, errors="replace")
    if p.returncode != 0:
        tail = "\n".join((p.stdout + p.stderr).splitlines()[-25:])
        raise ExtractError(f"Falló {Path(cmd[0]).name} (código {p.returncode}):\n{tail}")
    return p.stdout


# ---------- A1: UE Viewer export ----------

def step_a1(paths: dict[str, Path]) -> None:
    print("== A1: exportar con UE Viewer")
    export = WORK / "export"
    if export.exists():
        shutil.rmtree(export)
    export.mkdir(parents=True)
    cooked = paths["mirrors_edge_cooked_pc"]
    for key in ("body_1p", "anims_1p"):
        pkg = cooked / MANIFEST["source_files_me"][key]
        run([paths["umodel_exe"], "-game=mirror", f"-path={cooked}", "-export", "-psk", "-nooverwrite",
             f"-out={export}", str(pkg)])
    psks, psas = find_exports()
    print(f"  {len(psks)} mallas (.psk), {len(psas)} sets de animación (.psa)")
    write_skeleton_doc(psks, psas)


def find_exports() -> tuple[list[Path], list[Path]]:
    export = WORK / "export"
    psks = sorted(p for p in export.rglob("*") if p.suffix.lower() in (".psk", ".pskx"))
    psas = sorted(export.rglob("*.psa"))
    if not psks or not psas:
        raise ExtractError(f"UE Viewer no dejó .psk/.psa en {export}. Corre antes --step a1.")
    return psks, psas


def pick_mesh(psks: list[Path]) -> Path:
    """The 1P package may hold several meshes: take the one with the most bones (the full 1P body)."""
    return max(psks, key=lambda p: (len(ue_psk.read_psk(p).bones), p.stat().st_size))


def write_skeleton_doc(psks: list[Path], psas: list[Path]) -> None:
    """A1 deliverable: bone and animation NAMES only (no game data), for the team to map to FaithAnimState."""
    mesh_path = pick_mesh(psks)
    mesh = ue_psk.read_psk(mesh_path)
    lines = ["# Esqueleto y animaciones de Faith en primera persona (A1)", "",
             "> Generado por `python mod/tools/extractor/extract.py --step a1`. Solo nombres: ningún archivo del juego.",
             "", f"- **Malla elegida:** `{mesh_path.stem}` ({len(mesh.bones)} huesos, {len(mesh.points)} vértices, "
             f"{len(mesh.faces)} triángulos, {len(mesh.materials)} materiales)",
             f"- **Otras mallas del paquete:** {', '.join(f'`{p.stem}`' for p in psks if p != mesh_path) or 'ninguna'}",
             f"- **Límite de Source:** 128 huesos por modelo → {'✅ cabe' if len(mesh.bones) <= 128 else '❌ hay que quitar huesos'}",
             "", "## Huesos", "", "| # | Hueso | Padre |", "|---|---|---|"]
    for i, b in enumerate(mesh.bones):
        parent = "—" if i == 0 else mesh.bones[b.parent].name
        lines.append(f"| {i} | `{b.name}` | `{parent}` |")
    lines += ["", "## Materiales", ""] + [f"- `{m}`" for m in mesh.materials]
    lines += ["", "## Animaciones", ""]
    resolved = resolve_anims(psas)
    for psa_path in psas:
        psa = ue_psk.read_psa(psa_path)
        lines += [f"### `{psa_path.stem}` ({len(psa.sequences)} animaciones, {len(psa.bones)} huesos)", "",
                  "| Animación | Frames | FPS | Usada como |", "|---|---|---|---|"]
        for s in psa.sequences:
            used = ", ".join(k for k, v in resolved.items() if v and v[1] == s.name)
            lines.append(f"| `{s.name}` | {s.num_frames} | {s.rate:.0f} | {used} |")
        lines.append("")
    lines += ["## Asignación a `FaithAnimState`", "", "| Secuencia | Animación de ME |", "|---|---|"]
    for k, v in resolved.items():
        lines.append(f"| `{k}` | {f'`{v[1]}`' if v else '⚠️ sin animación (usa el fallback)'} |")
    out = REPO / "docs" / "contratos" / "faith_esqueleto.md"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"  Escrito {out.relative_to(REPO)}")


def resolve_anims(psas: list[Path]) -> dict[str, tuple[Path, str] | None]:
    names = [(p, s.name) for p in psas for s in ue_psk.read_psa(p).sequences]
    out: dict[str, tuple[Path, str] | None] = {}
    for seq in MANIFEST["sequences"]:
        rule = ANIM_MAP["sequences"].get(seq, {})
        hit = None
        if rule.get("exact"):
            hit = next(((p, n) for p, n in names if n.lower() == rule["exact"].lower()), None)
        for words in rule.get("any", []):
            if hit:
                break
            hit = next(((p, n) for p, n in names if all(w.lower() in n.lower() for w in words)), None)
        out[seq] = hit
    return out


# ---------- A2: PSK/PSA -> SMD ----------

def step_a2() -> None:
    print("== A2: PSK/PSA -> SMD")
    psks, psas = find_exports()
    mesh = ue_psk.read_psk(pick_mesh(psks))
    out = WORK / "smd"
    if out.exists():
        shutil.rmtree(out)
    (out / "anims").mkdir(parents=True)
    (out / "faith_ref.smd").write_text(smd.reference_smd(mesh), encoding="utf-8")
    resolved = resolve_anims(psas)
    cache: dict[Path, ue_psk.Psa] = {}
    missing = []
    for seq, hit in resolved.items():
        if not hit:
            missing.append(seq)
            continue
        psa = cache.setdefault(hit[0], ue_psk.read_psa(hit[0]))
        idx = next(i for i, s in enumerate(psa.sequences) if s.name == hit[1])
        (out / "anims" / f"{seq}.smd").write_text(smd.animation_smd(mesh, psa, idx), encoding="utf-8")
    print(f"  {len(resolved) - len(missing)} animaciones convertidas; sin animación propia: {', '.join(missing) or 'ninguna'}")


# ---------- A3: .qc + studiomdl ----------

def fallback_chain(seq: str, have: set[str]) -> str:
    seen = set()
    while seq not in have and seq not in seen:
        seen.add(seq)
        seq = ANIM_MAP["sequences"].get(seq, {}).get("fallback", "idle")
    return seq if seq in have else "idle"


def camera_bone(mesh: ue_psk.Psk) -> str:
    for want in ("camera", "head", "neck"):
        for b in mesh.bones:
            if want in b.name.lower():
                return b.name
    return mesh.bones[0].name


def write_qc(model_key: str, mesh: ue_psk.Psk) -> Path:
    smd_dir = WORK / "smd"
    have = {p.stem for p in (smd_dir / "anims").glob("*.smd")}
    if "idle" not in have:  # studiomdl needs at least one sequence
        (smd_dir / "anims" / "idle.smd").write_text((smd_dir / "faith_ref.smd").read_text(encoding="utf-8")
                                                    .split("triangles")[0] + "\n", encoding="utf-8")
        have.add("idle")
    model = MANIFEST["models"][model_key]
    lines = [f"// Generado por mod/tools/extractor/extract.py ({model['use']}). No editar a mano.",
             f'$modelname "{model["path"].removeprefix("models/")}"',
             f'$cdmaterials "models/{MODEL_DIR}/faith/"',
             f'$body body "faith_ref.smd"',
             "$surfaceprop flesh",
             f'$attachment "camera" "{camera_bone(mesh)}" 0 0 0 rotate 0 0 0']
    for seq in MANIFEST["sequences"]:
        rule = ANIM_MAP["sequences"].get(seq, {})
        src = seq if seq in have else fallback_chain(seq, have)
        loop = " loop" if rule.get("loop") else ""
        lines.append(f'$sequence "{seq}" "anims/{src}.smd" fps 30{loop}')
    qc = smd_dir / f"{Path(model['path']).stem}.qc"
    qc.write_text("\n".join(l for l in lines if l) + "\n", encoding="utf-8")
    return qc


def step_a3(paths: dict[str, Path]) -> None:
    print("== A3: .qc + studiomdl")
    psks, _ = find_exports()
    mesh = ue_psk.read_psk(pick_mesh(psks))
    if len(mesh.bones) > 128:
        raise ExtractError(f"La malla tiene {len(mesh.bones)} huesos; Source admite 128.")
    portal = paths["portal_root"]
    game = portal / "portal"
    install = Path(MANIFEST["install_root"].replace("<Portal>", str(portal)))
    for key, model in MANIFEST["models"].items():
        qc = write_qc(key, mesh)
        run([paths["source_sdk_bin"] / "studiomdl.exe", "-game", game, "-nop4", "-verbose", qc], cwd=qc.parent)
        built = game / Path(model["path"]).parent
        dest = install / Path(model["path"]).parent
        dest.mkdir(parents=True, exist_ok=True)
        stem = Path(model["path"]).stem
        for f in built.glob(stem + ".*"):
            shutil.move(str(f), dest / f.name)
        print(f"  {key}: {dest / (stem + '.mdl')}")


def verify(paths: dict[str, Path]) -> bool:
    install = Path(MANIFEST["install_root"].replace("<Portal>", str(paths["portal_root"])))
    ok = True
    for key, model in MANIFEST["models"].items():
        base = install / model["path"]
        for ext in (".mdl", ".vvd", ".dx90.vtx"):
            f = base.with_name(base.stem + ext)
            present = f.exists() and f.stat().st_size > 0
            ok &= present
            print(f"  {'OK ' if present else 'FALTA'} {f}")
    return ok


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--step", choices=["a1", "a2", "a3", "all"], default="all")
    ap.add_argument("--check", action="store_true", help="solo comprobar rutas y herramientas")
    args = ap.parse_args()
    try:
        paths = load_paths()
        problems = check(paths)
        if args.check or problems:
            for p in problems:
                print("✗", p)
            if not problems:
                print("✓ Todo listo para extraer.")
            return 1 if problems else 0
        if args.step in ("a1", "all"):
            step_a1(paths)
        if args.step in ("a2", "all"):
            step_a2()
        if args.step in ("a3", "all"):
            step_a3(paths)
        if args.step == "all":
            print("== Verificación")
            if not verify(paths):
                return 1
            print("✓ Faith instalada. Para verla: <Portal>/bin/hlmv.exe y abre models/aperture_runner/faith_arms.mdl")
        return 0
    except ExtractError as e:
        print("✗", e)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
