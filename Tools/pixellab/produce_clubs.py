"""v0.7 per-club production: every club's OWN kit art (from the owner's R08 south idles) is turned into a full
exaggerated pose set (poses_x.SETS_X), so each club plays in its real jersey instead of a recoloured template.

  python -I Tools/pixellab/produce_clubs.py --clubs newcastle-united --workers 8 --floor 100
  python -I Tools/pixellab/produce_clubs.py --all --workers 10 --floor 100

Characters per club (clubs.py chooses them): CHARS below. Output: ArtWork/raw_clubs/<slug>/<char>/
(rot_<dir>.png, <dir>/<pose>.png). Resumable: anything already on disk is skipped. Budget-capped by --floor
(the account balance below which no new job starts)."""
import argparse, json, os, sys, threading, time, csv
from concurrent.futures import ThreadPoolExecutor, as_completed
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [HERE]
import plclient, poses, poses_x
from PIL import Image

PACK = os.path.join(ROOT, "ArtSource", "Packs", "R08_93_Clubs_South_Idle_Only")
RAW = os.path.join(ROOT, "ArtWork", "raw_clubs")
GEN_DIRS = ["south", "south-east", "east", "north-east", "north"]
# per club (owner, v0.7): one body in the home kit, one in the away kit, one keeper (first-choice kit)
CHARS = [("home", 0), ("away", 0), ("keeper1", 0)]
BIG_HAIR = {"large high afro", "rounded afro", "large sweeping waves", "long loose dreadlocks", "long tied-back dreadlocks"}
lock = threading.Lock()
budget = {"floor": 100.0, "stop": False, "last": 0.0, "usd": 1e9}

def say(*a):
    with lock: print(time.strftime("%H:%M:%S"), *a, flush=True)

def check_budget():
    if budget["stop"]: return False
    with lock:
        if time.time() - budget["last"] > 20:
            budget["usd"] = plclient.balance()["credits"]["usd"]; budget["last"] = time.time()
        usd = budget["usd"]
    if usd < budget["floor"]:
        budget["stop"] = True; say(f"BUDGET FLOOR reached (${usd:.2f} < ${budget['floor']}), stopping new jobs"); return False
    return True

_REF = None
def ref_rows():
    global _REF
    if _REF is None: _REF = {r["id"]: r for r in csv.DictReader(open(os.path.join(PACK, "Character_Reference.csv")))}
    return _REF

def pick_players(slug, kit, n=2):
    """Different looks for variety: the first body, then the most different skin tone after it."""
    R = ref_rows()
    # big hair (tall afros, long loose dreadlocks, big waves) comes out of the 8-rotation step as a block sitting on
    # top of the head (v0.6 "hair floating on top of heads"), so those players are not used as animated bodies
    files = [f for f in sorted(os.listdir(os.path.join(PACK, "Clubs", slug, "South_Native64", kit)))
             if R.get(f[:-4], {}).get("hair", "") not in BIG_HAIR] or sorted(os.listdir(os.path.join(PACK, "Clubs", slug, "South_Native64", kit)))
    # every club's pack has the same 130 faces, so each club starts at a different one (variety across the league);
    # a body that already has its rotations on disk is kept (resumable runs never switch players)
    done = [f for f in files if os.path.exists(os.path.join(RAW, slug, f"{kit}_{f[:-4]}", "rot_south.png"))]
    import zlib
    first = done[0] if done else files[zlib.crc32(slug.encode()) % len(files)]; out = [first]
    t0 = R.get(first[:-4], {}).get("tone", "")
    for f in files[1:]:
        if len(out) >= n: break
        if R.get(f[:-4], {}).get("tone", "") != t0: out.append(f)
    for f in files[1:]:
        if len(out) >= n: break
        if f not in out: out.append(f)
    return out

def cast_for(slug, kits):
    pal = {k["slug"]: k for k in json.load(open(os.path.join(PACK, "Club_Kit_Palettes.json")))}[slug]
    R = ref_rows(); cast = []
    for kit, i in CHARS:
        players = pick_players(slug, kit)
        if i >= len(players): continue
        f = players[i]; r = R.get(f[:-4], {}); k = pal[kit]
        gk = kit.startswith("keeper")
        look = f"{r.get('tone', 'fair').split('/')[0].strip()} skin, {r.get('color', '')} {r.get('hair', '')}, {r.get('build', 'athletic')} build"
        kitdesc = f"{k['pattern']} {k['shirt']} shirt with {k['trim']} trim, {k['shorts']} shorts, {k['socks']} socks"
        cast.append({
            "id": f"{kit}_{f[:-4]}", "slug": slug, "kind": "keeper" if gk else "outfield",
            "src": os.path.join(PACK, "Clubs", slug, "South_Native64", kit, f),
            "desc": f"retro 16-bit pixel art {'goalkeeper with gloves' if gk else 'footballer'}, {look}, {kitdesc}, black boots, empty hands, no ball"})
    return cast

def prepare(c):
    d = os.path.join(RAW, c["slug"], c["id"]); os.makedirs(d, exist_ok=True)
    ref = os.path.join(d, "ref96.png")
    if not os.path.exists(ref):
        im = Image.open(c["src"]).convert("RGBA")
        cv = Image.new("RGBA", (96, 96), (0, 0, 0, 0)); cv.paste(im, (16, 24), im); cv.save(ref)
    if not all(os.path.exists(os.path.join(d, f"rot_{x}.png")) for x in plclient.DIRS8):
        if not check_budget(): return False
        rots = plclient.rotations8(Image.open(ref), description=c["desc"], tag=f"{c['slug']}/{c['id']}-rot")
        for x, im in zip(plclient.DIRS8, rots): im.save(os.path.join(d, f"rot_{x}.png"))
        say(c["slug"], c["id"], "rotations done")
    return True

def pose_job(c, direction, bi, batch):
    d = os.path.join(RAW, c["slug"], c["id"], direction); os.makedirs(d, exist_ok=True)
    if all(os.path.exists(os.path.join(d, f"{n}.png")) for n in batch): return "skip"
    if not check_budget(): return "budget"
    idle = Image.open(os.path.join(RAW, c["slug"], c["id"], f"rot_{direction}.png"))
    kpf = os.path.join(d, "_rest.json")
    if os.path.exists(kpf): rest = json.load(open(kpf))
    else:
        rest = plclient.estimate_skeleton(idle)["keypoints"]
        for k in rest: k["z_index"] = int(round(k.get("z_index", 0)))
        json.dump(rest, open(kpf, "w"))
    kps = [poses.keypoints_for(poses.P[n], direction, rest) for n in batch]
    imgs, _ = plclient.pose_frames(idle, rest, kps, c["desc"], "football player poses, exaggerated arcade action, no ball",
                                   direction, tag=f"{c['slug']}/{c['id']}-{direction}-b{bi}")
    for n, im in zip(batch, imgs): im.save(os.path.join(d, f"{n}.png"))
    json.dump({"batch": batch, "kps": kps}, open(os.path.join(d, f"_kps_b{bi}.json"), "w"))
    return "ok"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clubs", default=""); ap.add_argument("--all", action="store_true")
    ap.add_argument("--workers", type=int, default=8); ap.add_argument("--floor", type=float, default=100.0)
    ap.add_argument("--reverse", action="store_true"); ap.add_argument("--chars", default=""); ap.add_argument("--dirs", default=""); ap.add_argument("--batches", default="")
    a = ap.parse_args(); budget["floor"] = a.floor
    # --all goes division by division (the clubs most often played finish first)
    slugs = [r["Slug"] for r in csv.DictReader(open(os.path.join(PACK, "Clubs_by_Division.csv"), encoding="utf-8"))] if a.all else [s for s in a.clubs.split(",") if s]
    if a.reverse: slugs = slugs[::-1]     # a second runner works from the other end; resumable files keep them apart
    cast = [c for s in slugs for c in cast_for(s, None)]
    if a.chars: cast = [c for c in cast if c["id"].split("_")[0] in a.chars.split(",") or c["id"] in a.chars.split(",")]
    dirs = a.dirs.split(",") if a.dirs else GEN_DIRS
    say(f"clubs {len(slugs)}  characters {len(cast)}  balance ${plclient.balance()['credits']['usd']:.2f}  floor ${a.floor}")
    with ThreadPoolExecutor(a.workers) as ex:
        futs = {}
        # rotations first (each club's pose jobs need them), then poses; interleaved per club so whole clubs finish early
        ready = [c for c, ok in zip(cast, ex.map(prepare, cast)) if ok]
        for c in ready:
            for bi, batch in enumerate(poses_x.SETS_X[c["kind"]]):
                if a.batches and str(bi) not in a.batches.split(","): continue
                for direction in dirs:
                    futs[ex.submit(pose_job, c, direction, bi, batch)] = (c["slug"], c["id"], direction, bi)
        done = 0
        for f in as_completed(futs):
            done += 1
            try: r = f.result()
            except Exception as e: r = "ERROR " + str(e)[:300]
            say(f"[{done}/{len(futs)}]", *futs[f], r)
    say(f"finished  balance ${plclient.balance()['credits']['usd']:.2f}")

if __name__ == "__main__":
    main()
