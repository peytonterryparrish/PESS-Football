"""Raw PixelLab stills -> QA'd, palette-clamped sprite atlas per base character.
  python -I Tools/pipeline/build_atlas.py [ids...]
In:  ArtWork/raw/<id>/rot_<dir>.png, ArtWork/raw/<id>/<dir>/<pose>.png
Out: GameData/Sprites/<id>.png + .json (frames "dir/pose" -> [col,row,flip]), ArtWork/sheets/<id>.png, ArtWork/qa/<id>.json

QA per frame (rejected frames are left out; the game falls back to the neighbouring direction or the idle):
  - hard alpha (no soft edges), stray specks removed (components < 6 px apart from the body)
  - baked-in ball removed (a whitish blob below the knees)
  - every pixel clamped to the character's palette (+ kit key colours for template-kit characters)
  - size vs the idle in that direction, nothing touching the canvas edge, colour drift
"""
import colorsys, json, os, sys
from collections import Counter, deque
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path[:0] = [os.path.join(ROOT, "Tools", "pipeline"), os.path.join(ROOT, "Tools", "pixellab")]
import kit, poses, poses_more, sheet

RAW = os.path.join(ROOT, "ArtWork", "raw")
CELL = 96; COLS = 20
DIRS8 = ["south", "south-east", "east", "north-east", "north", "north-west", "west", "south-west"]

def hsv(c): return colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255)

def components(img):
    px = img.load(); W, H = img.size; seen = set(); comps = []
    for y in range(H):
        for x in range(W):
            if px[x, y][3] == 0 or (x, y) in seen: continue
            q = deque([(x, y)]); seen.add((x, y)); c = []
            while q:
                a, b = q.popleft(); c.append((a, b))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        n = (a + dx, b + dy)
                        if 0 <= n[0] < W and 0 <= n[1] < H and n not in seen and px[n][3] > 0: seen.add(n); q.append(n)
            comps.append(c)
    return comps

def clean(img, idle_bbox):
    img = img.convert("RGBA"); px = img.load(); W, H = img.size
    notes = []
    for y in range(H):
        for x in range(W):
            if px[x, y][3] < 128: px[x, y] = (0, 0, 0, 0)
            else: px[x, y] = px[x, y][:3] + (255,)
    comps = components(img)
    if not comps: return img, ["empty"]
    comps.sort(key=len, reverse=True)
    big = comps[0]
    if len(comps) > 1 and len(comps[1]) >= 0.3 * len(big): notes.append("double_figure")
    for c in comps:
        if c is not big and len(c) < 30:
            for p in c: px[p] = (0, 0, 0, 0)
            if len(c) >= 6: notes.append(f"blob{len(c)}")
    # ball: whitish low-saturation cluster in the lower part of the figure
    top, bot = idle_bbox[1], idle_bbox[3]
    knee = top + (bot - top) * 0.62
    white = set()
    for y in range(int(knee), H):
        for x in range(W):
            c = px[x, y]
            if c[3] and hsv(c)[1] < 0.30 and hsv(c)[2] > 0.52: white.add((x, y))
    # grow to grey seams adjacent to whites
    if len(white) >= 7:
        grow = set(white)
        for (x, y) in list(white):
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (x + dx, y + dy)
                    if 0 <= n[0] < W and 0 <= n[1] < H and px[n][3] and hsv(px[n])[1] < 0.2 and hsv(px[n])[2] > 0.25: grow.add(n)
        xs = [p[0] for p in grow]; ys = [p[1] for p in grow]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if 3 <= w <= 12 and 3 <= h <= 12 and len(grow) >= 0.45 * w * h:
            for p in grow: px[p] = (0, 0, 0, 0)
            notes.append("ball_removed")
    return img, notes

def palette_for(rots, kit_keyed):
    cnt = Counter()
    for im in rots:
        for c in im.convert("RGBA").get_flattened_data():
            if c[3] >= 128 and (not kit_keyed or kit.kit_region(c[:3]) is None): cnt[c[:3]] += 1
    return [c for c, n in cnt.items() if n >= 3]

def snap_plain(img, pal):
    img = img.copy(); px = img.load(); md = 0
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if not a: continue
            best = min(pal, key=lambda p: (p[0] - r) ** 2 + (p[1] - g) ** 2 + (p[2] - b) ** 2)
            md = max(md, ((best[0] - r) ** 2 + (best[1] - g) ** 2 + (best[2] - b) ** 2) ** .5)
            px[x, y] = best + (255,)
    return img, {"maxdist": md}

def build(cid, cast):
    d = os.path.join(RAW, cid)
    meta = cast[cid]; kind = meta["kind"]; keyed = bool(meta.get("template_kit"))
    rots = {x: Image.open(os.path.join(d, f"rot_{x}.png")).convert("RGBA") for x in DIRS8}
    pal = palette_for(rots.values(), keyed)
    poses_all = [n for b in poses_more.SETS[kind] for n in b]
    cells = []      # (key, image, flip_of)
    qa = {"rejected": {}, "notes": {}}
    # idles: all eight real rotations
    for x in DIRS8:
        im, notes = clean(rots[x], rots[x].getbbox())
        im, st = kit.snap_frame(im, pal) if keyed else snap_plain(im, pal)
        cells.append((f"{x}/idle", im))
    for x in poses_more.GEN_DIRS:
        ib = rots[x].getbbox(); idle_n = sum(1 for c in rots[x].get_flattened_data() if c[3] >= 128)
        for n in poses_all:
            p = os.path.join(d, x, n + ".png")
            if not os.path.exists(p): continue
            im, notes = clean(Image.open(p), ib)
            bb = im.getbbox()
            n_op = sum(1 for c in im.get_flattened_data() if c[3])
            bad = None
            if not bb: bad = "empty"
            elif n_op < 0.45 * idle_n: bad = f"too_small {n_op}/{idle_n}"
            elif n_op > 2.6 * idle_n: bad = f"too_big {n_op}/{idle_n}"
            elif bb[0] == 0 or bb[1] == 0 or bb[2] == CELL or bb[3] == CELL: notes.append("touches_edge")
            if "double_figure" in notes: bad = "double_figure"
            if bad: qa["rejected"][f"{x}/{n}"] = bad; continue
            im, st = kit.snap_frame(im, pal) if keyed else snap_plain(im, pal)
            if st.get("maxdist", 0) > 90: notes.append(f"colour_drift{int(st['maxdist'])}")
            if notes: qa["notes"][f"{x}/{n}"] = notes
            cells.append((f"{x}/{n}", im))
    # atlas
    rows = (len(cells) + COLS - 1) // COLS
    atlas = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
    frames = {}
    for i, (key, im) in enumerate(cells):
        c, r = i % COLS, i // COLS
        atlas.paste(im, (c * CELL, r * CELL)); frames[key] = [c, r, 0]
    for dst, src in poses_more.MIRROR.items():
        for n in poses_all:
            if f"{src}/{n}" in frames: frames[f"{dst}/{n}"] = frames[f"{src}/{n}"][:2] + [1]
    out = os.path.join(ROOT, "GameData", "Sprites"); os.makedirs(out, exist_ok=True)
    atlas.save(os.path.join(out, f"{cid}.png"))
    json.dump({"id": cid, "kind": kind, "cell": CELL, "pivot": [48, 88], "size": list(atlas.size), "kit_keyed": keyed,
               "atlas": f"GameData/Sprites/{cid}.png", "frames": frames}, open(os.path.join(out, f"{cid}.json"), "w"))
    os.makedirs(os.path.join(ROOT, "ArtWork", "qa"), exist_ok=True)
    qa["frames"] = len(cells); qa["palette"] = len(pal)
    json.dump(qa, open(os.path.join(ROOT, "ArtWork", "qa", f"{cid}.json"), "w"), indent=1)
    # contact sheet (kit previewed in a sample club kit)
    kits = {k["slug"]: k for k in json.load(open(os.path.join(ROOT, "ArtSource/Packs/R08_93_Clubs_South_Idle_Only/Club_Kit_Palettes.json")))}
    sd = os.path.join(ROOT, "ArtWork", "sheets", cid); os.makedirs(sd, exist_ok=True)
    order = ["idle"] + poses_all
    for x in DIRS8:
        paths = []
        for n in order:
            k = f"{x}/{n}"
            if k not in frames: continue
            c, r, fl = frames[k]
            im = atlas.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
            if fl: im = im.transpose(Image.FLIP_LEFT_RIGHT)
            if keyed: im = kit.preview_kit(im, kits["newcastle-united"]["home"] if x in ("south", "north") else kits["liverpool"]["home"])
            fp = os.path.join(sd, f"{x}__{n}.png"); im.save(fp); paths.append(fp)
        if paths: sheet.make(paths, os.path.join(ROOT, "ArtWork", "sheets", f"{cid}_{x}.png"), 2, 12)
    print(cid, "cells", len(cells), "rejected", len(qa["rejected"]), "flagged", len(qa["notes"]), "atlas", atlas.size)

if __name__ == "__main__":
    cast = {c["id"]: c for c in json.load(open(os.path.join(ROOT, "Tools", "pixellab", "cast.json")))}
    ids = sys.argv[1:] or [c for c in cast if os.path.exists(os.path.join(RAW, c, "rot_south.png"))]
    for cid in ids: build(cid, cast)
