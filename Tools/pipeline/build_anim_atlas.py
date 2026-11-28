"""v0.9.9: the owner's approved animations (ArtWork/raw_12/<body>/<dir>/<anim>/fNN.png, made by
Tools/pixellab/produce_approved.py) go into the body's atlas in 12 directions, next to the poses that have no approved
take yet. Run after build_base_atlas.py (it reads that atlas, drops the cycles the approved ones replace, appends).

  PF_RAW_BASE=raw_base_n python Tools/pipeline/build_base_atlas.py N01 NK1
  python Tools/pipeline/build_anim_atlas.py N01 NK1

Frames are named "<anim>_fNN" ("run_f03", "header_02_f05"); <id>.json gains "anims": {anim: {n, dt, loop, ev}} where
ev is the contact frame (ball struck / reached), which the game lines up with its own clip timing. Every frame is
cleaned like the base atlas (palette snap, kit keys, tidy) and placed in a 96 px cell with the ground line on the pivot
(y 88); takes drawn on a wider canvas (dives, falls, the goal kick) are centred on the body each frame, because the
game moves the player itself."""
import json, os, sys, glob
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path[:0] = [os.path.join(ROOT, "Tools", "pipeline"), os.path.join(ROOT, "Tools", "pixellab")]
import build_atlas as BA, build_club_atlas as BC, kit, build_base_atlas as BB
import plclient as pl

RAW12 = os.path.join(ROOT, "ArtWork", "raw_12"); RAWN = os.path.join(ROOT, "ArtWork", "raw_base_n"); OUTD = os.path.join(ROOT, "GameData", "Sprites")
CELL, COLS = 96, 42
# old cycles the approved animations replace (their cells are dropped to keep the atlas under 4096 px)
DROPS = {"run": ("run_", "runa_", "runb_", "runr_"), "sprint": ("sprint_", "sprinta_", "sprintr_"), "dribble": ("dribble_", "dribblea_", "dribbler_"), "walk": ("walk_",)}
# contact frame as a fraction of the clip when it can't be measured (kicks use the frame where the body is widest)
EV = {"pass": None, "shot": None, "volley": None, "gk_goalkick": None, "gk_punt": None, "header": 0.5, "tackle": 0.45, "throwin": 0.55,
      "gk_throw": 0.55, "gk_dive_low": 0.35, "gk_dive_high": 0.35, "gk_catch": 0.4, "gk_high": 0.45, "gk_scoop": 0.4, "chest": 0.4,
      "fall": 0.3, "gk_punch": 0.45}

def to_cell(im, canvas_h, recentre):
    """the frame in a 96 px cell: ground line (canvas_h - 10, or 88 on 96 px canvases) on y 88; wide-canvas takes
    centred on the body's opaque centroid"""
    W, H = im.size
    ground = 88 if H == 96 else H - 10
    if recentre:
        px = im.load(); xs = [x for y in range(H) for x in range(W) if px[x, y][3] >= 128]
        cx = int(round(sum(xs) / len(xs))) if xs else W // 2
    else: cx = W // 2
    out = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    out.paste(im.crop((cx - 48, ground - 88, cx + 48, ground + 8)), (0, 0))
    return out

def contact(frames, anim):
    base = anim.split("_")[0] if not anim.startswith("gk_") else "_".join(anim.split("_")[:2]) if not anim.startswith("gk_dive") else "_".join(anim.split("_")[:3])
    for k in (anim, base, "_".join(anim.split("_")[:-1])):
        if k in EV and EV[k] is not None: return int(round(EV[k] * (len(frames) - 1)))
        if k in EV:   # widest frame in the second half-ish: the kicking leg at full extension
            lo = len(frames) // 4 if k != "gk_goalkick" else len(frames) // 2
            ws = [(im.getbbox()[2] - im.getbbox()[0]) if im.getbbox() else 0 for im in frames]
            return max(range(lo, len(frames)), key=lambda i: ws[i])
    return -1

def build(bid):
    meta = json.load(open(os.path.join(OUTD, bid + ".json"))); old = Image.open(os.path.join(OUTD, bid + ".png")).convert("RGBA")
    rots = {x: Image.open(os.path.join(RAWN, bid, f"rot_{x}.png")).convert("RGBA") for x in BB.DIRS8}
    pal = BA.palette_for(rots.values(), True)
    have = {os.path.basename(a) for a in glob.glob(os.path.join(RAW12, bid, "*", "*"))}
    DROP = tuple(p for a, ps in DROPS.items() if a in have for p in ps) or ("@none@",)
    cells = []
    for key, (c, r, fl) in meta["frames"].items():
        if fl: continue
        if key.split("/")[1].startswith(DROP): continue
        cells.append((key, old.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))))
    mirrors_old = {k: v for k, v in meta["frames"].items() if v[2] and not k.split("/")[1].startswith(DROP)}
    anims = {}
    keeper = meta.get("kind") == "keeper"
    def add(key, im):
        if not keeper: im, _ = BB.strip_ball(im)   # a ball PixelLab drew at his feet (dribble) or by his head
        im, _ = kit.snap_frame(im, pal); im, _ = BB.kit_fill(im); im, _ = BC.tidy(im, fill=True); cells.append((key, im))
    # the 12-direction turns replace the 8-direction idle stills where a 12-direction idle exists
    for x in pl.DRAWN12:
        f = os.path.join(RAWN, bid, f"rot12_{x}.png")
        if os.path.exists(f) and x not in ("south", "east", "north"):
            add(f"{x}/idle", Image.open(f).convert("RGBA"))
    root = os.path.join(RAW12, bid)
    for x in pl.DRAWN12:
        for ad in sorted(glob.glob(os.path.join(root, x, "*"))):
            if not os.path.exists(os.path.join(ad, "take.json")): continue
            anim = os.path.basename(ad); t = json.load(open(os.path.join(ad, "take.json")))
            fr = [Image.open(f).convert("RGBA") for f in sorted(glob.glob(os.path.join(ad, "f*.png")))]
            if t["loop"] and len(fr) > 2: fr = fr[:-1]
            H = fr[0].size[1]; rec = fr[0].size[0] > 96
            fr = [to_cell(im, H, rec) for im in fr]
            for i, im in enumerate(fr): add(f"{x}/{anim}_f{i:02d}", im)
            a = anims.setdefault(anim, {"n": len(fr), "dt": t["dt"], "loop": bool(t["loop"]), "ev": contact(fr, anim)})
            a["n"] = min(a["n"], len(fr))
    rows = (len(cells) + COLS - 1) // COLS
    atlas = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0)); frames = {}
    for i, (key, im) in enumerate(cells):
        c, r = i % COLS, i // COLS; atlas.paste(im, (c * CELL, r * CELL)); frames[key] = [c, r, 0]
    # mirrors: the old 8-direction ones (side-swapped names, as before), then the 12-direction ones
    for key in mirrors_old:
        sx, n = key.split("/"); src = BB.MIRROR.get(sx)
        sk = f"{src}/{BB.side_swap(n)}"
        if sk in frames: frames[key] = frames[sk][:2] + [1]
    for dst, src in pl.MIRROR12.items():
        for key in list(frames):
            sx, n = key.split("/")
            if sx == src and (n == "idle" or "_f" in n) and frames[key][2] == 0 and f"{dst}/{n}" not in frames:
                frames[f"{dst}/{n}"] = frames[key][:2] + [1]
    atlas.save(os.path.join(OUTD, f"{bid}.png"))
    meta.update({"size": list(atlas.size), "frames": frames, "anims": anims, "dirs": 12})
    json.dump(meta, open(os.path.join(OUTD, f"{bid}.json"), "w"))
    print(bid, "cells", len(cells), "atlas", atlas.size, "anims", {k: (v["n"], v["ev"]) for k, v in anims.items()})

if __name__ == "__main__":
    for b in sys.argv[1:]: build(b)
