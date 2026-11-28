"""v0.10 audit fixes applied to the SOURCE frames (ArtWork/raw_12/<body>/<dir>/<anim>/fNN.png), the PixelLab artefacts
that ArtWork/audit_v100 found by eye (Tools/pipeline/audit_anim.py sheets). Originals are copied once to
ArtWork/audit_v100/raw_backup/ before the first write, so the script is re-runnable from the backups (it always starts
from the backup, never from an already-fixed frame).

  python -I Tools/pipeline/fix_raw12.py [--dry]      # then rebuild: build_base_atlas.py + build_anim_atlas.py
  python -I Tools/pipeline/fix_raw12.py --only=NK1/a150 --refresh
        only the ops on that body/direction; --refresh first re-copies their backups from the current takes (after an
        animation was regenerated: otherwise the fix would restore the old take). v0.10 bug hunt: the round-3 NK1 a150
        takes were written over the fixed frames, so the keeper's blade and stick were back in the game.

Ops:
  stalk  the owner's "long brown streak above their head" in N01 a060 run/walk/sprint: PixelLab grew a thin skin/brown
         stalk out of the back of the head with a cap on top (every frame of the three cycles; a300 is its mirror).
         Everything above the real hairline goes, the stub beside the head too, and the hairline row is re-inked.
  idle   NK1 a150 gk_ready: the take holds a grey blade and a brown-red stick above his head in every frame (the
         keeper's NE/N streak). No frame is clean, so the cycle becomes the a150 turn (rot12_a150) held.
  hold   the first frames of NK1 a150 catch/high/punch/scoop start from that broken ready pose: they hold the first
         clean frame instead.
  unball a white ball PixelLab left above/around the head (N01 north chest): light grey/white pixels above the shoulders.
  head   a head drawn as a thin hair hook / hollow ring (N01 a150 chest, a120 celebrate_05/07, north chest after the
         ball): the head is replaced by the same take's nearest good head, aligned on the shirt's neckline.
"""
import os, shutil, sys
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RAW12 = os.path.join(ROOT, "ArtWork", "raw_12"); RAWN = os.path.join(ROOT, "ArtWork", "raw_base_n")
BACK = os.path.join(ROOT, "ArtWork", "audit_v100", "raw_backup")

OPS = [
    ("stalk", "N01", "a060", "run", None), ("stalk", "N01", "a060", "walk", None), ("stalk", "N01", "a060", "sprint", None),
    ("idle", "NK1", "a150", "gk_ready", None),
    # v0.10 bug hunt: the round-3 a150 takes (written over the fixed frames) carry the blade a frame longer in the scoop
    # (f02) and a grey blob beside the body in high f02-f03 and punch f02; held frames picked again on those takes
    ("hold", "NK1", "a150", "gk_catch", (range(0, 5), 5)),
    ("hold", "NK1", "a150", "gk_high", (range(0, 4), 4)),
    ("hold", "NK1", "a150", "gk_punch", (range(0, 3), 3)),
    ("hold", "NK1", "a150", "gk_scoop", (range(0, 3), 3)),
    ("unball", "N01", "north", "chest", [2, 3]),
    ("head", "N01", "north", "chest", ([2, 3], 1)),
    ("head", "N01", "a150", "chest", ([2, 3, 4, 5], 1)),
    ("head", "N01", "a120", "celebrate_05", (list(range(3, 13)), 2)),
    ("head", "N01", "a120", "celebrate_07", ([3], 2)),
    ("grass", "NK1", "north", "gk_scoop", [4, 5, 6, 7, 8]),
    ("box", "NK1", "a030", "gk_scoop", (3, (56, 44, 67, 55), "all")),       # a ball held beside the gloves (keyed as shirt)
    ("box", "NK1", "a060", "gk_catch", (4, (38, 28, 46, 35), "all")),       # a ball above his head (keyed as shirt)
    ("box", "NK1", "a030", "gk_high", (3, (44, 12, 54, 21), "all")),        # a white ball floating over the gloves
    ("box", "N01", "south", "dribble", (1, (41, 82, 48, 89), "all")),       # the baked ball at his foot (the game draws its own)
    ("box", "N01", "south", "dribble", (2, (40, 81, 48, 88), "all")),
    ("box", "N01", "south", "dribble", (3, (42, 80, 49, 88), "all")),
    ("box", "N01", "a030", "dribble", (0, (33, 83, 39, 86), "all")),
    ("box", "N01", "a030", "dribble", (0, (42, 83, 44, 88), "all")),
]
# the older 8-direction poses (ArtWork/raw_base_n/<body>/<dir>/t0/<pose>.png, still the game's fallbacks): baked balls
BASE_OPS = [
    ("N01", "south/t0/header", (0, 0, 95, 11), "all"),                 # ball crumbs above the head
    ("N01", "south/t0/pass_strike", (24, 81, 31, 88), "all"),          # the ball at the kicking foot
    ("N01", "south/t0/shot_plant", (36, 60, 46, 64), "light"),         # ball crumbs beside the planted leg
    ("N01", "south-east/t0/stumble", (55, 83, 64, 92), "nondark"),     # the ball under his foot (the boot is dark)
]

def path(b, d, a, i): return os.path.join(RAW12, b, d, a, f"f{i:02d}.png")

def original(p):
    """the untouched source (backed up on first use)"""
    bk = os.path.join(BACK, os.path.relpath(p, RAW12))
    if not os.path.exists(bk):
        os.makedirs(os.path.dirname(bk), exist_ok=True); shutil.copy2(p, bk)
    return np.array(Image.open(bk).convert("RGBA"))

def frames(b, d, a):
    import glob
    return sorted(int(os.path.basename(f)[1:3]) for f in glob.glob(os.path.join(RAW12, b, d, a, "f*.png")))

def hsv(a):
    import colorsys
    f = a[..., :3].astype(float) / 255
    mx = f.max(-1); mn = f.min(-1); s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0)
    return s, mx

def dark(a): return (a[..., 3] >= 128) & (a[..., :3].max(-1) < 72)

def shirt(a):
    """template-blue shirt pixels (the raw frames still wear the template kit)"""
    r, g, b = [a[..., i].astype(int) for i in range(3)]
    return (a[..., 3] >= 128) & (b > r + 40) & (b > g + 25)

def fix_stalk(a):
    op = a[..., 3] >= 128
    top = np.nonzero(op.any(1))[0].min()
    w = [op[y].sum() for y in range(top, top + 16)]
    thin = top + int(np.argmin(w[3:])) + 3                 # the stalk's narrowest row, between the cap and the head
    d = dark(a); hair = None
    for y in range(thin + 1, top + 30):
        run = best = 0
        for v in d[y]: run = run + 1 if v else 0; best = max(best, run)
        if best >= 3 or op[y].sum() >= 7: hair = y; break
    ref = np.nonzero(op[hair + 4])[0]; xl, xr = ref.min(), ref.max()
    b = a.copy(); b[:hair] = 0
    for y in range(hair, hair + 4):
        b[y, :xl] = 0; b[y, xr + 1:] = 0
    # the hairline row: whatever the stalk left there (light, blue, red) becomes hair; reddish leftovers below go
    hc = np.array([45, 30, 29, 255], np.uint8)
    for x in range(96):
        if b[hair, x, 3] >= 128 and not dark(b[hair:hair + 1, x:x + 1])[0, 0]: b[hair, x] = hc
    s, v = hsv(b)
    r, g, bb = [b[..., i].astype(int) for i in range(3)]
    red = (b[..., 3] >= 128) & (s > 0.6) & (r > g + 60) & (r > bb + 60)
    red[hair + 3:] = False
    b[red] = hc
    edge = b[hair + 1]
    if edge[xl, 3] >= 128 and not dark(b[hair + 1:hair + 2, xl:xl + 1])[0, 0] and shirt(b[hair + 1:hair + 2, xl:xl + 1])[0, 0]: b[hair + 1, xl] = hc
    return b

def neck(a):
    sh = shirt(a); rows = [y for y in range(a.shape[0]) if sh[y].sum() >= 4]
    y0 = rows[0]; xs = np.nonzero(sh[y0] | sh[y0 + 1])[0]
    return y0, int(round(xs.mean()))

def fix_head(dst, src, half=7, below=3, patch=0):
    """dst's head and collar (everything above the neckline + `below` rows, within +-half of its centre) replaced by
    src's, aligned on the neckline (the first shirt rows) and its centre"""
    yd, xd = neck(dst); ys, xs = neck(src)
    out = dst.copy()
    x0, x1 = max(0, xd - half), min(96, xd + half + 1)
    out[:yd + below, x0:x1] = 0
    for y in range(0, ys + below):
        for x in range(max(0, xs - half), min(96, xs + half + 1)):
            if src[y, x, 3] >= 128:
                ty, tx = y + yd - ys, x + xd - xs
                if 0 <= ty < 96 and x0 <= tx < x1: out[ty, tx] = src[y, x]
    # a broken head that reached into the shirt (a hollow ring): non-shirt pixels just under the collar take the shirt
    # colour of their row
    for y in range(yd + below, yd + below + patch):
        row = out[y]; sh = shirt(out[y:y + 1])[0]
        if not sh.any(): continue
        cols, cnt = np.unique(row[sh][:, :3], axis=0, return_counts=True); c = cols[np.argmax(cnt)]
        for x in range(xd - 4, xd + 5):
            if row[x, 3] >= 128 and not sh[x]: out[y, x, :3] = c
    return out

def unball(a):
    yd, _ = neck(a)
    s, v = hsv(a)
    m = (a[..., 3] >= 128) & (s < 0.2) & (v > 0.62)
    m[yd + 3:] = False
    b = a.copy(); b[m] = 0
    return b

def box(a, bx, mode):
    x0, y0, x1, y1 = bx; b = a.copy(); s, v = hsv(a)
    sel = np.zeros(a.shape[:2], bool); sel[y0:y1 + 1, x0:x1 + 1] = True
    if mode == "light": sel &= (s < 0.3) & (v > 0.6)
    if mode == "nondark": sel &= ~dark(a)
    b[sel] = 0
    return b

def grass(a):
    """a ground squiggle PixelLab drew in the template green (it became shorts): green pixels in the lower third that a
    3x3 opening of the green removes (the shorts are solid, the squiggle is 1-2 px)"""
    from scipy import ndimage as ND
    r, g, bb = [a[..., i].astype(int) for i in range(3)]
    gr = (a[..., 3] >= 128) & (g > r + 40) & (g > bb + 20)
    thin = gr & ~ND.binary_opening(gr, np.ones((3, 3), bool)); thin[:64] = False
    b = a.copy(); b[thin] = 0
    return b

def run(dry=False, only=None, refresh=False):
    done = []
    ops = [o for o in OPS if not only or f"{o[1]}/{o[2]}" == only]
    if refresh:
        import glob
        for _, b, d, an, _ in ops:
            for f in glob.glob(os.path.join(RAW12, b, d, an, "f*.png")):
                bk = os.path.join(BACK, os.path.relpath(f, RAW12))
                if not dry: os.makedirs(os.path.dirname(bk), exist_ok=True); shutil.copy2(f, bk)
    cache = {}
    def get(b, d, an, i):
        k = (b, d, an, i)
        if k not in cache: cache[k] = original(path(b, d, an, i))
        return cache[k]
    out = {}
    for op, b, d, an, arg in ops:
        idx = frames(b, d, an)
        cur = lambda i: out.get((b, d, an, i), get(b, d, an, i))
        if op == "stalk":
            for i in idx: out[(b, d, an, i)] = fix_stalk(cur(i))
        elif op == "idle":
            idle = np.array(Image.open(os.path.join(RAWN, b, f"rot12_{d}.png")).convert("RGBA"))
            for i in idx: get(b, d, an, i); out[(b, d, an, i)] = idle.copy()   # get(): back the take up first
        elif op == "hold":
            rng, good = arg
            for i in rng: out[(b, d, an, i)] = cur(good).copy()
        elif op == "unball":
            for i in arg: out[(b, d, an, i)] = unball(cur(i))
        elif op == "head":
            fr, srci = arg
            for i in fr:   # celebrate_05: arms up beside the head from f04 on, so a narrower head box
                kw = {"half": 5, "patch": 6} if an == "celebrate_05" and i >= 4 else {"half": 8} if an == "celebrate_05" else {}
                out[(b, d, an, i)] = fix_head(cur(i), cur(srci), **kw)
        elif op == "grass":
            for i in arg: out[(b, d, an, i)] = grass(cur(i))
        elif op == "box":
            i, bx, mode = arg; out[(b, d, an, i)] = box(cur(i), bx, mode)   # cur(): boxes on one frame stack
        done.append(f"{op} {b}/{d}/{an} {arg if op not in ('stalk', 'idle') else 'all ' + str(len(idx))}")
    for b, rel, bx, mode in (BASE_OPS if not only else []):
        p = os.path.join(RAWN, b, *rel.split("/")) + ".png"
        bk = os.path.join(BACK, "raw_base_n", b, *rel.split("/")) + ".png"
        if not os.path.exists(bk): os.makedirs(os.path.dirname(bk), exist_ok=True); shutil.copy2(p, bk)
        a2 = box(np.array(Image.open(bk).convert("RGBA")), bx, mode)
        if not dry: Image.fromarray(a2).save(p)
        done.append(f"box {b}/{rel} {bx} {mode}")
    for (b, d, an, i), a in out.items():
        if not dry: Image.fromarray(a).save(path(b, d, an, i))
    print("\n".join(done)); print(len(out), "frames written" if not dry else "frames (dry run)")
    return out

if __name__ == "__main__":
    only = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--only=")), None)
    run("--dry" in sys.argv, only, "--refresh" in sys.argv)
