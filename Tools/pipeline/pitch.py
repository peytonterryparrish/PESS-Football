"""High-resolution pitch for match play (replaces the stadium plate's low-res baked grass).
ISS-style: flat mowing stripes across the pitch, fine grass grain, crisp anti-aliased white markings.
Ground-space texture: x in [-X0, X0], y in [-Y0, Y0] metres at PPM pixels per metre; +y is the far touchline,
which is the top row of the image (row 0 = y = +Y0). The game maps it onto the stadium projection.
Also writes per-lighting tints (from the stadium plates' own grass) so sunset / floodlit matches stay in tone.
Run: python Tools/pipeline/pitch.py
"""
import json, os
import numpy as np
from PIL import Image

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(REPO, "GameData", "Pitch")
PPM = 32
X0, Y0 = 57.0, 34.5
HL, HW = 52.5, 34.0
LW = 0.12                                   # line width (m)
LIGHT = np.array([74, 166, 62], float)      # mown stripe colours (sRGB)
DARK = np.array([58, 145, 50], float)
LINE = np.array([238, 240, 232], float)
STRIPE = HL * 2 / 20                        # 20 stripes goal line to goal line


def grid():
    W, H = int(X0 * 2 * PPM), int(Y0 * 2 * PPM)
    xs = (np.arange(W) + 0.5) / PPM - X0
    ys = Y0 - (np.arange(H) + 0.5) / PPM
    return np.meshgrid(xs, ys)


def noise(shape, cell, rng):
    """Smooth value noise: random grid upsampled bilinearly."""
    H, W = shape
    g = rng.standard_normal((H // cell + 2, W // cell + 2)).astype(np.float32)
    im = Image.fromarray(g, mode="F").resize(((W // cell + 2) * cell, (H // cell + 2) * cell), Image.BILINEAR)
    return np.asarray(im)[:H, :W]


def seg_dist(X, Y, a, b):
    ax, ay = a; bx, by = b
    dx, dy = bx - ax, by - ay
    t = np.clip(((X - ax) * dx + (Y - ay) * dy) / (dx * dx + dy * dy), 0, 1)
    return np.hypot(X - (ax + t * dx), Y - (ay + t * dy))


def arc_dist(X, Y, c, r, keep=None):
    d = np.abs(np.hypot(X - c[0], Y - c[1]) - r)
    if keep is not None: d = np.where(keep, d, np.inf)
    return d


def markings(X, Y):
    """Distance (m) from each pixel to the nearest line centre; lines sit inside the field of play."""
    h = LW / 2
    L, Wd = HL - h, HW - h
    D = np.full(X.shape, np.inf, np.float32)
    def m(d): np.minimum(D, d, out=D)
    for s in (-1, 1):
        m(seg_dist(X, Y, (-L, s * Wd), (L, s * Wd)))            # touchlines
        m(seg_dist(X, Y, (s * L, -Wd), (s * L, Wd)))            # goal lines
        # penalty area 16.5 x 40.32, goal area 5.5 x 18.32
        for depth, half in ((16.5, 20.16), (5.5, 9.16)):
            xi = s * (L - depth + h)
            m(seg_dist(X, Y, (xi, -half), (xi, half)))
            m(seg_dist(X, Y, (s * L, half), (xi, half)))
            m(seg_dist(X, Y, (s * L, -half), (xi, -half)))
        # penalty arc (outside the area only), corner arcs
        spot = (s * (HL - 11.0), 0.0)
        m(arc_dist(X, Y, spot, 9.15, keep=(s * X) < (HL - 16.5)))
        for t in (-1, 1):
            c = (s * HL, t * HW)
            m(arc_dist(X, Y, c, 1.0 - h, keep=((s * X) <= HL) & ((t * Y) <= HW)))
    m(seg_dist(X, Y, (0, -Wd), (0, Wd)))                        # halfway line
    m(arc_dist(X, Y, (0, 0), 9.15))                             # centre circle
    # spots (filled discs, given as distance to a 0 m line so they read as solid)
    S = np.full(X.shape, np.inf, np.float32)
    for c in ((0, 0), (HL - 11, 0), (-HL + 11, 0)):
        np.minimum(S, np.maximum(np.hypot(X - c[0], Y - c[1]) - 0.2 + h, 0), out=S)
    return np.minimum(D, S)


def build():
    os.makedirs(OUT, exist_ok=True)
    X, Y = grid()
    rng = np.random.default_rng(1993)
    # mowing stripes: bands across the pitch, soft 3 cm transition
    band = (X + HL) / STRIPE
    f = band - np.floor(band)
    edge = np.clip(np.minimum(f, 1 - f) * STRIPE / 0.03, 0, 1)
    light = (np.floor(band).astype(int) % 2 == 0)
    w = np.where(light, 0.5 + 0.5 * edge, 0.5 - 0.5 * edge)[..., None]
    col = DARK + (LIGHT - DARK) * w
    # grass: broad soft patches, mower grain along each stripe, fine per-pixel blade noise
    patches = noise(X.shape, 96, rng) * 0.025 + noise(X.shape, 24, rng) * 0.015
    grain = np.asarray(Image.fromarray(rng.standard_normal((X.shape[0], X.shape[1] // 6 + 1)).astype(np.float32), "F")
                       .resize((X.shape[1] // 6 * 6 + 6, X.shape[0]), Image.BILINEAR))[:, :X.shape[1]] * 0.018
    blades = rng.standard_normal(X.shape).astype(np.float32) * 0.022
    col = col * (1 + patches + grain + blades)[..., None]
    # markings, anti-aliased by analytic coverage (1 px soft edge)
    D = markings(X, Y)
    cov = np.clip((LW / 2 - D) * PPM + 0.5, 0, 1)[..., None]
    col = col * (1 - cov * 0.94) + LINE * cov * 0.94
    img = np.clip(col, 0, 255).astype(np.uint8)
    Image.fromarray(img, "RGB").save(os.path.join(OUT, "pitch.png"), optimize=True)
    print("pitch.png", img.shape)
    return img


def tints(img):
    """Per-lighting multiplier: each stage's plate grass relative to the daylight plate (linear light)."""
    proj = json.load(open(os.path.join(REPO, "ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Projection.json")))
    (axx, ayx, _), (axy, ayy, _) = proj["world_xyz_to_4k"]
    ox, oy = proj["origin_4k_px"]
    pts = [(x, y) for x in np.linspace(-45, 45, 31) for y in np.linspace(-28, 28, 21) if abs(x) > 1.5 and np.hypot(x, y) > 10]
    means = {}
    for st in ("Daylight", "Sunset", "Floodlit_Night"):
        acc = []
        for club in ("arsenal", "afc-bournemouth", "chelsea", "everton"):
            p = os.path.join(REPO, f"ArtSource/Packs/R08_Final_93_Club_Stadiums_4K/Clubs/{club}/{st}_Stadium_4K.png")
            if not os.path.exists(p): continue
            im = Image.open(p).convert("RGB")
            for x, y in pts:
                acc.append(im.getpixel((int(ox + axx * x + ayx * y), int(oy + axy * x + ayy * y))))
        means[st] = np.mean(np.array(acc, float), 0)
    # sRGB mean ratio, applied as a damped linear multiplier (grass red is near zero, so raw ratios are noisy)
    out = {st: [round(float(v), 4) for v in np.clip((means[st] / means["Daylight"]) ** 1.1, 0.6, 1.5)] for st in means}
    json.dump({"ppm": PPM, "x0": X0, "y0": Y0, "tint": out}, open(os.path.join(OUT, "pitch.json"), "w"), indent=1)
    print("tints", out)


if __name__ == "__main__":
    tints(build())
