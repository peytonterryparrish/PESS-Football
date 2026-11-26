"""v0.9 fans that react (owner: "the fans need to be a little bit more alive ... when a big moment happens, they cheer
or they go no"). Each of the owner's 20 standing supporters (the fan pack, GameData/Fans) is animated by PixelLab from
its own idle: a 3-frame cheer and a 3-frame "no!" (hands on head, then slumping).

  python -I Tools/pixellab/produce_fans.py --takes 1 --spend 3

Output: ArtWork/raw_fans/F<nn>/<job>/t<take>/<pose>.png (96 px canvas, the 64 px fan pasted at 16, 24).
Tools/pipeline/fans.py packs them into GameData/Fans/fans.png rows 2-7."""
import argparse, json, os, sys, threading, time
from concurrent.futures import ThreadPoolExecutor, as_completed
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [HERE]
import plclient, poses
from PIL import Image

RAW = os.path.join(ROOT, "ArtWork", "raw_fans")
SHEET = os.path.join(ROOT, "GameData", "Fans", "fans.png")   # row 1 = the 20 standing idles (64 px cells)
P = {
    # arms out in a wide V: straight up reads as "arms over the face" from the front (take 0 audit)
    "fan_up":    dict(lean=-6, rise=0.02, r_sh=0, l_sh=0, r_shabd=155, l_shabd=155, r_el=0, l_el=0, head=-15),
    "fan_jump":  dict(lean=-10, rise=0.10, r_knee=25, l_knee=25, r_hip=15, l_hip=15, r_sh=0, l_sh=0, r_shabd=165, l_shabd=165, r_el=5, l_el=5, head=-20),
    "fan_fist":  dict(lean=-4, r_sh=0, r_shabd=160, r_el=30, l_sh=40, l_el=100, head=-10),
    "fan_head":  dict(lean=-6, r_sh=150, r_el=140, r_shabd=60, l_sh=150, l_el=140, l_shabd=60, head=-20),
    "fan_bow":   dict(lean=22, r_sh=140, r_el=145, r_shabd=55, l_sh=140, l_el=145, l_shabd=55, head=25),
    "fan_slump": dict(lean=14, drop=0.03, r_sh=5, l_sh=5, r_shabd=6, l_shabd=6, r_el=15, l_el=15, head=30),
}
JOBS = [("cheer", ["fan_up", "fan_jump", "fan_fist"], "football supporter in the stands celebrating a goal, both arms raised wide above the head in a V, face visible, jumping"),
        ("no", ["fan_head", "fan_bow", "fan_slump"], "football supporter in the stands, hands on head in disbelief, then slumping, disappointed")]
DESC = "retro 16-bit pixel art football supporter standing in the stands, front view, same clothes and colours as the reference"
lock = threading.Lock(); state = {"start": 0.0, "spend": 3.0, "stop": False}

def say(*a):
    with lock: print(time.strftime("%H:%M:%S"), *a, flush=True)

def budget_ok():
    if state["stop"]: return False
    if state["start"] - plclient.balance()["credits"]["usd"] >= state["spend"]:
        state["stop"] = True; say("SPEND CAP reached"); return False
    return True

def idle(f):
    sh = Image.open(SHEET).convert("RGBA")
    cell = sh.crop((f * 64, 64, f * 64 + 64, 128))
    cv = Image.new("RGBA", (96, 96), (0, 0, 0, 0)); cv.paste(cell, (16, 24), cell)
    return cv

def job(f, take, name, batch, action):
    d = os.path.join(RAW, f"F{f:02d}"); td = os.path.join(d, name, f"t{take}"); os.makedirs(td, exist_ok=True)
    if all(os.path.exists(os.path.join(td, f"{n}.png")) for n in batch): return "skip"
    if not budget_ok(): return "budget"
    im = idle(f)
    kpf = os.path.join(d, "_rest.json")
    if os.path.exists(kpf): rest = json.load(open(kpf))
    else:
        rest = plclient.estimate_skeleton(im)["keypoints"]
        for k in rest: k["z_index"] = int(round(k.get("z_index", 0)))
        json.dump(rest, open(kpf, "w"))
    kps = [poses.keypoints_for(P[n], "south", rest) for n in batch]
    imgs, _ = plclient.pose_frames(im, rest, kps, DESC, action, "south", seed=1000 * take + 11, tag=f"fans/F{f:02d}-{name}-t{take}")
    for n, x in zip(batch, imgs): x.save(os.path.join(td, f"{n}.png"))
    return "ok"

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--takes", type=int, default=1); ap.add_argument("--first-take", type=int, default=0)
    ap.add_argument("--spend", type=float, default=3.0); ap.add_argument("--fans", nargs="*", type=int, default=list(range(20))); ap.add_argument("--only", nargs="*", default=None)
    a = ap.parse_args()
    state["start"] = plclient.balance()["credits"]["usd"]; state["spend"] = a.spend
    say(f"balance ${state['start']:.2f}")
    tasks = [(f, t, n, b, act) for f in a.fans for t in range(a.first_take, a.first_take + a.takes) for n, b, act in JOBS if not a.only or n in a.only]
    with ThreadPoolExecutor(4) as ex:
        fut = {ex.submit(job, *t): t for t in tasks}
        for x in as_completed(fut):
            t = fut[x]
            try: say(f"F{t[0]:02d}", t[2], f"t{t[1]}", x.result())
            except Exception as e: say(f"F{t[0]:02d}", t[2], "FAILED", str(e)[:200])
    say(f"done: spent ${state['start'] - plclient.balance()['credits']['usd']:.2f}")

if __name__ == "__main__":
    main()
