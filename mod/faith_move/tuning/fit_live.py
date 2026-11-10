"""P5: fit Faith's movement constants to the live Mirror's Edge recordings (qa/telemetria/*_live.csv).

Usage (from the repo root):
    python mod/faith_move/tuning/fit_live.py

Writes docs/tuning/P5_ajuste.md (table) and docs/tuning/*.svg (charts). Standard library only.
All measurements are in Mirror's Edge units (uu, uu/s); Source units are uu / 2.54.
"""
from __future__ import annotations

import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TELEMETRY = ROOT / "qa" / "telemetria"
OUT = ROOT / "docs" / "tuning"
UU_TO_SU = 1.0 / 2.54

# EMovement values used here (mod/tools/me_telemetry/me_telemetry.h).
WALK, FALL, WR_RIGHT, WR_LEFT, WCLIMB, VAULT, PULLUP, JUMP, WR_JUMP, SLIDE, LANDING, ROLL = (
    1, 2, 4, 5, 6, 9, 10, 11, 12, 16, 20, 91)


class Sample:
    __slots__ = ("t", "v2d", "vz", "z", "ms")

    def __init__(self, row: dict[str, str]):
        self.t = float(row["t"])
        self.v2d = math.hypot(float(row["vx"]), float(row["vy"]))
        self.vz = float(row["vz"])
        self.z = float(row["z"])
        self.ms = int(row["movement_state"])


def load(name: str) -> list[Sample]:
    with open(TELEMETRY / f"{name}_live.csv", newline="") as f:
        return [Sample(r) for r in csv.DictReader(f)]


def segments(data: list[Sample], state: int) -> list[list[Sample]]:
    out, cur = [], []
    for s in data:
        if s.ms == state:
            cur.append(s)
        elif cur:
            out.append(cur)
            cur = []
    if cur:
        out.append(cur)
    return out


def slope(pts: list[tuple[float, float]]) -> float:
    n = len(pts)
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    return sum((x - mx) * (y - my) for x, y in pts) / sum((x - mx) ** 2 for x, _ in pts)


def mean(xs: list[float]) -> float:
    return sum(xs) / len(xs)


def duration(seg: list[Sample]) -> float:
    return seg[-1].t - seg[0].t


def fit_sprint(data: list[Sample]) -> tuple[float, float]:
    """Fit v = cap - gap * exp(-t / tau) on the sprint part (before the stick is released)."""
    run = [s for s in data if s.ms == WALK and s.v2d > 300 and s.t < 6.72]
    best = None
    for cap2 in range(1430, 1460):
        cap = cap2 / 2
        pts = [(s.t, math.log(cap - s.v2d)) for s in run if cap - s.v2d > 1]
        b = slope(pts)
        a = mean([p[1] for p in pts]) - b * mean([p[0] for p in pts])
        err = sum((y - (a + b * x)) ** 2 for x, y in pts)
        if best is None or err < best[0]:
            best = (err, cap, -1.0 / b)
    return best[1], best[2]


def fit_stop(data: list[Sample]) -> float:
    """Time constant of the exponential braking when the stick is released."""
    peak = max(range(len(data)), key=lambda i: data[i].v2d)
    pts = [(s.t, math.log(s.v2d)) for s in data[peak + 1:] if 15 < s.v2d < 650]
    return -1.0 / slope(pts)


def measure() -> list[dict]:
    rows: list[dict] = []

    def add(name, measured, unit, source, before, after, note=""):
        rows.append(dict(name=name, measured=measured, unit=unit, source=source,
                         before=before, after=after, note=note))

    jump = load("jump")
    roll = load("skill_roll")
    g_jump = -mean([slope([(s.t, s.vz) for s in seg[1:]]) for seg in segments(jump, JUMP)])
    g_roll = -slope([(s.t, s.vz) for s in roll if s.ms in (JUMP, FALL) and s.vz > -1500][1:])
    g = (g_jump + g_roll) / 2
    add("Gravedad de Faith", g, "uu/s²", "jump, skill_roll", "646 su/s² (1641 uu)", f"{g * UU_TO_SU:.1f} su/s²")
    jz = mean([seg[0].vz for seg in segments(jump, JUMP)])
    add("Salto (vz inicial)", jz, "uu/s", "jump (3 saltos)", "248 su/s (630 uu)", f"{jz * UU_TO_SU:.1f} su/s")
    add_xy = mean([seg[0].v2d - prev.v2d for seg in segments(jump, JUMP)[1:]
                   for prev in [jump[jump.index(seg[0]) - 1]]])
    add("Salto: velocidad horizontal añadida", add_xy, "uu/s", "jump", "39.4 su/s (100 uu)", "sin cambio")

    sprint = load("sprint_start")
    cap, tau = fit_sprint(sprint)
    add("Sprint máximo", cap, "uu/s", "sprint_start", "283.5 su/s (720 uu)", "sin cambio")
    add("Sprint: constante de tiempo", tau, "s", "sprint_start", "rampa lineal de 1.5 s",
        f"exponencial, τ = {tau:.2f} s", "157.5 → 281.5 su/s en ~7.7 s")
    tau_stop = fit_stop(sprint)
    add("Frenado al soltar (τ)", tau_stop, "s", "sprint_start", "lineal 2419 su/s² (0.12 s)",
        f"exponencial, τ = {tau_stop:.2f} s", "283.5 → 0 en ~0.5 s")

    wr = load("wallrun_right") + load("wallrun_left")
    # Only whole wallruns: long enough and not already running when the recording started (t > 0).
    wr_segs = [seg for st in (WR_RIGHT, WR_LEFT) for seg in segments(wr, st) if len(seg) > 30 and seg[0].t > 0.1]
    g_wr = -mean([slope([(s.t, s.vz) for s in seg]) for seg in wr_segs])
    add("Wallrun: gravedad", g_wr, "uu/s²", "wallrun_right (entero)", "escala 0.28", f"escala {g_wr / g:.3f}")
    wr_dur = max(duration(seg) for seg in wr_segs)
    add("Wallrun: duración máxima", wr_dur, "s", "wallrun_right", "1.2 s", "sin cambio (sale sola por la gravedad)")
    left = load("wallrun_left")
    ground_starts = [seg[0].vz for seg in segments(left, WR_LEFT) if left.index(seg[0]) > 0 and left[left.index(seg[0]) - 1].ms == WALK]
    if ground_starts:
        z0 = mean(ground_starts)
        add("Wallrun desde el suelo: vz", z0, "uu/s", "wallrun_left", "150 su/s", f"{z0 * UU_TO_SU:.1f} su/s")
    wj = mean([seg[0].vz for seg in segments(wr, WR_JUMP)])
    add("Salto desde la pared: vz", wj, "uu/s", "wallrun_*", "= salto (248 su/s)", f"{wj * UU_TO_SU:.1f} su/s",
        "constante nueva kWallrunJumpZ")

    ledge = load("ledge_grab")
    wc = segments(ledge, WCLIMB)[0]
    add("Wallclimb: vz inicial", wc[0].vz, "uu/s", "ledge_grab", "260 su/s", f"{wc[0].vz * UU_TO_SU:.1f} su/s",
        "wallclimb.csv grabó un vault; el wallclimb está en ledge_grab.csv")
    g_wc = -slope([(s.t, s.vz) for s in wc])
    add("Wallclimb: gravedad", g_wc, "uu/s²", "ledge_grab", "escala 0.6", f"escala {g_wc / g:.3f}")
    pull = mean([duration(seg) for seg in segments(ledge, PULLUP)])
    add("Subir el borde (duración)", pull, "s", "ledge_grab (2)", "0.6 s", f"{pull:.2f} s")
    vault = mean([duration(seg) for seg in segments(load("wallclimb"), VAULT)])
    add("Vault (duración)", vault, "s", "wallclimb (2)", "0.35 s", f"{vault:.2f} s")

    slide = load("slide")
    sl = segments(slide, SLIDE)
    dec = -mean([slope([(s.t, s.v2d) for s in seg]) for seg in sl])
    add("Slide: frenado", dec, "uu/s²", "slide (2)", "120 su/s²", f"{dec * UU_TO_SU:.1f} su/s²")
    add("Slide: velocidad de salida", mean([seg[-1].v2d for seg in sl]), "uu/s", "slide", "98.4 su/s (250 uu)",
        "sin cambio")

    hard = segments(load("hard_landing"), LANDING)[0]
    add("Caída dura: bloqueo", duration(hard), "s", "hard_landing", "0.5 s, sale a 59 su/s",
        f"{duration(hard):.2f} s, sale a 0")
    rs = segments(roll, ROLL)[0]
    add("Skill Roll: duración", duration(rs), "s", "skill_roll", "0.6 s", f"{duration(rs):.2f} s")
    add("Skill Roll: velocidad de salida", rs[-1].v2d, "uu/s", "skill_roll", "conserva la del aterrizaje",
        f"{rs[-1].v2d * UU_TO_SU:.1f} su/s")
    return rows


# ---------- charts (plain SVG) ----------

def svg_chart(path: Path, title: str, ylabel: str, series: list[tuple[str, str, list[tuple[float, float]]]]):
    w, h, ml, mr, mt, mb = 640, 320, 56, 16, 32, 40
    xs = [x for _, _, pts in series for x, _ in pts]
    ys = [y for _, _, pts in series for _, y in pts]
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(0.0, min(ys)), max(ys) * 1.05

    def px(x):
        return ml + (x - x0) / (x1 - x0) * (w - ml - mr)

    def py(y):
        return h - mb - (y - y0) / (y1 - y0) * (h - mt - mb)

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" font-family="sans-serif" font-size="12">',
           f'<rect width="{w}" height="{h}" fill="white"/>',
           f'<text x="{ml}" y="20" font-size="14" font-weight="bold">{title}</text>']
    for i in range(6):
        yv = y0 + (y1 - y0) * i / 5
        out.append(f'<line x1="{ml}" x2="{w - mr}" y1="{py(yv):.1f}" y2="{py(yv):.1f}" stroke="#e5e5e5"/>')
        out.append(f'<text x="{ml - 6}" y="{py(yv) + 4:.1f}" text-anchor="end" fill="#555">{yv:.0f}</text>')
    for i in range(6):
        xv = x0 + (x1 - x0) * i / 5
        out.append(f'<text x="{px(xv):.1f}" y="{h - mb + 16}" text-anchor="middle" fill="#555">{xv:.1f}</text>')
    out.append(f'<text x="{(ml + w - mr) / 2}" y="{h - 6}" text-anchor="middle" fill="#555">tiempo (s)</text>')
    out.append(f'<text x="14" y="{(mt + h - mb) / 2}" transform="rotate(-90 14 {(mt + h - mb) / 2})" '
               f'text-anchor="middle" fill="#555">{ylabel}</text>')
    for i, (name, color, pts) in enumerate(series):
        d = " ".join(f"{px(x):.1f},{py(y):.1f}" for x, y in pts)
        dash = ' stroke-dasharray="6 4"' if i else ""
        out.append(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="2"{dash}/>')
        lx = w - mr - 200
        ly = mt + 8 + i * 16
        out.append(f'<line x1="{lx}" x2="{lx + 20}" y1="{ly}" y2="{ly}" stroke="{color}" stroke-width="2"{dash}/>')
        out.append(f'<text x="{lx + 26}" y="{ly + 4}">{name}</text>')
    out.append("</svg>")
    path.write_text("\n".join(out), encoding="utf-8")


def charts(g: float, jz: float, tau: float, tau_stop: float, g_wr: float, slide_dec: float):
    ME, OLD, NEW = "#1f6feb", "#d1242f", "#1a7f37"
    sprint = load("sprint_start")
    me = [(s.t, s.v2d) for s in sprint if s.t < 7.4]
    v0 = me[0][1]
    old, new = [], []
    for i in range(0, 741):
        t = i / 100
        if t < 6.74:
            old.append((t, min(720.0, v0 + (720 - 400) / 1.5 * t)))
            new.append((t, 720 - (720 - v0) * math.exp(-t / tau)))
        else:
            dt = t - 6.74
            old.append((t, max(0.0, 720 - 6144 * dt)))
            v = new[-1][1] if dt < 0.011 else 715 * math.exp(-dt / tau_stop)
            new.append((t, v if v > 12 else 0.0))
    svg_chart(OUT / "sprint.svg", "Sprint y frenado (velocidad horizontal)", "uu/s",
              [("Mirror's Edge", ME, me), ("antes (lineal)", OLD, old), ("ahora (P5)", NEW, new)])

    jump = load("jump")
    seg = [s for s in jump if s.t < 1.0 and s.ms in (JUMP, FALL)]
    t0 = seg[0].t
    me = [(s.t - t0, s.vz) for s in seg]
    old = [(i / 100, 630 - 1641 * i / 100) for i in range(0, 78)]
    new = [(i / 100, jz - g * i / 100) for i in range(0, 78)]
    svg_chart(OUT / "salto.svg", "Salto: velocidad vertical", "uu/s",
              [("Mirror's Edge", ME, me), ("antes", OLD, old), ("ahora (P5)", NEW, new)])

    wr = load("wallrun_right")
    seg = [s for s in wr if s.ms == WR_RIGHT and s.t > 2.5]
    t0, vz0 = seg[0].t, seg[0].vz
    me = [(s.t - t0, s.vz) for s in seg]
    dur = seg[-1].t - t0
    old = [(i / 100, vz0 - 1641 * 0.28 * i / 100) for i in range(0, int(dur * 100))]
    new = [(i / 100, vz0 - g_wr * i / 100) for i in range(0, int(dur * 100))]
    svg_chart(OUT / "wallrun.svg", "Wallrun: velocidad vertical en la pared", "uu/s",
              [("Mirror's Edge", ME, me), ("antes (escala 0.28)", OLD, old), ("ahora (P5)", NEW, new)])

    slide = load("slide")
    seg = segments(slide, SLIDE)[0]
    t0, v0 = seg[0].t, seg[0].v2d
    me = [(s.t - t0, s.v2d) for s in seg]
    dur = seg[-1].t - t0
    old = [(i / 100, v0 - 120 * 2.54 * i / 100) for i in range(0, int(dur * 100))]
    new = [(i / 100, v0 - slide_dec * i / 100) for i in range(0, int(dur * 100))]
    svg_chart(OUT / "slide.svg", "Slide: velocidad horizontal", "uu/s",
              [("Mirror's Edge", ME, me), ("antes", OLD, old), ("ahora (P5)", NEW, new)])


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    rows = measure()
    by = {r["name"]: r["measured"] for r in rows}
    charts(by["Gravedad de Faith"], by["Salto (vz inicial)"], by["Sprint: constante de tiempo"],
           by["Frenado al soltar (τ)"], by["Wallrun: gravedad"], by["Slide: frenado"])
    lines = ["| Valor | Medido en ME | Grabación | Antes | Ahora | Nota |", "|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r['name']} | {r['measured']:.2f} {r['unit']} | `{r['source']}` | {r['before']} | "
                     f"{r['after']} | {r['note']} |")
    table = "\n".join(lines)
    print(table)
    (OUT / "P5_tabla.md").write_text(table + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
