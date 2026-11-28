"""v0.10: where the ball sits in the hands on every frame of the approved animations (keeper catches, dives, get-ups,
throws, punts; the outfield throw-in), so a held ball is drawn in the gloves and never floats.

  python -I Tools/pipeline/anim_hands.py            # NK1 + N01 (and any other set with approved anims)
  python -I Tools/pipeline/anim_hands.py NK1 --sheet  # one set; --sheet writes ArtWork/audit_v010/anim_hands_<id>.png

Run it after any atlas rebuild (build_anim_atlas.py). It merges into GameData/Sprites/<id>_hands.json: entries for
"<dir>/<anim>_fNN" frames are rewritten, every other entry (the v0.8 stills) is kept. Anchors are cell pixels of the
frame as stored in the atlas cell, already mirrored for flipped frames (the game mirrors them again for a mirrored dive).

Keeper frames: the gloves are the light-grey pixels (125..235, near-neutral). Two gloves close together hold the ball
between them (their midpoint, nudged out from the body by a ball radius); far apart, the glove furthest from the body
leads. No glove visible (hugged to the chest, or behind him): in front of the chest from the silhouette.
Outfield throw-in frames before the release: the top of the raised arms."""
import glob, json, os, sys
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SPR = os.path.join(ROOT, "GameData", "Sprites")
CELL = 96
BALL_R = 3.0          # the drawn ball's radius in cell pixels (FPFMatch::Draw: ~6 px across)
KEEPER_ANIMS = ("gk_",)
THROW_ANIMS = ("throwin",)

def is_glove(p):
    r, g, b, a = p
    return a > 0 and max(r, g, b) - min(r, g, b) < 26 and 120 <= max(r, g, b) <= 240

def clusters(cell):
    px = cell.load(); seen = set(); out = []
    pts = {(x, y) for y in range(CELL) for x in range(CELL) if is_glove(px[x, y])}
    for s in pts:
        if s in seen: continue
        st = [s]; seen.add(s); c = []
        while st:
            x, y = st.pop(); c.append((x, y))
            for dx in (-2, -1, 0, 1, 2):
                for dy in (-2, -1, 0, 1, 2):
                    q = (x + dx, y + dy)
                    if q in pts and q not in seen: seen.add(q); st.append(q)
        if len(c) >= 2: out.append(c)
    return out

def centroid(ps):
    return (sum(p[0] for p in ps) / len(ps), sum(p[1] for p in ps) / len(ps))

def silhouette(cell):
    px = cell.load()
    return [(x, y) for y in range(CELL) for x in range(CELL) if px[x, y][3]]

def chest(pts, side):
    top = min(p[1] for p in pts); bot = max(p[1] for p in pts); h = bot - top + 1
    y = top + h * 0.36
    band = [p for p in pts if abs(p[1] - y) <= 3] or pts
    xs = [p[0] for p in band]
    if side == 0: return ((min(xs) + max(xs)) / 2, y + 2)
    xe = max(xs) if side > 0 else min(xs)
    return (xe - side * 3, y + 2)

def keeper_anchor(cell, dr):
    pts = silhouette(cell)
    if not pts: return None
    body = centroid(pts)
    top = min(p[1] for p in pts); bot = max(p[1] for p in pts); xs = [p[0] for p in pts]
    upright = (bot - top) > (max(xs) - min(xs)) * 0.9
    # standing, nothing grey below the hips is a glove (boot studs, sock tape)
    cs = [c for c in clusters(cell) if not (upright and centroid(c)[1] > top + (bot - top) * 0.62)]
    cs = sorted(cs, key=len, reverse=True)[:2]
    side = 1 if ("east" in dr or dr in ("a030", "a060", "a120", "a150")) else -1 if ("west" in dr or dr in ("a210", "a240", "a300", "a330")) else 0
    if not cs: return chest(pts, side)
    cc = [centroid(c) for c in cs]
    d2 = (cc[0][0] - cc[1][0]) ** 2 + (cc[0][1] - cc[1][1]) ** 2 if len(cc) == 2 else 1e9
    overhead = len(cc) == 2 and abs(cc[0][1] - cc[1][1]) <= 6 and max(cc[0][1], cc[1][1]) < body[1] - 8 and d2 <= 28 ** 2
    if d2 <= 16 ** 2 or overhead:   # both hands on it (at the chest, or overhead)
        m = ((cc[0][0] + cc[1][0]) / 2, (cc[0][1] + cc[1][1]) / 2)
    else:
        # one glove, or two apart (a dive's reach): the one furthest from the body leads
        m = max(cc, key=lambda c: (c[0] - body[0]) ** 2 + (c[1] - body[1]) ** 2)
    # the ball sits in the palms: half a ball out from the body along body -> gloves
    dx, dy = m[0] - body[0], m[1] - body[1]; L = max(1e-3, (dx * dx + dy * dy) ** 0.5)
    k = min(BALL_R * 0.6, L) / L
    return (m[0] + dx * k, m[1] + dy * k)

def throw_anchor(cell):
    pts = silhouette(cell)
    if not pts: return None
    top = min(p[1] for p in pts); band = [p for p in pts if p[1] <= top + 3]
    return (sum(p[0] for p in band) / len(band), top + 2)

def anim_of(pose):
    a, _, n = pose.rpartition("_f")
    return a if n.isdigit() else None

def run(sid, sheet=False):
    jp = os.path.join(SPR, sid + ".json"); d = json.load(open(jp))
    anims = d.get("anims", {})
    if not anims: return 0
    at = Image.open(os.path.join(ROOT, d["atlas"])).convert("RGBA")
    hp = os.path.join(SPR, sid + "_hands.json")
    hands = json.load(open(hp)) if os.path.exists(hp) else {}
    hands = {k: v for k, v in hands.items() if not anim_of(k.split("/")[1])}
    n = 0; drawn = []
    for key, (c, r, flip) in d["frames"].items():
        dr, pose = key.split("/"); a = anim_of(pose)
        if not a or a not in anims: continue
        keeper = a.startswith(KEEPER_ANIMS)
        throw = a.startswith(THROW_ANIMS) and int(pose.rpartition("_f")[2]) <= max(0, anims[a].get("ev", 0))
        if not keeper and not throw: continue
        cell = at.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
        if flip: cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
        if not cell.getbbox(): continue
        an = keeper_anchor(cell, dr) if keeper else throw_anchor(cell)
        if an:
            hands[key] = [round(an[0], 1), round(an[1], 1)]; n += 1
            drawn.append((key, cell, an))
    json.dump(hands, open(hp, "w"), indent=0)
    if sheet and drawn:
        drawn.sort(key=lambda t: t[0])
        img = Image.new("RGBA", (CELL * 16, CELL * ((len(drawn) + 15) // 16)), (60, 120, 50, 255)); dw = ImageDraw.Draw(img)
        for i, (k, cell, (x, y)) in enumerate(drawn):
            X, Y = (i % 16) * CELL, (i // 16) * CELL; img.alpha_composite(cell, (X, Y))
            dw.ellipse([X + x - BALL_R, Y + y - BALL_R, X + x + BALL_R, Y + y + BALL_R], fill=(255, 255, 255, 200), outline=(255, 0, 0, 255))
        os.makedirs(os.path.join(ROOT, "ArtWork", "audit_v010"), exist_ok=True)
        img.resize((img.width * 2, img.height * 2), Image.NEAREST).save(os.path.join(ROOT, "ArtWork", "audit_v010", f"anim_hands_{sid}.png"))
    return n

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    ids = args or [os.path.basename(f)[:-5] for f in sorted(glob.glob(os.path.join(SPR, "*.json"))) if not f.endswith("_hands.json")]
    for sid in ids:
        n = run(sid, "--sheet" in sys.argv)
        if n: print(f"{sid}: {n} animation hand anchors")

if __name__ == "__main__":
    main()
