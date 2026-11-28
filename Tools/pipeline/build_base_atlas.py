"""v0.8 base-body pilot atlases: ArtWork/raw_base/<body>/ -> GameData/Sprites/<body>.png/.json (kit-keyed).

  python -I Tools/pipeline/build_base_atlas.py B01 BK1            # auto-picked takes
  python -I Tools/pipeline/build_base_atlas.py B01 --sheets        # + per-direction contact sheets in ArtWork/base_pilot/

For every direction and animation job it uses ONE take (so a cycle's frames always come from the same generation):
the take scored best by QA (opaque size like the idle, no stray islands, nothing at the cell edge), unless
ArtWork/base_pilot/picks.json names one: {"B01/east/run": 2}. Each frame is cleaned like the club sets (scrub,
islands, tidy), kit pixels become key colours and everything else snaps to the body's own palette (from the owner's
idle and its turns). Mirrored directions take the opposite-side frame, flipped. Keepers get the owner's audited flat
dives from the R08 pack, recoloured into the template kit."""
import json, os, sys
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path[:0] = [os.path.join(ROOT, "Tools", "pipeline"), os.path.join(ROOT, "Tools", "pixellab")]
import build_atlas as BA, build_club_atlas as BC, kit, poses_base, produce_base

RAW = os.path.join(ROOT, "ArtWork", os.environ.get("PF_RAW_BASE", "raw_base")); OUTD = os.path.join(ROOT, "GameData", "Sprites")
PILOT = os.path.join(ROOT, "ArtWork", "base_pilot")
CELL, COLS = 96, 20
DIRS8 = ["south", "south-east", "east", "north-east", "north", "north-west", "west", "south-west"]
GEN = produce_base.GEN
MIRROR = {"south-west": "south-east", "west": "east", "north-west": "north-east"}

def side_swap(n):
    for a, b in (("_R1", "_L1"), ("_L1", "_R1"), ("_R", "_L"), ("_L", "_R")):
        if n.endswith(a): return n[: -len(a)] + b
    if n.endswith("R") and not n.endswith("_R"): return n[:-1] + "L"
    if n.endswith("L") and not n.endswith("_L"): return n[:-1] + "R"
    return n

def light_comps(im):
    """connected blobs of light, unsaturated pixels (balls, the skeleton guide's grey limbs, skin highlights)"""
    import colorsys
    from collections import deque
    px = im.load(); W, H = im.size; L = set()
    for y in range(H):
        for x in range(W):
            c = px[x, y]
            if c[3]:
                _, s_, v = colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255)
                if s_ < 0.26 and v > 0.45: L.add((x, y))
    seen, out = set(), []
    for p0 in L:
        if p0 in seen: continue
        comp, q = [], deque([p0]); seen.add(p0)
        while q:
            a, b = q.popleft(); comp.append((a, b))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (a + dx, b + dy)
                    if n in L and n not in seen: seen.add(n); q.append(n)
        out.append(comp)
    return out

def strip_ball(im):
    """Base bodies: a baked-in ball is a roundish light blob at the feet or above the head (the owner's idle has the
    same warm greys in skin highlights, so colour alone can't tell). It is cut out with its dark panels, sparing kit."""
    px = im.load(); bb = im.getbbox(); notes = []
    if not bb: return im, notes
    top, bot = bb[1], bb[3]; hgt = bot - top
    for comp in light_comps(im):
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if not (5 <= len(comp) <= 120 and 3 <= w <= 13 and 3 <= h <= 13 and 0.5 <= w / h <= 2.0): continue
        if kit.WIDE and sum(px[q][0] - px[q][2] for q in comp) / len(comp) > 40: continue   # v0.10: warm = a lit knee or calf, not a ball (it left gaps in the legs)
        cy = (min(ys) + max(ys)) / 2
        cx = (min(xs) + max(xs)) / 2; wid = bb[2] - bb[0]
        dense = len(comp) >= 0.4 * w * h and 4 <= w <= 10 and 4 <= h <= 10
        round_ball = len(comp) >= 0.4 * w * h and 6 <= w <= 13 and 6 <= h <= 13 and 0.7 <= w / h <= 1.4   # a ball anywhere (held to the chest)
        at_end = cx < bb[0] + 0.22 * wid or cx > bb[2] - 0.22 * wid      # lying figures: the ball is at an end
        if not (cy > top + 0.72 * hgt or cy < top + 0.12 * hgt or (at_end and dense) or round_ball): continue
        r = max(w, h) / 2 + 1.2; n = 0
        for y in range(int(cy - r - 1), int(cy + r + 2)):
            for x in range(int(cx - r - 1), int(cx + r + 2)):
                if 0 <= x < im.width and 0 <= y < im.height and (x - cx) ** 2 + (y - cy) ** 2 <= r * r and px[x, y][3]:
                    if kit.kit_region(px[x, y][:3]) is None: px[x, y] = (0, 0, 0, 0); n += 1
        notes.append(f"ball{n}")
    return im, notes

def far_islands(im, gap=4):
    """anything more than `gap` px from the main body (a ball kicked loose, a stray fragment) goes"""
    comps = sorted(BA.components(im), key=len, reverse=True)
    if len(comps) < 2: return im, []
    px = im.load(); body = set(comps[0]); n = 0
    for c in comps[1:]:
        if min(min(abs(x - bx) + abs(y - by) for bx, by in body) for x, y in c[:40]) > gap:
            for q in c: px[q] = (0, 0, 0, 0)
            n += len(c)
    return im, ([f"far{n}"] if n else [])

def ghost_score(im):
    """size of big light elongated blobs (the skeleton guide's grey limbs), for picking a take without them"""
    g = 0
    for comp in light_comps(im):
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if len(comp) >= 14 and max(w, h) >= 2.5 * min(w, h): g += len(comp)
    return g

def kit_fill(im, passes=3, need=3):
    """v0.8.1: grey, purple and blue leftovers inside a kit area (the guide's grey, a template body's own grey blot on the
    shorts, dithered sock bands) become that kit region. Only pixels that aren't warm like skin, hair or the white shirt
    number are touched, and only when `need` of their 8 neighbours are key pixels of ONE region; a few passes fill a blot
    from its edges in."""
    px = im.load(); W, H = im.size; n = 0
    inv = set(kit.REGION_CODE.values())
    for _ in range(passes):
        ch = []
        for y in range(1, H - 1):
            for x in range(1, W - 1):
                r, g, b, a = px[x, y]
                if not a or (b == 1 and r in inv): continue
                if r > g > b and r - b >= 45: continue          # skin, hair, highlights, the number: warm, kept
                if r + g + b < 200: continue                    # outline and boot blacks kept
                cnt = {}
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx or dy:
                            q = px[x + dx, y + dy]
                            if q[3] and q[2] == 1 and q[0] in inv: cnt.setdefault(q[0], []).append(q[1])
                if not cnt: continue
                reg, gs = max(cnt.items(), key=lambda kv: len(kv[1]))
                if len(gs) >= need: ch.append((x, y, reg, sorted(gs)[len(gs) // 2]))
        for x, y, reg, gv in ch: px[x, y] = (reg, gv, 1, 255)
        n += len(ch)
        if not ch: break
    return im, n

def audit_clean(im, keeper=False):
    """v0.10 (ArtWork/audit_v100): after the snap and tidy, (1) loose debris goes: islands apart from the body that are
    not a detached hand/foot (skin, or a keeper's light glove, close to the body): ball crumbs, speckle, a ball kicked
    away; (2) thin dark/brown lines sticking UP out of the silhouette (the PixelLab hair-line / "antenna" above the head)
    go: 1-2 px wide runs a 3x3 opening removes, with nothing of the body above them, reaching >= 3 px from it.
    Returns (image, removed pixel count)."""
    import numpy as np
    from scipy import ndimage as ND
    a = np.array(im); op = a[..., 3] > 0
    if not op.any(): return im, 0
    key = op & (a[..., 2] == 1) & np.isin(a[..., 0], list(kit.REGION_CODE.values()))
    rgb = a[..., :3].astype(int); mx = rgb.max(-1); mn = rgb.min(-1)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1), 0); val = mx / 255
    warm = (rgb[..., 0] >= rgb[..., 1]) & (rgb[..., 1] >= rgb[..., 2] - 4)
    skin = op & ~key & warm & (val > 0.55) & (sat > 0.12) & (sat < 0.62)
    light = op & ~key & (sat < 0.2) & (val > 0.6)
    E8 = np.ones((3, 3), bool); n0 = int(op.sum())
    lab, n = ND.label(op, E8)
    if n > 1:
        sizes = ND.sum(op, lab, range(1, n + 1)); main = lab == (1 + int(np.argmax(sizes)))
        dist = ND.distance_transform_cdt(~main, metric="chessboard")
        for i in range(1, n + 1):
            m = lab == i
            if m is main or (m & main).any(): continue
            k = int(m.sum()); d = int(dist[m].min())
            if k >= 120 or (k >= 30 and d <= 8): continue        # a limb (a foot PixelLab drew apart), not debris
            sk = (skin & m).sum() / k; lt = (light & m).sum() / k
            keep = (d <= 6 and k >= 4 and sk >= 0.5) or (keeper and d <= 6 and k >= 6 and lt >= 0.5) or (d <= 2 and k >= 4)
            if not keep: op[m] = False
    opened = ND.binary_opening(op, E8)
    if opened.any():
        top = np.nonzero(op.any(1))[0].min()
        tl, tn = ND.label(op & ~opened, E8)
        far = ND.distance_transform_cdt(~opened, metric="chessboard")
        for i in range(1, tn + 1):
            m = tl == i; k = int(m.sum())
            if k < 3 or k > 24: continue
            ys, xs = np.nonzero(m)
            if ys.min() > top + 20 or far[m].max() < 3: continue
            if opened[:ys.min(), xs.min():xs.max() + 1].any(): continue          # the body goes on above it: not on top
            if (skin & m).sum() / k > 0.4: continue                             # a finger, not a hair line
            if (light & m).sum() / k > 0.3: continue                            # a glove's fingertips
            op[m] = False
    a[~op] = 0
    # sock shading PixelLab drew in a dull red the snap sent to the body's palette (lips/skin shadow colours): red-hued
    # pixels touching sock keys become sock keys too (two passes, so a 2-px band fills from its edges)
    import colorsys
    for _ in range(2):
        sock = (a[..., 3] > 0) & (a[..., 2] == 1) & (a[..., 0] == kit.REGION_CODE["socks"])
        nb = ND.convolve(sock.astype(int), np.ones((3, 3), int), mode="constant")
        ys, xs = np.nonzero((a[..., 3] > 0) & ~sock & (a[..., 2] != 1) & (nb >= 2))
        ch = 0
        for y, x in zip(ys, xs):
            r, g, b = (int(v) for v in a[y, x, :3])
            h, s_, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255); h *= 360
            if (h <= 12 or h >= 338) and s_ > 0.5 and v > 0.25:
                a[y, x] = (kit.REGION_CODE["socks"], kit.SHADES[kit.shade_index((r, g, b), "socks")], 1, 255); ch += 1
        if not ch: break
    return Image.fromarray(a), n0 - int(op.sum())

def no_sash(im):
    """v0.8.1: in back views of a body whose turns drifted (produce_base BODIES[...]["sash"]), trim on the torso (above
    the waistband) becomes shirt, and the sash's bright yellow and pink leftovers too. Only for dark-skinned bodies:
    their skin never gets this bright."""
    px = im.load(); W, H = im.size
    rows = [y for y in range(H) if sum(1 for x in range(W) if px[x, y][3] and px[x, y][2] == 1 and px[x, y][0] == 80) >= 4]   # the waistband (a stray pixel isn't)
    if not rows: return im, 0
    lim = rows[0] - 2; n = 0
    for y in range(lim):
        for x in range(W):
            r, g, b, a = px[x, y]
            if not a: continue
            if b == 1 and r == 160: px[x, y] = (40, g, 1, 255); n += 1
            elif b != 1 and r > 180 and ((g > 140 and b < 130) or (b > 140)): px[x, y] = (40, 192, 1, 255); n += 1
    return im, n

def prep(im, idle_cols, keeper, ibox):
    im, notes = BA.clean(im, ibox, specks=False)
    im, n2 = BC.scrub(im, idle_cols, keeper); notes += n2
    im, n3 = strip_ball(im)
    if not keeper:   # the skeleton guide's grey limbs: big, light, elongated blobs (keepers' gloves are left alone)
        px = im.load()
        for comp in light_comps(im):
            xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
            w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
            if len(comp) >= 14 and max(w, h) >= 1.5 * min(w, h):
                for q in comp: px[q] = (0, 0, 0, 0)
                notes.append(f"ghostcut{len(comp)}")
    im, n4 = BC.islands(im, idle_cols, keeper)
    im, n5 = far_islands(im)
    g = ghost_score(im)
    return im, notes + n4 + ([f"ghost{g}"] if g else [])

def score(frames, idle_n):
    """lower is better"""
    s = 0.0
    for im, notes in frames:
        n = sum(1 for c in im.get_flattened_data() if c[3])
        r = n / max(1, idle_n)
        s += abs(r - 1.1) * 2 if (r < 0.6 or r > 2.0) else 0
        s += 0.5 * len([t for t in notes if not t.startswith('ball')]) + sum(int(t[5:]) / 10 for t in notes if t.startswith('ghost') and t[5:].isdigit()) + 0.3 * sum(t.startswith('ghostcut') for t in notes)
        bb = im.getbbox()
        if not bb: s += 10
        elif bb[0] == 0 or bb[1] == 0 or bb[2] == CELL or bb[3] == CELL: s += 1
    return s

def build(bid, sheets=False):
    body = produce_base.BODIES[bid]; keeper = body["kind"] == "keeper"; d = os.path.join(RAW, bid)
    kit.WIDE = bid in kit.WIDE_BODIES
    picks = json.load(open(os.path.join(PILOT, "picks.json"))) if os.path.exists(os.path.join(PILOT, "picks.json")) else {}
    rots = {x: Image.open(os.path.join(d, f"rot_{x}.png")).convert("RGBA") for x in DIRS8}
    pal = BA.palette_for(rots.values(), True)
    idle_cols = list({c[:3] for im in rots.values() for c in im.get_flattened_data() if c[3] >= 128 and kit.kit_region(c[:3]) is None})
    jobs = poses_base.KEEP_JOBS if keeper else poses_base.OUT_JOBS
    cells, chosen = [], {}
    def add(key, im):
        im, _ = kit.snap_frame(im, pal)
        if key.split("/")[0] in body.get("sash", ()): im, _ = no_sash(im)
        im, _ = kit_fill(im)
        im, _ = BC.tidy(im, fill=True)
        if kit.WIDE: im, _ = audit_clean(im, keeper)
        cells.append((key, im))
    for x in DIRS8:
        im, _ = BA.clean(rots[x], rots[x].getbbox(), specks=False)
        add(f"{x}/idle", im)
    for x in GEN:
        ibox = rots[x].getbbox(); idle_n = sum(1 for c in rots[x].get_flattened_data() if c[3] >= 128)
        for j in jobs:
            name, batch = j[0], j[1]
            if len(j) > 3 and j[3] and x not in j[3]: continue
            takes = sorted(int(t[1:]) for t in os.listdir(os.path.join(d, x)) if t.startswith("t") and
                           all(os.path.exists(os.path.join(d, x, t, f"{n}.png")) for n in batch))
            if not takes: continue
            cand = {}
            for t in takes:
                cand[t] = [prep(Image.open(os.path.join(d, x, f"t{t}", f"{n}.png")), idle_cols, keeper, ibox) for n in batch]
            t = picks.get(f"{bid}/{x}/{name}")
            if t not in cand: t = min(cand, key=lambda k: score(cand[k], idle_n))
            chosen[f"{x}/{name}"] = t
            for n, (im, _) in zip(batch, cand[t]):
                if f"{x}/{n}" not in {k for k, _ in cells}: add(f"{x}/{n}", im)
    if keeper:   # v0.9: the left-hand dive frames are the right-hand ones mirrored (front-on, so a flip is exact)
        have = dict(cells)
        for n in ("gk_push", "gk_load", "gk_reach", "gk_fly", "gk_land"):
            if f"south/{n}_R" in have and f"south/{n}_L" not in have: add(f"south/{n}_L", have[f"south/{n}_R"].transpose(Image.FLIP_LEFT_RIGHT))
    if keeper and body.get("src"):   # the owner's audited flat dives (R08 pack), recoloured into the template kit (v0.9.6: bodies with their own dive only)
        kits = {k["slug"]: k for k in json.load(open(os.path.join(produce_base.IDLES, "Club_Kit_Palettes.json")))}
        player = os.path.splitext(os.path.basename(body["src"]))[0]
        for name, cell in BC.pack_dives(body["club"], player, body["kit"]):
            cell, _ = kit.to_template(cell, kits[body["club"]][body["kit"]])
            wide = os.path.join(d, "pack", name + ".png")    # v0.9.5: the stocky version (Tools/pixellab/widen_dives.py)
            if os.path.exists(wide): cell = Image.open(wide).convert("RGBA")
            add(f"south/{name}", cell)
    rows = (len(cells) + COLS - 1) // COLS
    atlas = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0)); frames = {}
    for i, (key, im) in enumerate(cells):
        c, r = i % COLS, i // COLS; atlas.paste(im, (c * CELL, r * CELL)); frames[key] = [c, r, 0]
    for dst, src in MIRROR.items():
        for key in list(frames):
            sx, n = key.split("/")
            if sx == src and n != "idle": frames[f"{dst}/{side_swap(n)}"] = frames[key][:2] + [1]
    atlas.save(os.path.join(OUTD, f"{bid}.png"))
    json.dump({"id": bid, "kind": body["kind"], "cell": CELL, "pivot": [48, 88], "size": list(atlas.size), "kit_keyed": True,
               "atlas": f"GameData/Sprites/{bid}.png", "base_body": True, "frames": frames}, open(os.path.join(OUTD, f"{bid}.json"), "w"))
    os.makedirs(PILOT, exist_ok=True)
    json.dump(chosen, open(os.path.join(PILOT, f"{bid}_chosen_takes.json"), "w"), indent=1)
    if sheets:
        kits = {k["slug"]: k for k in json.load(open(os.path.join(produce_base.IDLES, "Club_Kit_Palettes.json")))}
        order = ["idle"] + [n for j in jobs for n in j[1]]
        for kname, kk in (("liverpool", kits["liverpool"]["keeper1" if keeper else "home"]), ("newcastle", kits["newcastle-united"]["keeper1" if keeper else "home"])):
            for x in DIRS8:
                row = [n for n in order if f"{x}/{n}" in frames]
                sh = Image.new("RGBA", (CELL * len(row), CELL), (58, 118, 48, 255))
                for i, n in enumerate(row):
                    c, r, fl = frames[f"{x}/{n}"]; im = atlas.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
                    if fl: im = im.transpose(Image.FLIP_LEFT_RIGHT)
                    sh.alpha_composite(kit.preview_kit(im, kk), (i * CELL, 0))
                sh.resize((sh.width * 2, sh.height * 2), Image.NEAREST).save(os.path.join(PILOT, f"sheet_{bid}_{kname}_{x}.png"))
    print(bid, "cells", len(cells), "atlas", atlas.size, "takes", chosen)

if __name__ == "__main__":
    ids = [a for a in sys.argv[1:] if not a.startswith("--")] or list(produce_base.BODIES)
    for b in ids: build(b, "--sheets" in sys.argv)
