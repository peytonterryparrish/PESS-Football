"""Production runner: base characters -> 8 rotations -> skeleton-posed STILLS per generated direction.
Resumable (skips files that exist), parallel, and budget-capped.

  python -I Tools/pixellab/produce.py Tools/pixellab/cast.json --workers 6 --floor 117

cast.json: [{"id": "O01", "kind": "outfield", "src": "<png path>", "template_kit": true, "desc": "..."}...]
Output: ArtWork/raw/<id>/rot_<dir>.png, ArtWork/raw/<id>/<dir>/<pose>.png, <dir>/_kps.json
"""
import argparse, json, os, sys, threading, time, traceback
from concurrent.futures import ThreadPoolExecutor, as_completed
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [HERE, os.path.join(ROOT, "Tools", "pipeline")]
import plclient, poses, poses_more, kit
from PIL import Image

RAW = os.path.join(ROOT, "ArtWork", "raw")
lock = threading.Lock()
budget = {"floor": 117.0, "stop": False}

def say(*a):
    with lock: print(time.strftime("%H:%M:%S"), *a, flush=True)

def check_budget():
    if budget["stop"]: return False
    usd = plclient.balance()["credits"]["usd"]
    if usd < budget["floor"]:
        budget["stop"] = True; say(f"BUDGET FLOOR reached (${usd:.2f} < ${budget['floor']}), stopping new jobs"); return False
    return True

def prepare(c):
    d = os.path.join(RAW, c["id"]); os.makedirs(d, exist_ok=True)
    ref = os.path.join(d, "ref96.png")
    if not os.path.exists(ref):
        im = Image.open(os.path.join(ROOT, c["src"])).convert("RGBA")
        if c.get("template_kit"):
            kits = {k["slug"]: k for k in json.load(open(os.path.join(ROOT, "ArtSource/Packs/R08_93_Clubs_South_Idle_Only/Club_Kit_Palettes.json")))}
            im, _ = kit.to_template(im, kits["portsmouth"]["home"])
        cv = Image.new("RGBA", (96, 96), (0, 0, 0, 0)); cv.paste(im, (16, 24), im); cv.save(ref)
    if not all(os.path.exists(os.path.join(d, f"rot_{x}.png")) for x in plclient.DIRS8):
        if not check_budget(): return False
        rots = plclient.rotations8(Image.open(ref), description=c["desc"], tag=f"{c['id']}-rot")
        for x, im in zip(plclient.DIRS8, rots): im.save(os.path.join(d, f"rot_{x}.png"))
        say(c["id"], "rotations done")
    return True

def pose_job(c, direction, bi, batch):
    d = os.path.join(RAW, c["id"], direction); os.makedirs(d, exist_ok=True)
    if all(os.path.exists(os.path.join(d, f"{n}.png")) for n in batch): return "skip"
    if not check_budget(): return "budget"
    idle = Image.open(os.path.join(RAW, c["id"], f"rot_{direction}.png"))
    kpf = os.path.join(d, "_rest.json")
    if os.path.exists(kpf): rest = json.load(open(kpf))
    else:
        rest = plclient.estimate_skeleton(idle)["keypoints"]
        for k in rest: k["z_index"] = int(round(k.get("z_index", 0)))
        json.dump(rest, open(kpf, "w"))
    kps = [poses.keypoints_for(poses.P[n], direction, rest) for n in batch]
    imgs, _ = plclient.pose_frames(idle, rest, kps, c["desc"], c.get("action", "football player poses, no ball"),
                                   direction, tag=f"{c['id']}-{direction}-b{bi}")
    for n, im in zip(batch, imgs): im.save(os.path.join(d, f"{n}.png"))
    json.dump({"batch": batch, "kps": kps}, open(os.path.join(d, f"_kps_b{bi}.json"), "w"))
    return "ok"

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("cast"); ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--floor", type=float, default=117.0); ap.add_argument("--only", default="")
    a = ap.parse_args(); budget["floor"] = a.floor
    cast = json.load(open(a.cast))
    if a.only: cast = [c for c in cast if c["id"] in a.only.split(",")]
    say(f"cast {len(cast)}  balance ${plclient.balance()['credits']['usd']:.2f}  floor ${a.floor}")
    with ThreadPoolExecutor(a.workers) as ex:
        ready = [c for c, ok in zip(cast, ex.map(prepare, cast)) if ok]
        futs = {}
        for c in ready:
            for bi, batch in enumerate(poses_more.SETS[c["kind"]]):
                for direction in c.get("dirs", poses_more.GEN_DIRS):
                    futs[ex.submit(pose_job, c, direction, bi, batch)] = (c["id"], direction, bi)
        done = 0
        for f in as_completed(futs):
            done += 1
            try: r = f.result()
            except Exception as e: r = "ERROR " + str(e)[:300]
            say(f"[{done}/{len(futs)}]", *futs[f], r)
    say(f"finished  balance ${plclient.balance()['credits']['usd']:.2f}")

if __name__ == "__main__":
    main()
