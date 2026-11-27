"""v0.9.8: the keeper's catch/hold/punch/scoop/punt frames still had a drawn ball. debake_ball.py can't tell his grey
gloves from the ball (same greys), and a whole-frame pixen edit changed his arms and sleeves. So: find the ball (a light
blob at least 26 px and 6x6, gloves are narrower), mask just that box, and inpaint it with PixelLab (inpaint-v3).
Originals are kept as <dir>/t0/_baked/<pose>.png.
  python Tools/pixellab/debake_keeper_pixen.py ArtWork/raw_base_n/NK1 [--dry]"""
import os, sys, shutil, concurrent.futures as cf
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import plclient as pl

POSES = ["gk_catch_chest", "gk_catch_high", "gk_hold_ball", "gk_punch", "gk_scoop", "gk_punt_drop", "gk_punt_strike",
         "gk_throw_back", "gk_throw_release"]
EXTRA = {("south", "gk_punch"): (45, 0, 53, 6)}   # checked by eye: a ball just above his fists, smaller than the rule

def light(c): return c[3] >= 128 and min(c[:3]) >= 160 and max(c[:3]) - min(c[:3]) < 30

def ball_box(im):
    px = im.load(); W, H = im.size; seen = set(); best = None
    for y in range(H):
        for x in range(W):
            if (x, y) in seen or not light(px[x, y]): continue
            st = [(x, y)]; comp = []; seen.add((x, y))
            while st:
                a, b = st.pop(); comp.append((a, b))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        q = (a + dx, b + dy)
                        if 0 <= q[0] < W and 0 <= q[1] < H and q not in seen and light(px[q]): seen.add(q); st.append(q)
            xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
            bx = (min(xs), min(ys), max(xs), max(ys))
            if len(comp) >= 26 and bx[2] - bx[0] >= 5 and bx[3] - bx[1] >= 5 and (best is None or len(comp) > best[0]): best = (len(comp), bx)
    return best and best[1]

def one(root, d, p, dry):
    td = os.path.join(root, d, "t0"); f = os.path.join(td, p + ".png")
    if not os.path.exists(f): return None
    bk = os.path.join(td, "_baked"); os.makedirs(bk, exist_ok=True)
    src = os.path.join(bk, p + ".png")
    if not os.path.exists(src): shutil.copy(f, src)
    im = Image.open(src).convert("RGBA")
    box = EXTRA.get((d, p)) or ball_box(im)
    if not box:
        shutil.copy(src, f); return d, p, "no ball"
    if dry: return d, p, box
    m = Image.new("RGB", im.size, (0, 0, 0))
    m.paste((255, 255, 255), (max(0, box[0] - 1), max(0, box[1] - 1), min(96, box[2] + 2), min(96, box[3] + 2)))
    held = box[1] < 70 and p in ("gk_catch_chest", "gk_hold_ball", "gk_scoop")
    desc = ("his grey goalkeeper gloves and hands, empty, no ball" if held else
            "nothing here: transparent background, or his body/boot continuing naturally; no ball")
    r = pl.run_job("/inpaint-v3", {"description": desc, "inpainting_image": {"image": pl.b64img(im), "size": {"width": 96, "height": 96}}, "mask_image": {"image": pl.b64img(m), "size": {"width": 96, "height": 96}},
                                   "seed": 7, "no_background": True}, tag="gk_inpaint_" + d + "_" + p)
    k = pl.to_img((r.get("images") or [r.get("image")])[0])
    if k.size != im.size: k = k.resize(im.size, Image.NEAREST)
    out = im.copy(); out.paste(k.crop((box[0] - 1, box[1] - 1, box[2] + 2, box[3] + 2)), (box[0] - 1, box[1] - 1))
    out.save(f); return d, p, box

if __name__ == "__main__":
    root = sys.argv[1]; dry = "--dry" in sys.argv
    dirs = [x for x in os.listdir(root) if os.path.isdir(os.path.join(root, x, "t0"))]
    with cf.ThreadPoolExecutor(10) as ex:
        for fu in cf.as_completed([ex.submit(one, root, d, p, dry) for d in dirs for p in POSES]):
            try: print(fu.result(), flush=True)
            except Exception as e: print("FAILED", e, flush=True)
