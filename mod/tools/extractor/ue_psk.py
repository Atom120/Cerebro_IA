"""Readers for the Unreal ActorX formats that UE Viewer (umodel) exports: .psk (skeletal mesh) and .psa (animations).

Standard library only. Layouts follow the ActorX SDK (the same ones the Blender io_scene_psk_psa addon reads).
Owner: Antonio (A1/A2). Nothing read here is ever written into the repo.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path

HEADER = struct.Struct("<20siii")  # chunk id, type flag, data size, data count


@dataclass
class Bone:
    name: str
    parent: int          # index into the bone list; the root points to itself (0)
    rot: tuple[float, float, float, float]  # quaternion x, y, z, w (ActorX convention)
    pos: tuple[float, float, float]


@dataclass
class Wedge:
    point: int
    u: float
    v: float
    material: int


@dataclass
class Face:
    wedges: tuple[int, int, int]
    material: int


@dataclass
class Psk:
    points: list[tuple[float, float, float]] = field(default_factory=list)
    wedges: list[Wedge] = field(default_factory=list)
    faces: list[Face] = field(default_factory=list)
    materials: list[str] = field(default_factory=list)
    bones: list[Bone] = field(default_factory=list)
    weights: list[tuple[float, int, int]] = field(default_factory=list)  # weight, point, bone


@dataclass
class AnimSequence:
    name: str
    group: str
    num_bones: int
    rate: float          # frames per second
    first_frame: int
    num_frames: int


@dataclass
class Psa:
    bones: list[Bone] = field(default_factory=list)
    sequences: list[AnimSequence] = field(default_factory=list)
    # keys[frame][bone] = (pos xyz, rot xyzw), frames of every sequence one after another
    keys: list[tuple[tuple[float, float, float], tuple[float, float, float, float]]] = field(default_factory=list)

    def sequence_keys(self, seq: AnimSequence):
        n = len(self.bones)
        start = seq.first_frame * n
        return [self.keys[start + f * n: start + (f + 1) * n] for f in range(seq.num_frames)]


def _cstr(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("latin-1").strip()


def _chunks(data: bytes):
    off = 0
    while off + HEADER.size <= len(data):
        cid, _flags, size, count = HEADER.unpack_from(data, off)
        off += HEADER.size
        yield _cstr(cid), size, count, off
        off += size * count


def _bone(data: bytes, off: int) -> Bone:
    name = _cstr(data[off:off + 64])
    _flags, _children, parent = struct.unpack_from("<Iii", data, off + 64)
    qx, qy, qz, qw, px, py, pz = struct.unpack_from("<7f", data, off + 76)
    return Bone(name, parent, (qx, qy, qz, qw), (px, py, pz))


def read_psk(path: Path | str) -> Psk:
    data = Path(path).read_bytes()
    psk = Psk()
    for cid, size, count, off in _chunks(data):
        if cid.startswith("PNTS"):
            psk.points = [struct.unpack_from("<3f", data, off + i * size) for i in range(count)]
        elif cid.startswith("VTXW"):
            big = count > 65536
            for i in range(count):
                o = off + i * size
                point = struct.unpack_from("<I", data, o)[0] if big else struct.unpack_from("<H", data, o)[0]
                u, v = struct.unpack_from("<2f", data, o + 4)
                psk.wedges.append(Wedge(point, u, v, data[o + 12]))
        elif cid.startswith("FACE3200"):
            for i in range(count):
                w0, w1, w2, mat = struct.unpack_from("<3IB", data, off + i * size)
                psk.faces.append(Face((w0, w1, w2), mat))
        elif cid.startswith("FACE"):
            for i in range(count):
                w0, w1, w2, mat = struct.unpack_from("<3HB", data, off + i * size)
                psk.faces.append(Face((w0, w1, w2), mat))
        elif cid.startswith("MATT"):
            psk.materials = [_cstr(data[off + i * size: off + i * size + 64]) for i in range(count)]
        elif cid.startswith("REFSKEL"):
            psk.bones = [_bone(data, off + i * size) for i in range(count)]
        elif cid.startswith("RAWW"):
            psk.weights = [struct.unpack_from("<fii", data, off + i * size) for i in range(count)]
    return psk


def read_psa(path: Path | str) -> Psa:
    data = Path(path).read_bytes()
    psa = Psa()
    for cid, size, count, off in _chunks(data):
        if cid.startswith("BONENAMES"):
            psa.bones = [_bone(data, off + i * size) for i in range(count)]
        elif cid.startswith("ANIMINFO"):
            for i in range(count):
                o = off + i * size
                name, group = _cstr(data[o:o + 64]), _cstr(data[o + 64:o + 128])
                total_bones = struct.unpack_from("<i", data, o + 128)[0]
                _track_time, rate = struct.unpack_from("<2f", data, o + 148)
                _start_bone, first, frames = struct.unpack_from("<3i", data, o + 156)
                psa.sequences.append(AnimSequence(name, group, total_bones, rate, first, frames))
        elif cid.startswith("ANIMKEYS"):
            for i in range(count):
                px, py, pz, qx, qy, qz, qw = struct.unpack_from("<7f", data, off + i * size)
                psa.keys.append(((px, py, pz), (qx, qy, qz, qw)))
    return psa


# ---------- writers (only used by the tests to build small synthetic files) ----------

def _chunk(cid: str, size: int, items: list[bytes]) -> bytes:
    return HEADER.pack(cid.encode().ljust(20, b"\0"), 0x1e83b9, size, len(items)) + b"".join(items)


def _bone_bytes(b: Bone) -> bytes:
    return (b.name.encode().ljust(64, b"\0") + struct.pack("<Iii", 0, 0, b.parent)
            + struct.pack("<7f", *b.rot, *b.pos) + struct.pack("<4f", 0, 0, 0, 0))


def write_psk(path: Path | str, psk: Psk) -> None:
    out = [_chunk("ACTRHEAD", 0, [])]
    out.append(_chunk("PNTS0000", 12, [struct.pack("<3f", *p) for p in psk.points]))
    out.append(_chunk("VTXW0000", 16, [struct.pack("<HH2fBBH", w.point, 0, w.u, w.v, w.material, 0, 0)
                                       for w in psk.wedges]))
    out.append(_chunk("FACE0000", 12, [struct.pack("<3HBBI", *f.wedges, f.material, 0, 1) for f in psk.faces]))
    out.append(_chunk("MATT0000", 88, [m.encode().ljust(64, b"\0") + bytes(24) for m in psk.materials]))
    out.append(_chunk("REFSKELT", 120, [_bone_bytes(b) for b in psk.bones]))
    out.append(_chunk("RAWWEIGHTS", 12, [struct.pack("<fii", *w) for w in psk.weights]))
    Path(path).write_bytes(b"".join(out))


def write_psa(path: Path | str, psa: Psa) -> None:
    out = [_chunk("ANIMHEAD", 0, [])]
    out.append(_chunk("BONENAMES", 120, [_bone_bytes(b) for b in psa.bones]))
    infos = []
    for s in psa.sequences:
        infos.append(s.name.encode().ljust(64, b"\0") + s.group.encode().ljust(64, b"\0")
                     + struct.pack("<4i", s.num_bones, 0, 0, s.num_frames * s.num_bones)
                     + struct.pack("<3f", 1.0, float(s.num_frames), s.rate)
                     + struct.pack("<3i", 0, s.first_frame, s.num_frames))
    out.append(_chunk("ANIMINFO", 168, infos))
    out.append(_chunk("ANIMKEYS", 32, [struct.pack("<8f", *p, *q, 1.0) for p, q in psa.keys]))
    Path(path).write_bytes(b"".join(out))
