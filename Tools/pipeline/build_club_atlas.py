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

def complete(d, kind):
    names = [n for b in poses_x.SETS_X[kind] for n in b]
    return all(os.path.exists(os.path.join(d, x, n + ".png")) for x in GEN for n in names)

def build(slug, cdir):
    kit, player = cdir.split("_", 1)
    kind = "keeper" if kit.startswith("keeper") else "outfield"
    d = os.path.join(RAW, slug, cdir)
    rots = {x: Image.open(os.path.join(d, f"rot_{x}.png")).convert("RGBA") for x in DIRS8}
    src = Image.open(os.path.join(d, "ref96.png")).convert("RGBA")
    pal = BA.palette_for(list(rots.values()) + [src, src, src], False)     # the owner's idle counts triple
    idle_cols = list({c[:3] for im in list(rots.values()) + [src] for c in im.getdata() if c[3] >= 128})
    names = [n for b in poses_x.SETS_X[kind] for n in b]
    cells, qa = [], {"rejected": {}, "notes": {}}
    for x in DIRS8:
        im, _ = BA.clean(rots[x], rots[x].getbbox())
        im, _ = BA.snap_plain(im, pal)
        cells.append((f"{x}/idle", im))
    for x in GEN:
        idle_n = sum(1 for c in rots[x].getdata() if c[3] >= 128)
        for n in names:
            p = os.path.join(d, x, n + ".png")
            if not os.path.exists(p): continue
            im, notes = BA.clean(Image.open(p), rots[x].getbbox(), idle_cols)
            bb = im.getbbox(); n_op = sum(1 for c in im.getdata() if c[3])
            bad = None
            if not bb: bad = "empty"
            elif n_op < 0.45 * idle_n: bad = f"too_small {n_op}/{idle_n}"
            elif n_op > 2.8 * idle_n: bad = f"too_big {n_op}/{idle_n}"
            if "double_figure" in notes: bad = "double_figure"
            if bad: qa["rejected"][f"{x}/{n}"] = bad; continue
            im, st = BA.snap_plain(im, pal)
            if st["maxdist"] > 90: notes.append(f"colour_drift{int(st['maxdist'])}")
            if notes: qa["notes"][f"{x}/{n}"] = notes
            cells.append((f"{x}/{n}", im))
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

if __name__ == "__main__":
    slugs = sys.argv[1:] or sorted(os.listdir(RAW))
    for s in slugs:
        for cdir in sorted(os.listdir(os.path.join(RAW, s))):
            kit = cdir.split("_")[0]
            kind = "keeper" if kit.startswith("keeper") else "outfield"
            if not complete(os.path.join(RAW, s, cdir), kind): continue
            build(s, cdir)
