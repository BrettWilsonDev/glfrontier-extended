#!/usr/bin/env python3
"""
make_icon.py - draws the GLFrontier icon (a ringed gas giant in a starry
disc, after the Frontier: Elite II logo) and writes every copy the builds use:

    tools/icon/glfrontier.png                        512 px master
    src/glfrontier.ico                               Windows exe (src/glfrontier.rc)
    tools/androidBuild/res/mipmap-*/ic_launcher.png  Android, legacy icon
    tools/androidBuild/res/mipmap-*/ic_launcher_foreground.png
                                                     Android 8+, adaptive icon layer
    vendor/minshell.html                             web favicon (inline data: URI)

The results are committed, so building the game doesn't need this script;
run it only to change the icon. Needs Pillow and numpy (requirements.txt).
"""
import base64
import io
import re
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SS = 1024  # drawing size; every output is scaled down from this

SPACE = np.array([4, 8, 24])
PLANET_BANDS = np.array([
    [250, 226, 176],  # cream
    [226, 150, 72],   # orange
    [178, 92, 44],    # rust
    [236, 186, 120],  # tan
    [150, 70, 36],    # brown
])
RING = np.array([236, 200, 124])
TILT = np.radians(-24)  # ring and bands tilt, clockwise


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def draw(size=SS, disc=True, scale=1.0):
    """RGBA float image. disc: the dark round background; scale: how much
    of the canvas the disc (radius 1) takes."""
    px = 2.0 / size
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float64)
    x = ((xs + 0.5) / size * 2 - 1) / scale
    y = ((ys + 0.5) / size * 2 - 1) / scale
    aa = px / scale * 1.2  # antialiasing width
    rgb = np.zeros((size, size, 3))
    alpha = np.zeros((size, size))

    def over(color, a):
        nonlocal rgb, alpha
        a = np.clip(a, 0, 1)[..., None]
        rgb = rgb * (1 - a) + color * a
        alpha = alpha + (1 - alpha) * a[..., 0]

    r = np.hypot(x, y)
    # --- the disc: deep space, lighter towards the upper left, and stars
    if disc:
        glow = np.clip(1 - np.hypot(x + 0.5, y + 0.6) / 1.8, 0, 1)
        space = SPACE + glow[..., None] * np.array([18, 26, 60])
        over(space, 1 - smoothstep(1 - aa, 1, r))
        rim = smoothstep(0.93, 0.985, r) * (1 - smoothstep(1 - aa, 1, r))
        over(np.array([70, 90, 150]), rim * 0.6)
    rng = np.random.default_rng(1993)
    for _ in range(26):
        while True:
            sx, sy = rng.uniform(-0.88, 0.88, 2)
            if np.hypot(sx, sy) < 0.86 and np.hypot(sx, sy * 2.2) > 1.0:
                break
        sr = rng.uniform(0.008, 0.02)
        d = np.hypot(x - sx, y - sy)
        over(np.array([255, 255, 255]), (1 - smoothstep(sr * 0.3, sr, d)) * rng.uniform(0.6, 1))

    # ring frame: xr along the ring, yr across it (flattened ellipse)
    c, s = np.cos(TILT), np.sin(TILT)
    xr = x * c + y * s
    yr = (-x * s + y * c)
    flat = 0.30
    rr = np.hypot(xr, yr / flat)

    def ring_alpha():
        a = smoothstep(0.66, 0.70, rr) * (1 - smoothstep(0.95, 0.98, rr))
        a *= 1 - 0.85 * (smoothstep(0.815, 0.825, rr) * (1 - smoothstep(0.845, 0.855, rr)))  # gap
        a *= 0.75 + 0.25 * np.sin(rr * 120)  # fine ringlets
        return a * 0.95

    def ring_color():
        shade = 0.8 + 0.35 * np.clip(-xr, -1, 1) * 0.5 + 0.1 * np.sin(rr * 40)
        return RING * shade[..., None]

    # --- back half of the ring (above the planet's centre)
    ra = ring_alpha()
    back = yr < 0
    over(ring_color(), ra * back)

    # --- the planet
    R = 0.52
    pr = r / R
    inside = 1 - smoothstep(1 - aa / R, 1, pr)
    nz = np.sqrt(np.clip(1 - pr ** 2, 0, 1))
    lb = (-x * s + y * c) / R  # latitude, tilted like the ring
    wobble = 0.05 * np.sin((x * c + y * s) / R * 5 + lb * 7)
    t = lb + wobble
    band = (np.sin(t * 7.5) + 0.6 * np.sin(t * 17 + 1.3) + 0.3 * np.sin(t * 31)) * 0.5 + 0.5
    idx = np.clip(band * (len(PLANET_BANDS) - 1), 0, len(PLANET_BANDS) - 1.001)
    i0 = np.floor(idx).astype(int)
    f = (idx - i0)[..., None]
    col = PLANET_BANDS[i0] * (1 - f) + PLANET_BANDS[i0 + 1] * f
    # a storm spot
    spot = np.hypot((x * c + y * s) / R - 0.25, (lb - 0.36) * 1.9)
    col = col * (1 - (1 - smoothstep(0.08, 0.14, spot))[..., None] * 0.35) + \
        np.array([200, 80, 40]) * (1 - smoothstep(0.08, 0.14, spot))[..., None] * 0.35
    # light from the upper left, dark limb
    L = np.array([-0.55, -0.55, 0.63])
    L /= np.linalg.norm(L)
    lam = np.clip((x / R) * L[0] + (y / R) * L[1] + nz * L[2], 0, 1)
    light = 0.12 + 0.95 * lam ** 0.9
    col = col * light[..., None]
    # ring shadow across the planet, just below the ring's front edge
    shadow_band = np.abs(np.hypot(xr, (yr - 0.10) / flat) - 0.8) < 0.15
    col = col * np.where(shadow_band & (yr > 0), 0.55, 1.0)[..., None]
    # thin atmosphere at the lit limb
    haze = smoothstep(0.82, 1.0, pr) * lam
    col = col + np.array([255, 220, 170]) * (haze * 0.25)[..., None]
    over(col, inside)

    # --- front half of the ring
    over(ring_color(), ra * (~back))

    return np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8)


def to_image(arr, size):
    im = Image.fromarray(arr, "RGBA")
    return im if im.size[0] == size else im.resize((size, size), Image.LANCZOS)


def main():
    icon = draw()
    master = to_image(icon, 512)
    master.save(ROOT / "tools" / "icon" / "glfrontier.png", optimize=True)

    # Windows: all the sizes Explorer asks for, 256 stored as PNG
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    to_image(icon, 256).save(ROOT / "src" / "glfrontier.ico", sizes=[(s, s) for s in ico_sizes])

    # Android: legacy icons, and the adaptive foreground (108 dp canvas, the
    # launcher shows the middle 72 dp, so the disc sits inside 66 dp)
    res = ROOT / "tools" / "androidBuild" / "res"
    fg = draw(disc=False, scale=0.62)
    for dpi, mult in [("mdpi", 1), ("hdpi", 1.5), ("xhdpi", 2), ("xxhdpi", 3), ("xxxhdpi", 4)]:
        d = res / f"mipmap-{dpi}"
        d.mkdir(parents=True, exist_ok=True)
        to_image(icon, int(48 * mult)).save(d / "ic_launcher.png", optimize=True)
        to_image(fg, int(108 * mult)).save(d / "ic_launcher_foreground.png", optimize=True)

    # web: favicon inlined into the shell page, so it ships inside index.html
    buf = io.BytesIO()
    to_image(icon, 64).save(buf, "PNG", optimize=True)
    uri = "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode()
    shell = ROOT / "vendor" / "minshell.html"
    html = shell.read_text(encoding="utf-8")
    html, n = re.subn(r'<link rel="(?:shortcut )?icon"[^>]*>',
                      f'<link rel="icon" type="image/png" href="{uri}">', html, count=1)
    if not n:
        raise SystemExit("no <link rel=icon> in vendor/minshell.html")
    shell.write_text(html, encoding="utf-8", newline="")
    print("icon written: png, ico, android mipmaps, web favicon")


if __name__ == "__main__":
    main()
