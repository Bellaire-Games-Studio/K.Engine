#!/usr/bin/env python3
"""
Generate the engine's built-in, seamless (GL_REPEAT-tileable) textures.

These are procedurally generated, so they are CC0 / public-domain by construction
(no third-party licensing) and tiny. Output: Assets/Textures/*.png (256x256 RGB),
which the web build already preloads via the Assets/ --preload-file.

    python3 scripts/gen_textures.py

Re-run to regenerate. Pure standard library (zlib for PNG); no PIL/numpy needed.
"""
import math
import os
import struct
import zlib

SIZE = 256
OUT = os.path.join(os.path.dirname(__file__), "..", "Assets", "Textures")


# ---- PNG writer (8-bit RGB) -------------------------------------------------
def write_png(path, w, h, rgb):
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)  # filter type 0 (none)
        raw.extend(rgb[y * stride:(y + 1) * stride])

    def chunk(typ, data):
        return (struct.pack(">I", len(data)) + typ + data +
                struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


# ---- tileable value noise ---------------------------------------------------
def h2(x, y, seed):
    n = (x * 374761393 + y * 668265263 + seed * 2246822519) & 0xffffffff
    n = ((n ^ (n >> 13)) * 1274126177) & 0xffffffff
    n ^= (n >> 16)
    return (n & 0xffffffff) / 4294967295.0


def smooth(t):
    return t * t * (3.0 - 2.0 * t)


def vnoise(x, y, period, seed):
    x0, y0 = math.floor(x), math.floor(y)
    fx, fy = x - x0, y - y0
    a = h2(x0 % period, y0 % period, seed)
    b = h2((x0 + 1) % period, y0 % period, seed)
    c = h2(x0 % period, (y0 + 1) % period, seed)
    d = h2((x0 + 1) % period, (y0 + 1) % period, seed)
    ux, uy = smooth(fx), smooth(fy)
    return (a * (1 - ux) * (1 - uy) + b * ux * (1 - uy)
            + c * (1 - ux) * uy + d * ux * uy)


def fbm(u, v, base, octaves, seed):
    val = tot = 0.0
    amp = 0.5
    freq = base
    for o in range(octaves):
        val += amp * vnoise(u * freq, v * freq, freq, seed + o * 17)
        tot += amp
        amp *= 0.5
        freq *= 2
    return val / tot


def clamp8(x):
    return 0 if x < 0 else 255 if x > 255 else int(x)


def lerp(a, b, t):
    return a + (b - a) * t


# ---- generators (each returns a w*h*3 bytearray) ----------------------------
def gen(fn):
    px = bytearray(SIZE * SIZE * 3)
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b = fn(x, y)
            i = (y * SIZE + x) * 3
            px[i] = clamp8(r); px[i + 1] = clamp8(g); px[i + 2] = clamp8(b)
    return px


def uvgrid(x, y):
    cell = 32
    on_line = (x % cell < 2) or (y % cell < 2)
    checker = ((x // cell) + (y // cell)) & 1
    if on_line:
        return (60, 200, 90)
    base = 70 if checker else 110
    return (base, base, base + 8)


def brick(x, y):
    bw, bh, mortar = 64, 32, 4
    row = y // bh
    off = bw // 2 if (row & 1) else 0
    bx = (x + off) % bw
    by = y % bh
    if bx < mortar or by < mortar:
        return (96, 92, 88)  # mortar
    bid_x = (x + off) // bw
    t = h2(bid_x, row, 7)
    n = fbm(x / SIZE, y / SIZE, 16, 3, 3)
    base = 150 + t * 40 + (n - 0.5) * 30
    return (base, base * 0.42 + 20, base * 0.32 + 12)


def planks(x, y):
    pw, gap = 64, 3
    px = x % pw
    pid = x // pw
    if px < gap:
        return (60, 40, 26)  # gap shadow
    grain = 0.5 + 0.5 * math.sin((y / SIZE) * 2 * math.pi * 6 + pid * 1.7
                                 + fbm(x / SIZE, y / SIZE, 8, 3, pid) * 4)
    n = fbm(x / SIZE * 2, y / SIZE * 0.3, 16, 4, pid * 5)
    v = 0.55 + 0.30 * grain + (n - 0.5) * 0.25
    return (150 * v, 96 * v, 56 * v)


def _features(seed):
    cells = 8
    pts = {}
    for cy in range(cells):
        for cx in range(cells):
            jx = h2(cx, cy, seed)
            jy = h2(cx, cy, seed + 99)
            pts[(cx, cy)] = ((cx + jx) / cells, (cy + jy) / cells)
    return pts, cells


_COBBLE_PTS, _COBBLE_CELLS = _features(13)


def cobble(x, y):
    u, v = x / SIZE, y / SIZE
    cells = _COBBLE_CELLS
    cx, cy = int(u * cells), int(v * cells)
    d1 = d2 = 9.0
    nearest = (cx, cy)
    for oy in range(-1, 2):
        for ox in range(-1, 2):
            key = ((cx + ox) % cells, (cy + oy) % cells)
            fx, fy = _COBBLE_PTS[key]
            dx = u - (fx + ox * (1 if cx + ox >= cells or cx + ox < 0 else 0))
            # toroidal distance
            dx = abs(u - fx); dx = min(dx, 1 - dx)
            dy = abs(v - fy); dy = min(dy, 1 - dy)
            d = dx * dx + dy * dy
            if d < d1:
                d2, d1, nearest = d1, d, key
            elif d < d2:
                d2 = d
    edge = math.sqrt(d2) - math.sqrt(d1)
    tone = 0.55 + 0.35 * h2(nearest[0], nearest[1], 21)
    n = fbm(u * 3, v * 3, 16, 3, 4)
    g = (tone + (n - 0.5) * 0.2) * 150 + 40
    if edge < 0.015:
        g *= 0.35  # dark gap between stones
    return (g, g * 0.98, g * 0.92)


def tiles(x, y):
    t, grout = 32, 3
    if (x % t) < grout or (y % t) < grout:
        return (70, 72, 78)
    tid = h2(x // t, y // t, 5)
    n = fbm(x / SIZE * 2, y / SIZE * 2, 16, 3, 8)
    base = 180 + tid * 50 + (n - 0.5) * 20
    return (base * 0.85, base * 0.9, base)


def concrete(x, y):
    u, v = x / SIZE, y / SIZE
    n = fbm(u, v, 8, 5, 2)
    stain = fbm(u * 0.5, v * 0.5, 4, 3, 30)
    g = 150 + (n - 0.5) * 70 - (1 - stain) * 25
    return (g, g, g * 1.02)


def grass(x, y):
    u, v = x / SIZE, y / SIZE
    patch = fbm(u, v, 6, 4, 11)
    blade = fbm(u * 6, v * 1.5, 24, 3, 12)
    g = 0.45 + 0.4 * patch + (blade - 0.5) * 0.3
    return (lerp(40, 120, g), lerp(80, 165, g), lerp(30, 70, g))


def metal(x, y):
    u, v = x / SIZE, y / SIZE
    brush = fbm(u * 8, v * 0.4, 32, 3, 9)  # stretched horizontally
    base = 120 + (brush - 0.5) * 50
    # panel seams every 128 px + rivets near them
    seam = (x % 128 < 2) or (y % 128 < 2)
    if seam:
        base *= 0.6
    rx, ry = x % 128, y % 128
    for (ox, oy) in ((10, 10), (118, 10), (10, 118), (118, 118)):
        if (rx - ox) ** 2 + (ry - oy) ** 2 < 9:
            base *= 0.5
    return (base, base * 1.02, base * 1.08)


TEXTURES = {
    "uvgrid": uvgrid,
    "brick": brick,
    "planks": planks,
    "cobble": cobble,
    "tiles": tiles,
    "concrete": concrete,
    "grass": grass,
    "metal": metal,
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, fn in TEXTURES.items():
        write_png(os.path.join(OUT, name + ".png"), SIZE, SIZE, gen(fn))
        print("wrote", name + ".png")


if __name__ == "__main__":
    main()
