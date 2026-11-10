"""A2: convert ActorX .psk/.psa (from UE Viewer) to Source .smd (reference mesh + one .smd per animation).

Pure Python, no Blender needed. Conversion UE3 -> Source: scale 1/2.54 (uu -> su), mirror Y (left-handed ->
right-handed), conjugate the ActorX rotations of every non-root bone (ActorX stores them inverted, like the
io_scene_psk_psa importer does), reverse the triangle winding (the mirror flips it) and flip V.
Owner: Antonio. The output goes to a temporary folder, never into the repo.
"""
from __future__ import annotations

import math
from pathlib import Path

from ue_psk import Bone, Psa, Psk

UU_TO_SU = 1.0 / 2.54
Quat = tuple[float, float, float, float]
Vec = tuple[float, float, float]


def ue_local(bone_index: int, rot: Quat, pos: Vec, scale: float = UU_TO_SU) -> tuple[Quat, Vec]:
    """ActorX local bone transform -> Source local transform (quaternion xyzw, position)."""
    x, y, z, w = rot
    if bone_index != 0:
        x, y, z = -x, -y, -z          # ActorX keeps non-root rotations conjugated
    # Mirror across the XZ plane (y -> -y): a rotation q maps to (-x, y, -z, w).
    q = (-x, y, -z, w)
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    q = tuple(c / n for c in q)
    return q, (pos[0] * scale, -pos[1] * scale, pos[2] * scale)


def quat_to_euler_xyz(q: Quat) -> Vec:
    """Quaternion -> the (x, y, z) radians studiomdl expects (matrix = Rz * Ry * Rx)."""
    x, y, z, w = q
    m00 = 1 - 2 * (y * y + z * z)
    m10 = 2 * (x * y + z * w)
    m20 = 2 * (x * z - y * w)
    m21 = 2 * (y * z + x * w)
    m22 = 1 - 2 * (x * x + y * y)
    m01 = 2 * (x * y - z * w)
    m11 = 1 - 2 * (x * x + z * z)
    sy = max(-1.0, min(1.0, -m20))
    ry = math.asin(sy)
    if abs(sy) < 0.999999:
        rx = math.atan2(m21, m22)
        rz = math.atan2(m10, m00)
    else:  # gimbal lock
        rx = 0.0
        rz = math.atan2(-m01, m11)
    return rx, ry, rz


def _nodes(bones: list[Bone]) -> list[str]:
    out = ["nodes"]
    for i, b in enumerate(bones):
        parent = -1 if i == 0 or b.parent == i else b.parent
        out.append(f'{i} "{b.name}" {parent}')
    out.append("end")
    return out


def _frame(lines: list[str], time: int, locals_: list[tuple[Quat, Vec]]) -> None:
    lines.append(f"time {time}")
    for i, (q, p) in enumerate(locals_):
        rx, ry, rz = quat_to_euler_xyz(q)
        lines.append(f"{i} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {rx:.6f} {ry:.6f} {rz:.6f}")


def _cross(a: Vec, b: Vec) -> Vec:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _sub(a: Vec, b: Vec) -> Vec:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _norm(v: Vec) -> Vec:
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
    return (v[0] / n, v[1] / n, v[2] / n)


def reference_smd(psk: Psk, material_names: list[str] | None = None) -> str:
    """Reference .smd: skeleton in bind pose + skinned triangles."""
    lines = ["version 1"] + _nodes(psk.bones) + ["skeleton"]
    _frame(lines, 0, [ue_local(i, b.rot, b.pos) for i, b in enumerate(psk.bones)])
    lines.append("end")

    influences: dict[int, list[tuple[int, float]]] = {}
    for weight, point, bone in psk.weights:
        if weight > 0.0:
            influences.setdefault(point, []).append((bone, weight))
    pts = [(p[0] * UU_TO_SU, -p[1] * UU_TO_SU, p[2] * UU_TO_SU) for p in psk.points]
    mats = material_names or psk.materials or ["faith"]

    lines.append("triangles")
    for face in psk.faces:
        ws = [psk.wedges[i] for i in reversed(face.wedges)]  # mirrored: reverse the winding
        a, b, c = (pts[w.point] for w in ws)
        normal = _norm(_cross(_sub(b, a), _sub(c, a)))
        mat = mats[face.material] if face.material < len(mats) else mats[0]
        lines.append(mat)
        for w in ws:
            p = pts[w.point]
            links = sorted(influences.get(w.point, [(0, 1.0)]), key=lambda x: -x[1])[:4]
            total = sum(x[1] for x in links) or 1.0
            link_txt = " ".join(f"{bone} {wt / total:.6f}" for bone, wt in links)
            lines.append(f"{links[0][0]} {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} "
                         f"{normal[0]:.6f} {normal[1]:.6f} {normal[2]:.6f} "
                         f"{w.u:.6f} {1.0 - w.v:.6f} {len(links)} {link_txt}")
    lines.append("end")
    return "\n".join(lines) + "\n"


def animation_smd(psk: Psk, psa: Psa, seq_index: int) -> str:
    """One animation as an .smd. Bones are matched by name; bones missing in the PSA keep the bind pose."""
    seq = psa.sequences[seq_index]
    frames = psa.sequence_keys(seq)
    by_name = {b.name.lower(): i for i, b in enumerate(psa.bones)}
    lines = ["version 1"] + _nodes(psk.bones) + ["skeleton"]
    for f, keys in enumerate(frames):
        locals_ = []
        for i, b in enumerate(psk.bones):
            j = by_name.get(b.name.lower())
            if j is None:
                locals_.append(ue_local(i, b.rot, b.pos))
            else:
                pos, rot = keys[j]
                locals_.append(ue_local(i, rot, pos))
        _frame(lines, f, locals_)
    lines.append("end")
    return "\n".join(lines) + "\n"
