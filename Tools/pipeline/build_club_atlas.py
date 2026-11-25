"""v0.7 per-club sprite atlases: ArtWork/raw_clubs/<slug>/<kit>_<player>/ -> GameData/Sprites/Clubs/<slug>_<kit>.png/.json

  python -I Tools/pipeline/build_club_atlas.py            # every club character that has finished all its poses
  python -I Tools/pipeline/build_club_atlas.py newcastle-united

Same QA as build_atlas.py (hard alpha, specks, baked-in balls, size, doubled figures), then every pixel is snapped
to the club's OWN palette: the colours of the owner's south idle for that kit plus PixelLab's 8 rotations of it.
So stripes, trim and crest colours stay exactly the club's. The atlas is not kit-keyed: the game draws it as is.
Contact sheets: ArtWork/sheets_clubs/<slug>_<kit>.png (one row per direction)."""
import json, os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path[:0] = [os.path.join(ROOT, "Tools", "pipeline"), os.path.join(ROOT, "Tools", "pixellab")]
import build_atlas as BA
import poses_x

RAW = os.path.join(ROOT, "ArtWork", "raw_clubs")
OUT = os.path.join(ROOT, "GameData", "Sprites", "Clubs")
SHEETS = os.path.join(ROOT, "ArtWork", "sheets_clubs")
CELL, COLS = 96, 20
DIRS8 = BA.DIRS8
GEN = ["south", "south-east", "east", "north-east", "north"]
MIRROR = {"south-west": "south-east", "west": "east", "north-west": "north-east"}
# poses whose left/right meaning matters: a mirrored direction takes the OTHER side's frame, flipped
SWAP = {"gk_shuffleR": "gk_shuffleL", "gk_dive_low_R1": "gk_dive_low_L1", "gk_dive_high_R1": "gk_dive_high_L1", "gk_stretch_R": "gk_stretch_L"}
SWAP.update({v: k for k, v in list(SWAP.items())})

def hsv(c):
    import colorsys
    return colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255)

def scrub(img, idle_cols):
    """v0.7 clean-up PixelLab leaves behind (the owner's "players standing in their legs", ghost balls):
    1. mannequin ghosts: solid blue-grey limbs from the skeleton guide, a colour the player never wears;
    2. baked-in balls: a round white blob with dark patches, at the silhouette's edge (feet, hands, over the head).
    Returns (image, notes)."""
    from collections import deque
    img = img.copy(); px = img.load(); W, H = img.size; notes = []
    def near_idle(c, t):
        return min((c[0] - q[0]) ** 2 + (c[1] - q[1]) ** 2 + (c[2] - q[2]) ** 2 for q in idle_cols) <= t * t
    ghost = 0
    grey = set()
    for y in range(H):
        for x in range(W):
            c = px[x, y]
            if not c[3]: continue
            h, sat, v = hsv(c)
            if sat < 0.27 and 0.28 < v < 0.8 and c[2] >= c[0] + 6: grey.add((x, y))
    seen = set()
    for p0 in list(grey):
        if p0 in seen: continue
        comp = []; q = deque([p0]); seen.add(p0)
        while q:
            a0, b0 = q.popleft(); comp.append((a0, b0))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (a0 + dx, b0 + dy)
                    if n in grey and n not in seen: seen.add(n); q.append(n)
        if len(comp) >= 10:                        # a solid grey limb, not the idle's scattered grey speckle
            for p in comp: px[p] = (0, 0, 0, 0)
            ghost += len(comp)
    if ghost >= 6: notes.append(f"ghost{ghost}")
    # balls
    white = {(x, y) for y in range(H) for x in range(W) if px[x, y][3] and hsv(px[x, y])[1] < 0.2 and hsv(px[x, y])[2] > 0.6}
    seen = set()
    for p0 in list(white):
        if p0 in seen: continue
        comp = []; q = deque([p0]); seen.add(p0)
        while q:
            a, b = q.popleft(); comp.append((a, b))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (a + dx, b + dy)
                    if n in white and n not in seen: seen.add(n); q.append(n)
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if not (4 <= len(comp) <= 70 and 4 <= w <= 11 and 4 <= h <= 11 and 0.6 <= w / h <= 1.65): continue
        cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
        r = max(w, h) / 2.0 + 1.4
        ring = [(x, y) for y in range(max(0, int(cy - r)), min(H, int(cy + r) + 1)) for x in range(max(0, int(cx - r)), min(W, int(cx + r) + 1))
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r]
        dark = sum(1 for (x, y) in ring if px[x, y][3] and hsv(px[x, y])[2] < 0.32 and hsv(px[x, y])[1] < 0.4)
        empty = sum(1 for (x, y) in ring if not px[x, y][3])
        if dark >= 2 and empty >= 0.12 * len(ring):
            for (x, y) in ring:
                c = px[x, y]
                if c[3] and (hsv(c)[1] < 0.3 or (x, y) in white): px[x, y] = (0, 0, 0, 0)
            notes.append("ball_removed")
    # specks left behind
    comps = BA.components(img)
    if comps:
        comps.sort(key=len, reverse=True)
        for c in comps[1:]:
            if len(c) < 30:
                for p in c: px[p] = (0, 0, 0, 0)
    return img, notes

import numpy as np
_NOISE = None
def _noise():
    global _NOISE
    if _NOISE is None:
        y, x = np.mgrid[0:CELL, 0:CELL].astype(np.uint64)
        h = ((x * np.uint64(73856093)) ^ (y * np.uint64(19349663))) * np.uint64(2654435761)
        h = (h >> np.uint64(13)) & np.uint64(1023)
        _NOISE = (h.astype(np.float64) / 1023.0) * 2.0 - 1.0        # fixed in sprite space: the grain never crawls
    return _NOISE

def grain(arr, m):
    """mean luminance step between horizontal neighbours, inside the body only (outlines excluded)"""
    L = arr[..., 0] * 0.3 + arr[..., 1] * 0.59 + arr[..., 2] * 0.11
    inner = m.copy()
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0), (0, 2)):
        inner &= np.roll(np.roll(m, dy, 0), dx, 1)
    d = np.abs(L - np.roll(L, -1, 1))[inner]
    return float(d.mean()) if d.size else 0.0

def snap_idle(img, pal, target=None):
    """Every pixel onto the owner's south-idle palette, with the idle's TEXTURE (owner, v0.7: directions must keep the
    south idle's dither, texture and shading). PixelLab's turned and posed frames come back smoother than the
    hand-dithered idles, so a fixed per-pixel grain is added before the snap, scaled until the frame's inner texture
    measures the same as the idle's (`target`)."""
    a = np.asarray(img.convert("RGBA")).astype(np.float64)
    m = a[..., 3] > 0
    P = np.array(pal, dtype=np.float64)
    N = _noise()[:a.shape[0], :a.shape[1]]
    def snap(amp):
        rgb = a[..., :3] + N[..., None] * amp
        d = ((rgb[:, :, None, :] - P[None, None, :, :]) ** 2).sum(-1)
        return P[d.argmin(-1)], d.min(-1)
    best = None
    for amp in ([0.0] if target is None else [0.0, 8.0, 14.0, 20.0, 28.0, 36.0]):
        out, dm = snap(amp)
        score = abs(grain(out, m) - target) if target is not None else 0
        if best is None or score < best[0] - 0.3: best = (score, out, amp)
    res = np.zeros_like(a); res[..., :3] = best[1]; res[..., 3] = np.where(m, 255, 0)
    res[~m] = 0
    md = float(np.sqrt(snap(0.0)[1][m].max())) if m.any() else 0.0
    return Image.fromarray(res.astype(np.uint8), "RGBA"), {"maxdist": md, "amp": best[2]}

PACK2 = os.path.join(ROOT, "ArtSource", "Packs", "R08_93_Clubs_Both_Kits_Complete")
PACK_DIVES = {"gk_fly_high_L": "GK026", "gk_fly_high_R": "GK027", "gk_fly_low_L": "GK028", "gk_fly_low_R": "GK029",
              "gk_fly_take": "GK030", "gk_smother": "GK031"}
_EXP = None
def pack_dives(slug, player, kit):
    global _EXP
    if not os.path.isdir(PACK2): return []
    if _EXP is None:
        sys.path.insert(0, PACK2)
        import export_poses as E
        _EXP = E
    out = []
    for name, aid in PACK_DIVES.items():
        try: im = _EXP.frame(slug, player, aid, None, kit).convert("RGBA")
        except Exception as e:
            print("  pack frame", slug, player, aid, kit, "unavailable:", e); continue
        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.paste(im, ((CELL - im.width) // 2, 88 - 82), im)      # pack pivot (48,82) onto ours (48,88)
        out.append((name, cell))
    return out

def complete(d, kind):
    names = [n for b in poses_x.SETS_X[kind] for n in b]
    return all(os.path.exists(os.path.join(d, x, n + ".png")) for x in GEN for n in names)

def build(slug, cdir):
    kit, player = cdir.split("_", 1)
    kind = "keeper" if kit.startswith("keeper") else "outfield"
    d = os.path.join(RAW, slug, cdir)
    rots = {x: Image.open(os.path.join(d, f"rot_{x}.png")).convert("RGBA") for x in DIRS8}
    src = Image.open(os.path.join(d, "ref96.png")).convert("RGBA")
    # owner (v0.7): every direction must keep the south idle's dither, texture and shading, so the palette is the
    # owner's idle ONLY (PixelLab's rotations drift toward smooth gradients and new shades)
    pal = list({c[:3] for c in src.get_flattened_data() if c[3] >= 128})
    idle_cols = pal
    sa = np.asarray(src).astype(np.float64); tgt = grain(sa, sa[..., 3] > 0)   # the idle's own texture
    names = [n for b in poses_x.SETS_X[kind] for n in b]
    cells, qa = [], {"rejected": {}, "notes": {}}
    for x in DIRS8:
        im, _ = BA.clean(rots[x], rots[x].getbbox())
        im, _ = snap_idle(im, pal, tgt)
        cells.append((f"{x}/idle", im))
    for x in GEN:
        idle_n = sum(1 for c in rots[x].get_flattened_data() if c[3] >= 128)
        for n in names:
            p = os.path.join(d, x, n + ".png")
            if not os.path.exists(p): continue
            im, notes = BA.clean(Image.open(p), rots[x].getbbox(), idle_cols)
            im, n2 = scrub(im, idle_cols); notes += n2
            bb = im.getbbox(); n_op = sum(1 for c in im.get_flattened_data() if c[3])
            bad = None
            if not bb: bad = "empty"
            elif n_op < 0.45 * idle_n: bad = f"too_small {n_op}/{idle_n}"
            elif n_op > 2.8 * idle_n: bad = f"too_big {n_op}/{idle_n}"
            if "double_figure" in notes: bad = "double_figure"
            if bad: qa["rejected"][f"{x}/{n}"] = bad; continue
            im, st = snap_idle(im, pal, tgt)
            if st["maxdist"] > 90: notes.append(f"colour_drift{int(st['maxdist'])}")
            if notes: qa["notes"][f"{x}/{n}"] = notes
            cells.append((f"{x}/{n}", im))
    if kind == "keeper":
        # owner's R08 action pack (v0.7 audit): only the flat dives and the smother passed (the outfield actions and the
        # standing keeper poses have scrambled legs). They are texel-exact remaps of this player's own idle, so the art
        # matches by construction; they're drawn for the in-flight part of every dive, in every facing (ISS-style).
        for name, img in pack_dives(slug, player, kit):
            cells.append((f"south/{name}", img))
    rows = (len(cells) + COLS - 1) // COLS
    atlas = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
    frames = {}
    for i, (key, im) in enumerate(cells):
        c, r = i % COLS, i // COLS
        atlas.paste(im, (c * CELL, r * CELL)); frames[key] = [c, r, 0]
    for dst, srcd in MIRROR.items():
        for n in names:
            s = f"{srcd}/{SWAP.get(n, n)}"
            if s in frames: frames[f"{dst}/{n}"] = frames[s][:2] + [1]
    os.makedirs(OUT, exist_ok=True)
    cid = f"{slug}_{kit}"
    atlas.save(os.path.join(OUT, cid + ".png"), optimize=True)
    json.dump({"id": cid, "kind": kind, "cell": CELL, "pivot": [48, 88], "size": list(atlas.size), "kit_keyed": False,
               "atlas": f"GameData/Sprites/Clubs/{cid}.png", "player": player, "frames": frames},
              open(os.path.join(OUT, cid + ".json"), "w"))
    os.makedirs(SHEETS, exist_ok=True)
    qa["frames"] = len(cells); qa["palette"] = len(pal)
    json.dump(qa, open(os.path.join(SHEETS, cid + "_qa.json"), "w"), indent=1)
    # contact sheet: one row per generated direction, every pose, on grass
    order = ["idle"] + names
    sheet = Image.new("RGBA", (len(order) * CELL, len(GEN) * CELL), (44, 112, 52, 255))
    for r, x in enumerate(GEN):
        for i, n in enumerate(order):
            k = f"{x}/{n}"
            if k in frames:
                c, rr, _ = frames[k]
                im = atlas.crop((c * CELL, rr * CELL, c * CELL + CELL, rr * CELL + CELL))
                sheet.paste(im, (i * CELL, r * CELL), im)
    sheet.convert("RGB").save(os.path.join(SHEETS, cid + ".png"))
    print(cid, "frames", len(cells), "rejected", len(qa["rejected"]), "palette", len(pal))

def build_club(s):
    out = []
    for cdir in sorted(os.listdir(os.path.join(RAW, s))):
        kit = cdir.split("_")[0]
        kind = "keeper" if kit.startswith("keeper") else "outfield"
        if not complete(os.path.join(RAW, s, cdir), kind): out.append(f"{s} {cdir} incomplete"); continue
        try: build(s, cdir); out.append(f"{s} {cdir} ok")
        except Exception as e: out.append(f"{s} {cdir} ERROR {e}")
    return out

if __name__ == "__main__":
    # python -I build_club_atlas.py [--jobs N] [slugs...]   (clubs in parallel processes)
    args = sys.argv[1:]; jobs = 1
    if args[:1] == ["--jobs"]: jobs = int(args[1]); args = args[2:]
    slugs = args or sorted(os.listdir(RAW))
    if jobs > 1:
        from multiprocessing import Pool
        with Pool(jobs) as pool:
            for lines in pool.imap_unordered(build_club, slugs):
                for l in lines: print(l, flush=True)
    else:
        for s in slugs:
            for l in build_club(s): print(l, flush=True)
