"""Tests for the extractor (A1-A4) with small synthetic PSK/PSA files. No Mirror's Edge files needed.

    python mod/tools/extractor/test_extractor.py
"""
from __future__ import annotations

import math
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import extract  # noqa: E402
import smd  # noqa: E402
from ue_psk import AnimSequence, Bone, Face, Psa, Psk, Wedge, read_psa, read_psk, write_psa, write_psk  # noqa: E402

FAILS = 0
S45 = math.sqrt(0.5)


def check(cond: bool, what: str) -> None:
    global FAILS
    if not cond:
        FAILS += 1
        print("  FAIL", what)


def near(a: float, b: float, tol: float = 1e-4) -> bool:
    return abs(a - b) <= tol


def sample_psk() -> Psk:
    bones = [Bone("Root", 0, (0.0, 0.0, S45, S45), (0.0, 0.0, 0.0)),   # 90 deg about Z
             Bone("Hand", 0, (0.0, 0.0, S45, S45), (25.4, 50.8, 0.0))]
    return Psk(points=[(0, 0, 0), (25.4, 0, 0), (0, 25.4, 0)],
               wedges=[Wedge(0, 0, 0, 0), Wedge(1, 1, 0, 0), Wedge(2, 0, 1, 0)],
               faces=[Face((0, 1, 2), 0)], materials=["faith_arms"], bones=bones,
               weights=[(1.0, 0, 0), (0.75, 1, 1), (0.25, 1, 0), (1.0, 2, 1)])


def test_roundtrip(tmp: Path) -> None:
    write_psk(tmp / "m.psk", sample_psk())
    m = read_psk(tmp / "m.psk")
    check(len(m.points) == 3 and len(m.faces) == 1 and len(m.wedges) == 3, "psk counts")
    check([b.name for b in m.bones] == ["Root", "Hand"], "bone names")
    check(m.materials == ["faith_arms"], "materials")
    check(len(m.weights) == 4, "weights")
    keys = [((0, 0, 0), (0, 0, 0, 1)), ((1, 2, 3), (0, 0, 0, 1))] * 3
    write_psa(tmp / "a.psa", Psa(bones=m.bones, sequences=[AnimSequence("Run_Fwd", "None", 2, 30, 0, 2),
                                                          AnimSequence("Idle", "None", 2, 30, 2, 1)], keys=keys))
    a = read_psa(tmp / "a.psa")
    check([s.name for s in a.sequences] == ["Run_Fwd", "Idle"], "psa sequence names")
    check(a.sequences[0].num_frames == 2 and a.sequences[1].first_frame == 2, "psa frames")
    check(near(a.sequence_keys(a.sequences[0])[1][1][0][2], 3.0), "psa key lookup")


def test_conversion() -> None:
    # Points: (x, y, z) uu -> (z, x, y) / 2.54 su (upright, not mirrored).
    check(all(near(a, b) for a, b in zip(smd.to_source((25.4, 50.8, 76.2)), (30.0, 10.0, 20.0))), "point axes")
    # The rotation is proper: forward (+Z) x left (+X) = up (+Y) before, +X x +Y = +Z after.
    f, l, u = smd.to_source((0, 0, 1)), smd.to_source((1, 0, 0)), smd.to_source((0, 1, 0))
    cross = (f[1] * l[2] - f[2] * l[1], f[2] * l[0] - f[0] * l[2], f[0] * l[1] - f[1] * l[0])
    check(all(near(a * 2.54, b) for a, b in zip(cross, u)), "no mirror (right-handed)")
    # Root with identity rotation: becomes the upright rotation, whose matrix maps x -> y.
    q, p = smd.ue_local(0, (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 25.4))
    check(all(near(a, b) for a, b in zip(p, (10.0, 0.0, 0.0))), "root position rotated")
    rx, ry, rz = smd.quat_to_euler_xyz(q)
    check(near(rx, math.pi / 2) and near(ry, 0) and near(rz, math.pi / 2), "upright euler (90, 0, 90)")
    # Non-root: ActorX stores it conjugated; position stays local.
    q, p = smd.ue_local(1, (0.0, 0.0, S45, S45), (25.4, 50.8, 0.0))
    check(near(smd.quat_to_euler_xyz(q)[2], -math.pi / 2), "child rotation conjugated")
    check(near(p[0], 10.0) and near(p[1], 20.0) and near(p[2], 0.0), "child position local, uu -> su")
    rx, ry, rz = smd.quat_to_euler_xyz((math.sin(0.2), 0, 0, math.cos(0.2)))
    check(near(rx, 0.4) and near(ry, 0) and near(rz, 0), "euler X")
    rx, ry, rz = smd.quat_to_euler_xyz((0, math.sin(0.3), 0, math.cos(0.3)))
    check(near(ry, 0.6) and near(rx, 0) and near(rz, 0), "euler Y")


def test_reference_smd() -> None:
    text = smd.reference_smd(sample_psk())
    lines = text.splitlines()
    check(lines[0] == "version 1", "smd header")
    check('0 "Root" -1' in lines and '1 "Hand" 0' in lines, "smd nodes")
    tri = lines[lines.index("triangles") + 1: lines.index("triangles") + 5]
    check(tri[0] == "faith_arms", "material line")
    # Same winding; the first vertex is wedge 0 = point (0, 0, 0). Its triangle lies in the uu XY plane, which
    # becomes the Source YZ plane, so the normal is +X (or -X).
    v = tri[1].split()
    check(all(near(float(c), 0.0) for c in v[1:4]), "first vertex")
    check(near(abs(float(v[4])), 1.0), "normal along X")
    check(near(float(v[8]), 1.0), "V flipped (1 - 0 = 1)")
    # Point 1 has two influences, normalised and sorted.
    v1 = tri[2].split()
    check(v1[9] == "2" and v1[10] == "1" and near(float(v1[11]), 0.75), "two weights, heaviest first")


def test_anim_map() -> None:
    names = ["Idle_Stand", "Run_Fwd", "Sprint_Fwd", "WallRun_Left", "WallRun_Right", "SkillRoll", "Slide_Loop",
             "Grab_Idle", "Grab_PullUp", "SpeedVault", "Jump_Start", "Fall_Loop", "Land_Soft", "Land_Hard"]
    with tempfile.TemporaryDirectory() as d:
        keys = [((0, 0, 0), (0, 0, 0, 1))] * len(names)
        write_psa(Path(d) / "a.psa", Psa(bones=[Bone("Root", 0, (0, 0, 0, 1), (0, 0, 0))],
                                         sequences=[AnimSequence(n, "None", 1, 30, i, 1) for i, n in enumerate(names)],
                                         keys=keys))
        r = extract.resolve_anims([Path(d) / "a.psa"])
    got = {k: v[1] if v else None for k, v in r.items()}
    check(set(got) == set(extract.MANIFEST["sequences"]), "every manifest sequence is mapped")
    check(got["sprint"] == "Sprint_Fwd" and got["run"] == "Run_Fwd", "run/sprint")
    check(got["wallrun_left"] == "WallRun_Left" and got["wallrun_right"] == "WallRun_Right", "wallruns")
    check(got["ledge_hang"] == "Grab_Idle" and got["ledge_climb_up"] == "Grab_PullUp", "ledge")
    check(got["land_hard"] == "Land_Hard" and got["skill_roll"] == "SkillRoll", "landings")
    check(got["wallclimb"] is None, "missing anims stay None")
    check(extract.fallback_chain("wallclimb_turn", {"idle", "run"}) == "idle", "fallback chain ends in idle")
    check(extract.fallback_chain("sprint", {"idle", "run"}) == "run", "fallback chain")


def main() -> int:
    print("extractor tests (A1-A4)")
    with tempfile.TemporaryDirectory() as d:
        test_roundtrip(Path(d))
    test_conversion()
    test_reference_smd()
    test_anim_map()
    print("  OK" if FAILS == 0 else f"  {FAILS} fallos")
    return 1 if FAILS else 0


if __name__ == "__main__":
    raise SystemExit(main())
