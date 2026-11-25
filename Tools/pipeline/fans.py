"""v0.7 crowd: the owner's ISS Deluxe supporter sprites, seated in every stadium's own seats.

  python -I Tools/pipeline/fans.py            # all 93 stadiums
  python -I Tools/pipeline/fans.py arsenal

Out:
  GameData/Fans/fans.png + fans.json   40 sprites (20 fans x seated/standing), 64 px cells, the owner's art unchanged
  GameData/Fans/<slug>.json            seat positions in stadium-atlas pixels: [[x, y, fan], ...] (x, y = the hips)

Seats are found from the stadium art itself: the pack bakes the home club's colour into every seat, so seat pixels
are the ones close to the home colour (or its shaded side) outside the pitch, its boards and the track. A jittered
grid one fan wide (players and fans share one scale: 64 px = 1.82 m drawn at half height) is laid over them, and
about four seats in five get a supporter."""
import csv, json, os, sys, random
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
STAD = os.path.join(ROOT, "ArtSource", "Packs", "R08_Final_93_Club_Stadiums_4K")
FANS = os.path.join(ROOT, "ArtSource", "Packs", "ISS_Deluxe_Soccer_Stadium_Fans", "02_ISS_Deluxe_Soccer", "Native_64")
OUT = os.path.join(ROOT, "GameData", "Fans")
# stadium projection (Projection.json): atlas = O + Ax*x + Ay*y + Az*z
O = np.array([1920.0, 1080.0]); AX = np.array([21.889, -1.960]); AY = np.array([-10.298, -8.840])
ATLAS_PER_SPRITE_PX = 1.82 * 0.5 * 22.533 / 64.0      # a 64 px sprite spans the drawn player height (HeightScale 0.5)
STEP_X = 29 * ATLAS_PER_SPRITE_PX * 0.92               # a seated fan is ~29 px wide
STEP_Y = 7.0                                            # about one seat row in the pack's stands

def build_sprites():
    os.makedirs(OUT, exist_ok=True)
    names = []
    for pose in ("seated", "standing"):
        for f in sorted(os.listdir(os.path.join(FANS, pose))):
            if f.endswith(".png"): names.append((pose, f))
    sheet = Image.new("RGBA", (64 * 20, 64 * 2), (0, 0, 0, 0))
    meta = {"cell": 64, "seated_hips": [32, 47], "standing_feet": [32, 63], "fans": 20, "rows": ["seated", "standing"]}
    for pose in ("seated", "standing"):
        fs = sorted(f for p, f in names if p == pose)
        for i, f in enumerate(fs[:20]):
            im = Image.open(os.path.join(FANS, pose, f)).convert("RGBA")
            sheet.paste(im, (i * 64, (0 if pose == "seated" else 1) * 64))
    sheet.save(os.path.join(OUT, "fans.png"))
    json.dump(meta, open(os.path.join(OUT, "fans.json"), "w"))

def seat_mask(img, home_hex):
    a = np.asarray(img.convert("RGB")).astype(np.float32)
    h = np.array([int(home_hex[i:i + 2], 16) for i in (1, 3, 5)], np.float32)
    best = None
    for k in (1.0, 0.8, 0.62, 1.15):
        d = np.sqrt(((a - np.clip(h * k, 0, 255)) ** 2).sum(-1))
        best = d if best is None else np.minimum(best, d)
    m = best < 34
    # not the pitch, its boards or the track: world rectangle x in [-59, 59], y in [-37, 35.6] projected to the atlas
    H, W = m.shape
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    det = AX[0] * AY[1] - AX[1] * AY[0]
    dx, dy = xx - O[0], yy - O[1]
    gx = (dx * AY[1] - dy * AY[0]) / det
    gy = (AX[0] * dy - AX[1] * dx) / det
    pitch = (np.abs(gx) < 59.2) & (gy > -37.0) & (gy < 35.6)
    # the pack's 72-row left-end stand draws its seats as thin home-colour stripes on grey steps: inside its footprint
    # any stripe nearby counts (its plane sits a little below the pitch, hence the generous depth range)
    global LEFT_STAND
    LEFT_STAND = (gx < -60.4) & (gx > -100) & (gy > -36.5) & (gy < 33.8)
    return m & ~pitch

def place(slug, home_hex, rng):
    img = Image.open(os.path.join(STAD, "Clubs", slug, "Daylight_Stadium_4K.png"))
    m = seat_mask(img, home_hex)
    H, W = m.shape
    # local density: a seat point needs seat colour around it (not a lone pixel of a board or a roof)
    from numpy.lib.stride_tricks import sliding_window_view
    pad = np.pad(m, 3)
    dens = sliding_window_view(pad, (7, 7)).mean(axis=(-1, -2))
    pts = []
    row = 0
    y = 2.0
    while y < H - 2:
        off = (row % 2) * STEP_X * 0.5
        x = 2.0 + off
        while x < W - 2:
            jx, jy = x + rng.uniform(-1.2, 1.2), y + rng.uniform(-0.8, 0.8)
            ix, iy = int(jx), int(jy)
            if (dens[iy, ix] > 0.42 or (LEFT_STAND[iy, ix] and dens[iy, ix] > 0.04)) and rng.random() < 0.82:
                pts.append([ix, iy, rng.randrange(20)])
            x += STEP_X
        y += STEP_Y; row += 1
    pts.sort(key=lambda p: p[1])           # back rows first
    json.dump({"slug": slug, "fans": pts}, open(os.path.join(OUT, slug + ".json"), "w"), separators=(",", ":"))
    return len(pts)

def main():
    build_sprites()
    rows = {r["source_slug"]: r["home_primary_hex"] for r in csv.DictReader(open(os.path.join(STAD, "Manifest.csv"), encoding="utf-8"))}
    slugs = sys.argv[1:] or sorted(rows)
    for s in slugs:
        n = place(s, rows[s], random.Random(__import__("zlib").crc32(s.encode())))
        print(s, n)

if __name__ == "__main__":
    main()
