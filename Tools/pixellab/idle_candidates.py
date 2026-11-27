"""v0.9.6 art reset: ~100 south idle candidates for one model in one kit, styled from the user's R08 south idle pack.
Each batch is one PixelLab job returning 16 images at 64x64 (the R08 native size). Writes
ArtWork/idle_candidates/<batch>/NN.png and a sheet per batch. Nothing here touches the game's atlases."""
import os, sys, json, concurrent.futures as cf
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import plclient as pl

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
R08 = os.path.join(ROOT, "ArtSource", "Packs", "R08_93_Clubs_South_Idle_Only", "Clubs", "arsenal", "South_Native64", "home")
OUT = os.path.join(ROOT, "ArtWork", "idle_candidates")
REFS = ["F001", "F005", "F021", "F041"]

KIT = "plain red short-sleeved football shirt, white shorts, red socks, black boots, short dark hair, light skin"
BASE = "one retro 16-bit football player, full body, standing still facing the viewer (front view, south), feet apart on the ground, " + KIT
BATCHES = {
    "A_r08_relaxed": ("style", BASE + ", relaxed idle stance, arms hanging a little away from the body so a gap shows between arms and torso"),
    "B_r08_apose":   ("style", BASE + ", A-pose: both arms angled 30 degrees out from the body, hands clearly apart from the hips and shorts"),
    "C_iss_stocky":  ("style", BASE + ", stocky compact arcade build like a 1990s Super Nintendo football game, slightly larger head, broad shoulders, sturdy legs, arms held out from the sides"),
    "D_tpose":       ("style", BASE + ", T-pose: both arms straight out to the sides at shoulder height, legs slightly apart"),
    "E_ref_apose":   ("ref",   BASE + ", same body, size and pixel style as the reference players, arms held out from the sides in an A-pose so they are separate from the torso"),
    "F_ref_stocky":  ("ref",   BASE + ", same pixel style as the reference players but a stockier 16-bit arcade build with a slightly bigger head, arms apart from the torso"),
    "G_ref_ready":   ("ref",   BASE + ", athletic ready stance, knees a touch bent, arms slightly out and forward, hands away from the body"),
}

def refs():
    return [Image.open(os.path.join(R08, f + ".png")).convert("RGBA") for f in REFS]

def run(name, kind, desc, seed):
    ims = refs()
    if kind == "style":
        body = {"style_images": [{"image": pl.b64img(i), "width": 64, "height": 64} for i in ims],
                "description": desc, "style_description": "detailed realistic 64px pixel art footballer sprites, soft shading, no black outline",
                "seed": seed, "no_background": True}
        r = pl.run_job("/generate-with-style-v2", body, tag="idle_" + name)
    else:
        body = {"description": desc, "image_size": {"width": 64, "height": 64}, "seed": seed, "no_background": True,
                "reference_images": [{"image": pl.b64img(i), "size": {"width": 64, "height": 64},
                                      "usage_description": "the player to match: body, proportions, kit style and shading"} for i in ims[:2]],
                "style_image": {"image": pl.b64img(ims[2]), "size": {"width": 64, "height": 64}},
                "style_options": {"color_palette": False, "outline": True, "detail": True, "shading": True}}
        r = pl.run_job("/generate-image-v2", body, tag="idle_" + name)
    out = [pl.to_img(x) for x in r["images"]]
    d = os.path.join(OUT, name); os.makedirs(d, exist_ok=True)
    for k, im in enumerate(out): im.save(os.path.join(d, "%02d.png" % k))
    return name, len(out)

if __name__ == "__main__":
    only = sys.argv[1:]
    with cf.ThreadPoolExecutor(7) as ex:
        fs = [ex.submit(run, n, k, d, 600 + i) for i, (n, (k, d)) in enumerate(BATCHES.items()) if not only or n in only]
        for f in cf.as_completed(fs):
            try: print(*f.result(), flush=True)
            except Exception as e: print("FAILED", e, flush=True)
