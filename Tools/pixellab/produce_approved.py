"""v0.9.8 -> v0.9.9: build the owner's approved animations (ArtWork/anim_candidates, picks from the approval gallery) in all
12 directions. For each action the Best take's prompt, seed and frame count are replayed from that take's first frame
turned to each of the 7 drawn directions (south, a030, a060, east, a120, a150, north; the other 5 are mirrors in the
atlas). Output: ArtWork/raw_12/<body>/<dir>/<action>/fNN.png + take.json.
  python Tools/pixellab/produce_approved.py picks.json [action ...]
picks.json: {"run": "07", "gk_dive_low": "03", ...}  (exported from the gallery's db: the Best per action)"""
import os, sys, json, glob, concurrent.futures as cf
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import plclient as pl
from anim_candidates import SPEC, first_frame, clean, qa

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CAND = os.path.join(ROOT, "ArtWork", "anim_candidates"); OUT = os.path.join(ROOT, "ArtWork", "raw_12")
# which drawn 8-direction view each 12-direction one is turned from, and by how much (see plclient.turns12)
TURN = {"south": ("south", 0), "a030": ("south", 30), "a060": ("south", 60), "east": ("east", 0), "a120": ("east", -30),
        "a150": ("north", 30), "north": ("north", 0)}

def start_frame(action, d12):
    """the take's first frame, seen from direction d12"""
    body, src, loop, fc, canvas, dt, take, var = SPEC[action]
    base, deg = TURN[d12]
    if src.startswith("rot_"): s2 = "rot_" + base
    else: s2 = base + "/" + src.split("/")[1]
    im = first_frame(body, s2, canvas, action)
    if deg:
        im = pl.lone(pl.rotate(im, deg, seed=5, tag=f"appr_turn_{action}_{d12}")) if max(im.size) <= 128 else im
    return im

def one(action, take, d12):
    m = json.load(open(os.path.join(CAND, action, take, "meta.json")))
    d = os.path.join(OUT, m["body"], d12, action)
    if os.path.exists(os.path.join(d, "take.json")): return action, d12, "kept"
    ff = start_frame(action, d12)
    side = {"south": "facing the camera", "north": "seen from behind", "east": "side view facing right"}.get(d12, "three-quarter view")
    prompt = m["prompt"].replace("side view", side).replace("facing the camera", side) if d12 != TURN_SRC(action) else m["prompt"]
    body = {"first_frame": pl.b64img(ff), "action": prompt[:1000], "frame_count": SPEC[action][3], "seed": m["seed"], "no_background": True}
    if m["loop"]: body["last_frame"] = pl.b64img(ff)
    for attempt in range(3):
        r = pl.run_job("/animate-with-text-v3", body, tag=f"appr_{action}_{d12}")
        ims = [clean(pl.to_img(x), m["body"]) for x in r["images"]]
        if not qa(action, ims): break
        body["seed"] += 1000
    os.makedirs(d, exist_ok=True)
    for k, im in enumerate(ims): im.save(os.path.join(d, "f%02d.png" % k))
    json.dump({"take": take, "seed": body["seed"], "loop": m["loop"], "dt": m["dt"], "frames": len(ims)}, open(os.path.join(d, "take.json"), "w"))
    return action, d12, len(ims)

def TURN_SRC(action):
    src = SPEC[action][1]
    return src[4:] if src.startswith("rot_") else src.split("/")[0]

if __name__ == "__main__":
    picks = json.load(open(sys.argv[1])); acts = [a for a in sys.argv[2:]] or list(picks)
    with cf.ThreadPoolExecutor(12) as ex:
        for f in cf.as_completed([ex.submit(one, a, picks[a], d) for a in acts for d in pl.DRAWN12]):
            try: print(*f.result(), flush=True)
            except Exception as e: print("FAILED", str(e)[:300], flush=True)
