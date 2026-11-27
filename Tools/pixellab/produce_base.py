"""v0.8 base-body pilot producer: a body in the template kit -> 8 rotations -> one PixelLab job per ANIMATION per
generated direction (poses_base.OUT_JOBS / KEEP_JOBS), optionally several takes (seeds) to pick from.

  python -I Tools/pixellab/produce_base.py --bodies B01 BK1 --takes 2 --spend 40

Resumable (skips takes whose frames exist) and capped: stops starting jobs once the balance has fallen by --spend.
Output: ArtWork/raw_base/<body>/rot_<dir>.png, <dir>/_rest.json, <dir>/t<take>/<pose>.png (+ _kps_<job>.json)."""
import argparse, json, os, shutil, sys, threading, time
from concurrent.futures import ThreadPoolExecutor, as_completed
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [HERE, os.path.join(ROOT, "Tools", "pipeline")]
import plclient, poses, poses_base, kit
from PIL import Image

RAW = os.path.join(ROOT, "ArtWork", os.environ.get("PF_RAW_BASE", "raw_base"))   # v0.9.5: raw_base_w = the stockier bodies
GEN = ["south", "south-east", "east", "north-east", "north"]
IDLES = os.path.join(ROOT, "ArtSource", "Packs", "R08_93_Clubs_South_Idle_Only")
BODIES = {
    # outfield: the v0.6 template-kit body O01 (owner's Portsmouth F001 idle); its rotations already exist (free)
    "B01": dict(kind="outfield", reuse="O01", desc="retro 16-bit pixel art footballer, fair skin, dark brown short crew cut, "
                "tall slender athletic build, blue shirt with pink trim, green shorts, red socks, black boots, empty hands, no ball"),
    # keeper: the owner's Chelsea G014 keeper idle, recoloured into the template kit
    "BK1": dict(kind="keeper", src="Clubs/chelsea/South_Native64/keeper2/G014.png", club="chelsea", kit="keeper2",
                desc="retro 16-bit pixel art football goalkeeper, warm brown skin, short black hair, blue goalkeeper jersey "
                "with magenta long sleeves, green shorts, red socks, grey goalkeeper gloves, black boots, no ball"),
}
# v0.8 full set (owner OK after the pilot): 5 more outfield bodies (compact hair: big hairstyles break up when
# animated, v0.7) from the v0.6 template bodies, and a second keeper
def _cast():
    c = json.load(open(os.path.join(HERE, "cast.json")))
    return {e["id"]: e for e in c}
for _bid, _src in {"B02": "O11", "B03": "O06", "B04": "O12", "B05": "O14", "B06": "O09"}.items():
    BODIES[_bid] = dict(kind="outfield", reuse=_src, desc=_cast()[_src]["desc"])
# O14's v0.6 back turns grew a pink and yellow sash its front (the owner's idle design) doesn't have: the builder paints
# it out (build_base_atlas.no_sash)
BODIES["B05"]["sash"] = ["north", "north-east", "north-west"]
BODIES["BK2"] = dict(kind="keeper", src="Clubs/newcastle-united/South_Native64/keeper1/G003.png", club="newcastle-united", kit="keeper1",
                     desc="retro 16-bit pixel art football goalkeeper, deep brown skin, brown short crew cut, blue goalkeeper jersey "
                     "with magenta long sleeves, green shorts, red socks, grey goalkeeper gloves, black boots, no ball")
lock = threading.Lock(); state = {"start": None, "spend": 40.0, "stop": False}

def say(*a):
    with lock: print(time.strftime("%H:%M:%S"), *a, flush=True)

def budget_ok():
    if state["stop"]: return False
    usd = plclient.balance()["credits"]["usd"]
    if state["start"] - usd >= state["spend"]:
        state["stop"] = True; say(f"SPEND CAP reached (${state['start'] - usd:.2f}), no new jobs"); return False
    return True

def prepare(bid):
    b = BODIES[bid]; d = os.path.join(RAW, bid); os.makedirs(d, exist_ok=True)
    if b.get("reuse"):
        src = os.path.join(ROOT, "ArtWork", "raw", b["reuse"])
        for f in ["ref96.png"] + [f"rot_{x}.png" for x in plclient.DIRS8]:
            if not os.path.exists(os.path.join(d, f)): shutil.copy(os.path.join(src, f), os.path.join(d, f))
        for x in GEN:
            os.makedirs(os.path.join(d, x), exist_ok=True)
            r = os.path.join(src, x, "_rest.json")
            if os.path.exists(r) and not os.path.exists(os.path.join(d, x, "_rest.json")): shutil.copy(r, os.path.join(d, x, "_rest.json"))
        return True
    ref = os.path.join(d, "ref96.png")
    if not os.path.exists(ref):
        im = Image.open(os.path.join(IDLES, b["src"])).convert("RGBA")
        kits = {k["slug"]: k for k in json.load(open(os.path.join(IDLES, "Club_Kit_Palettes.json")))}
        im, _ = kit.to_template_generic(im, kits[b["club"]][b["kit"]])   # any kit (to_template knows Portsmouth's home slot only)
        cv = Image.new("RGBA", (96, 96), (0, 0, 0, 0)); cv.paste(im, (16, 24), im); cv.save(ref)
    if not all(os.path.exists(os.path.join(d, f"rot_{x}.png")) for x in plclient.DIRS8):
        if not budget_ok(): return False
        rots = plclient.rotations8(Image.open(ref), description=b["desc"], tag=f"{bid}-rot")
        for x, im in zip(plclient.DIRS8, rots): im.save(os.path.join(d, f"rot_{x}.png"))
        say(bid, "rotations done")
    return True

def job(bid, direction, take, name, batch, action):
    b = BODIES[bid]; d = os.path.join(RAW, bid, direction); td = os.path.join(d, f"t{take}"); os.makedirs(td, exist_ok=True)
    if all(os.path.exists(os.path.join(td, f"{n}.png")) for n in batch): return "skip"
    if not budget_ok(): return "budget"
    idle = Image.open(os.path.join(RAW, bid, f"rot_{direction}.png"))
    kpf = os.path.join(d, "_rest.json")
    if os.path.exists(kpf): rest = json.load(open(kpf))
    else:
        rest = plclient.estimate_skeleton(idle)["keypoints"]
        for k in rest: k["z_index"] = int(round(k.get("z_index", 0)))
        json.dump(rest, open(kpf, "w"))
    kps = [poses.keypoints_for(poses_base.B[n], direction, rest) for n in batch]
    imgs, _ = plclient.pose_frames(idle, rest, kps, b["desc"], action, direction, seed=1000 * take + 7,
                                   tag=f"base/{bid}-{direction}-{name}-t{take}")
    for n, im in zip(batch, imgs): im.save(os.path.join(td, f"{n}.png"))
    json.dump({"batch": batch, "kps": kps}, open(os.path.join(td, f"_kps_{name}.json"), "w"))
    return "ok"

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--bodies", nargs="+", default=list(BODIES)); ap.add_argument("--takes", type=int, default=1)
    ap.add_argument("--spend", type=float, default=40.0); ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--only", nargs="*", default=None, help="job names to run (default: all)")
    a = ap.parse_args()
    state["start"] = plclient.balance()["credits"]["usd"]; state["spend"] = a.spend
    say(f"balance ${state['start']:.2f}, spend cap ${a.spend:.2f}")
    tasks = []
    for bid in a.bodies:
        if not prepare(bid): continue
        jobs = poses_base.KEEP_JOBS if BODIES[bid]["kind"] == "keeper" else poses_base.OUT_JOBS
        for take in range(a.takes):
            for j in jobs:
                name, batch, action = j[:3]; dirs = (j[3] if len(j) > 3 else None) or GEN   # v0.9: a job may limit its views
                if a.only and name not in a.only: continue
                for x in dirs: tasks.append((bid, x, take, name, batch, action))
    with ThreadPoolExecutor(a.workers) as ex:
        fut = {ex.submit(job, *t): t for t in tasks}
        for f in as_completed(fut):
            t = fut[f]
            try: say(t[0], t[1], f"t{t[2]}", t[3], f.result())
            except Exception as e: say(t[0], t[1], f"t{t[2]}", t[3], "FAILED", str(e)[:200])
    end = plclient.balance()["credits"]["usd"]
    say(f"done: balance ${end:.2f}, spent ${state['start'] - end:.2f}")

if __name__ == "__main__":
    main()
