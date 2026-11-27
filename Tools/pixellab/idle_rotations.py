"""v0.9.6 art reset, step 2: 8-direction standing turns for the approved south idle candidates (ArtWork/idle_candidates).
  python Tools/pixellab/idle_rotations.py C00 C01 ...    -> ArtWork/idle_rotations/<id>/<direction>.png + _strip.png"""
import os, sys, concurrent.futures as cf
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import plclient as pl

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "ArtWork", "idle_candidates")
OUT = os.path.join(ROOT, "ArtWork", "idle_rotations")
BATCH = {"A": "A_r08_relaxed", "B": "B_r08_apose", "C": "C_iss_stocky", "D": "D_tpose", "E": "E_ref_apose", "F": "F_ref_stocky", "G": "G_ref_ready"}
DESC = ("retro 16-bit football player, stocky compact arcade build, red short-sleeved shirt, white shorts, red socks, "
        "black boots, standing idle, arms slightly away from the body, same character from every side")

def one(cid):
    im = Image.open(os.path.join(SRC, BATCH[cid[0]], cid[1:] + ".png")).convert("RGBA")
    rots = pl.rotations8(im, DESC, seed=700 + int(cid[1:]), tag="idlerot_" + cid)
    d = os.path.join(OUT, cid); os.makedirs(d, exist_ok=True)
    strip = Image.new("RGBA", (64 * 8, 64), (0, 0, 0, 0))
    for k, (name, r) in enumerate(zip(pl.DIRS8, rots)):
        r.save(os.path.join(d, name + ".png"))
        strip.alpha_composite(r.resize((64, 64), Image.NEAREST) if r.size != (64, 64) else r, (64 * k, 0))
    strip.save(os.path.join(d, "_strip.png"))
    return cid, rots[0].size

if __name__ == "__main__":
    with cf.ThreadPoolExecutor(8) as ex:
        for f in cf.as_completed([ex.submit(one, c) for c in sys.argv[1:]]):
            try: print(*f.result(), flush=True)
            except Exception as e: print("FAILED", e, flush=True)
