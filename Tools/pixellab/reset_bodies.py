"""v0.9.6 art reset, step 3: the chosen south idle (ArtWork/idle_candidates) becomes the game's one outfield body N01 and,
drawn from the same sprite, its keeper NK1. Both are put into the template kit (blue shirt, magenta trim, green shorts,
red socks: the colours the sprite material recolours into every club's kit) and written as
ArtWork/raw_base_n/<id>/ref96.png, ready for produce_base.py (PF_RAW_BASE=raw_base_n).
  python Tools/pixellab/reset_bodies.py C05"""
import os, sys, json
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [HERE, os.path.join(ROOT, "Tools", "pipeline")]
import plclient as pl, kit, redkit

SRC = os.path.join(ROOT, "ArtWork", "idle_candidates")
OUT = os.path.join(ROOT, "ArtWork", "raw_base_n")
BATCH = {"A": "A_r08_relaxed", "B": "B_r08_apose", "C": "C_iss_stocky", "D": "D_tpose", "E": "E_ref_apose", "F": "F_ref_stocky", "G": "G_ref_ready"}
RED_KIT = {"shirt": "#c4161c", "trim": "#c4161c", "shorts": "#f2f2f2", "socks": "#c4161c"}

def ref96(im64):
    cv = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    l, t, r, b = im64.getbbox()
    cv.paste(im64, (48 - (l + r) // 2, 88 - b), im64)    # centred, feet on y=88 (the atlas pivot)
    return cv

def main(cid, seed=11):
    im = Image.open(os.path.join(SRC, BATCH[cid[0]], cid[1:] + ".png")).convert("RGBA")
    tpl, n = redkit.to_template(im)
    print(cid, "kit pixels by region:", n)
    d = os.path.join(OUT, "N01"); os.makedirs(d, exist_ok=True)
    ref96(tpl).save(os.path.join(d, "ref96.png"))
    # the keeper: the same sprite in goalkeeper kit (long magenta sleeves, grey gloves), via a pixen edit
    body = {"image": pl.b64img(ref96(tpl)), "description":
            "turn this footballer into a goalkeeper: give him long sleeves in exactly this magenta (#c040c0) down to the wrists "
            "and plain grey goalkeeper gloves on both hands. Keep everything else exactly the same: face, hair, skin, body "
            "shape, pose, the blue shirt, green shorts, red socks and black boots, same size and position, no outline, no shadow",
            "seed": seed, "no_background": True}
    r = pl.run_job("/edit-image-pixen", body, tag="reset_keeper")
    k = pl.to_img((r.get("images") or [r.get("image")])[0])
    bb = k.getbbox(); k2 = Image.new("RGBA", k.size, (0, 0, 0, 0)); k2.paste(k, (48 - (bb[0] + bb[2]) // 2, 88 - bb[3])); k = k2
    dk = os.path.join(OUT, "NK1"); os.makedirs(dk, exist_ok=True)
    k.save(os.path.join(dk, "ref96.png"))
    json.dump({"from": cid, "seed": seed}, open(os.path.join(OUT, "chosen.json"), "w"))
    print("wrote N01 and NK1 ref96")

if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 11)
