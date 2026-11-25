"""v0.8: where the hands are in every ball-in-hands frame, so the game draws the ball IN the keeper's gloves.

  python -I Tools/pipeline/hand_anchors.py            # all club sets
  python -I Tools/pipeline/hand_anchors.py arsenal    # one club; --sheet writes a check sheet to ArtWork/audit_v08/

For each GameData/Sprites/Clubs/<slug>_<kit>.json it writes <slug>_<kit>_hands.json: {"dir/pose": [x, y]} in cell pixels
(pivot 48,88), already mirrored for the flipped directions.
- PixelLab frames: measured from the silhouette (PixelLab does not follow the guide skeleton closely enough to use its
  wrists): the top of raised arms, or in front of the chest on the facing side.
- The owner's flat dives (gk_fly_*, gk_smother, gk_fly_take, no skeleton): the leading end of the silhouette.
The ball sprite is ~12 cell pixels across, so the anchor is the ball's centre."""
import glob, json, os, sys
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CLUBS = os.path.join(ROOT, "GameData", "Sprites", "Clubs")
RAW = os.path.join(ROOT, "ArtWork", "raw_clubs")
CELL = 96
HAND_POSES = {"gk_hold_ball", "gk_catch_chest", "gk_catch_high", "gk_scoop", "gk_throw_back", "gk_throw_release", "gk_punt_drop",
              "gk_stretch_R", "gk_stretch_L", "gk_dive_low_R1", "gk_dive_low_L1", "gk_dive_high_R1", "gk_dive_high_L1",
              "throwin_back", "throwin_release", "getup_kneel"}
FLY = {"gk_fly_low_R": +1, "gk_fly_high_R": +1, "gk_fly_low_L": -1, "gk_fly_high_L": -1, "gk_fly_take": 0, "gk_smother": -1}

def kps_for(raw_dir, pose):
    for f in sorted(glob.glob(os.path.join(raw_dir, "_kps_b*.json"))):
        d = json.load(open(f))
        if pose in d["batch"]:
            return d["kps"][d["batch"].index(pose)]
    return None

def opaque_near(cell, x, y, r=7):
    """nearest opaque pixel to (x, y) (the gloves are at the end of the arm), else (x, y)"""
    px = cell.load(); best = None
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            X, Y = int(round(x)) + dx, int(round(y)) + dy
            if 0 <= X < CELL and 0 <= Y < CELL and px[X, Y][3] and (best is None or dx * dx + dy * dy < best[0]):
                best = (dx * dx + dy * dy, X, Y)
    return (best[1], best[2]) if best else (x, y)

OVERHEAD = {"gk_catch_high", "gk_dive_high_R1", "gk_dive_high_L1", "gk_stretch_R", "gk_stretch_L", "throwin_back", "throwin_release", "gk_throw_back"}

def silhouette_anchor(cell, pose, dr):
    """PixelLab's frames don't follow the guide skeleton closely, so the hands are found from the silhouette itself:
    overhead poses -> the top of the raised arms; chest holds -> in front of the chest (toward the facing side);
    the scoop -> low in front."""
    px = cell.load(); pts = [(x, y) for y in range(CELL) for x in range(CELL) if px[x, y][3]]
    if not pts: return None
    top = min(p[1] for p in pts); bot = max(p[1] for p in pts); h = bot - top + 1
    side = 1 if "east" in dr else -1 if "west" in dr else 0
    if pose in OVERHEAD:
        band = [p for p in pts if p[1] <= top + 4]
        return (sum(p[0] for p in band) / len(band), top + 4)
    if pose == "gk_scoop" and side != 0:      # bent over, arms reaching forward: the front tip of the silhouette
        xe = max(p[0] for p in pts) if side > 0 else min(p[0] for p in pts)
        tip = [p for p in pts if abs(p[0] - xe) <= 3]
        return (xe - side * 4, sum(p[1] for p in tip) / len(tip))
    y = top + h * (0.62 if pose == "gk_scoop" else 0.36)
    band = [p for p in pts if abs(p[1] - y) <= 3]
    if side == 0 or not band:
        xs = [p[0] for p in band] or [48]
        return ((min(xs) + max(xs)) / 2, y)
    xe = max(p[0] for p in band) if side > 0 else min(p[0] for p in band)
    return (xe - side * 4, y)

def fly_anchor(cell, lead):
    px = cell.load(); pts = [(x, y) for y in range(CELL) for x in range(CELL) if px[x, y][3]]
    if not pts: return None
    if lead == 0:   # take-off: hands above the head
        y0 = min(p[1] for p in pts); top = [p for p in pts if p[1] <= y0 + 3]
        return (sum(p[0] for p in top) / len(top), y0 + 3)
    xe = max(p[0] for p in pts) if lead > 0 else min(p[0] for p in pts)
    end = [p for p in pts if abs(p[0] - xe) <= 4]
    return (xe - lead * 2.5, sum(p[1] for p in end) / len(end))

def anchors_for(jpath):
    d = json.load(open(jpath)); slug, _, kit = d["id"].rpartition("_")
    rawc = glob.glob(os.path.join(RAW, slug, f"{kit}_{d.get('player', '*')}"))
    at = Image.open(os.path.join(ROOT, d["atlas"])).convert("RGBA")
    out = {}
    for key, (c, r, flip) in d["frames"].items():
        dr, pose = key.split("/")
        if pose not in HAND_POSES and pose not in FLY: continue
        cell = at.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
        if flip: cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
        if not cell.getbbox(): continue
        if pose in FLY:
            a = fly_anchor(cell, FLY[pose])
        else:
            a = silhouette_anchor(cell, pose, dr)
        if a: out[key] = [round(a[0], 1), round(a[1], 1)]
    return d, at, out

def sheet(d, at, out, path):
    keys = sorted(out); img = Image.new("RGBA", (CELL * 12, CELL * ((len(keys) + 11) // 12)), (60, 120, 50, 255))
    dr = ImageDraw.Draw(img)
    for i, k in enumerate(keys):
        c, r, flip = d["frames"][k]; cell = at.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
        if flip: cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
        X, Y = (i % 12) * CELL, (i // 12) * CELL; img.alpha_composite(cell, (X, Y))
        x, y = out[k]; dr.ellipse([X + x - 6, Y + y - 6, X + x + 6, Y + y + 6], outline=(255, 255, 255, 255))
    img.resize((img.width * 2, img.height * 2), Image.NEAREST).save(path)

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    files = sorted(glob.glob(os.path.join(CLUBS, "*.json"))) + sorted(glob.glob(os.path.join(ROOT, "GameData", "Sprites", "B*.json")))   # + v0.8 base bodies
    files = [f for f in files if not f.endswith("_hands.json") and (not args or any(os.path.basename(f).startswith(a + "_") for a in args))]
    n = 0
    for f in files:
        d, at, out = anchors_for(f)
        if not out: continue
        json.dump(out, open(f[:-5] + "_hands.json", "w"), indent=0); n += 1
        if "--sheet" in sys.argv:
            os.makedirs(os.path.join(ROOT, "ArtWork", "audit_v08"), exist_ok=True)
            sheet(d, at, out, os.path.join(ROOT, "ArtWork", "audit_v08", f"hands_{d['id']}.png"))
    print(f"hand anchors for {n} sets")

if __name__ == "__main__":
    main()
