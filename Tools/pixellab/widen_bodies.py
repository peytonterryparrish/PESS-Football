"""v0.9.5 (owner: players "look too thin on the field"; ISS Deluxe study): stockier base bodies.
Each body's 8 standing turns are widened 1.3x with a 1.25x head (same 64 px height, legs a touch shorter), then
PixelLab's pixen edit cleans the stretched pixels without changing proportions. Fresh rest skeletons are estimated
for the generated views, so the animation jobs (produce_base.py with PF_RAW_BASE=raw_base_w) pose the new body.

  python -I Tools/pixellab/widen_bodies.py B01 B02 ... [--fx 1.3 --fh 1.25]
Output: ArtWork/raw_base_w/<body>/rot_<dir>.png, ref96.png, <dir>/_rest.json (raw stretch kept as _wide_<dir>.png)."""
import argparse, json, os, sys
from concurrent.futures import ThreadPoolExecutor
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import plclient
SRC = os.path.join(ROOT, "ArtWork", "raw_base"); DST = os.path.join(ROOT, "ArtWork", "raw_base_w")
GEN = ["south", "south-east", "east", "north-east", "north"]
CLEAN = ("clean up this pixel art footballer sprite: fix jagged or doubled pixels, smooth the outline and shading so it "
         "looks hand-drawn 16-bit; keep the exact proportions, width, pose, face and all colours; no black outline, no ground shadow")

def widen(im, fx, fh, head=0.13):
    bb = im.getbbox(); x0, y0, x1, y1 = bb; H = y1 - y0; fig = im.crop(bb)
    hh = int(round(H * head)); hd = fig.crop((0, 0, fig.width, hh)); body = fig.crop((0, hh, fig.width, H))
    hd = hd.resize((int(round(fig.width * fh)), int(round(hh * fh))), Image.NEAREST)
    body = body.resize((int(round(fig.width * fx)), H - hd.height), Image.NEAREST)
    o = Image.new("RGBA", im.size, (0, 0, 0, 0)); cx = (x0 + x1) // 2
    o.alpha_composite(hd, (cx - hd.width // 2, y0)); o.alpha_composite(body, (cx - body.width // 2, y0 + hd.height))
    return o

def one(bid, x, fx, fh):
    d = os.path.join(DST, bid); os.makedirs(os.path.join(d, x), exist_ok=True)
    out = os.path.join(d, f"rot_{x}.png")
    if not os.path.exists(out):
        w = widen(Image.open(os.path.join(SRC, bid, f"rot_{x}.png")).convert("RGBA"), fx, fh)
        w.save(os.path.join(d, f"_wide_{x}.png"))
        r = plclient.run_job("/edit-image-pixen", {"image": plclient.b64img(w), "description": CLEAN, "seed": int(os.environ.get("PF_SEED", "11")), "no_background": True}, tag=f"widen/{bid}-{x}")
        im = plclient.to_img((r.get("images") or [r.get("image")])[0])
        # keep the feet on the original ground line (the cell pivot is (48, 88))
        bb = im.getbbox(); dy = 88 - bb[3]
        if dy: im2 = Image.new("RGBA", im.size, (0, 0, 0, 0)); im2.paste(im, (0, dy)); im = im2
        im.save(out)
    if x in GEN and not os.path.exists(os.path.join(d, x, "_rest.json")):
        rest = plclient.estimate_skeleton(Image.open(out))["keypoints"]
        for k in rest: k["z_index"] = int(round(k.get("z_index", 0)))
        json.dump(rest, open(os.path.join(d, x, "_rest.json"), "w"))
    return bid, x

if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("bodies", nargs="+"); ap.add_argument("--fx", type=float, default=1.3); ap.add_argument("--fh", type=float, default=1.25)
    a = ap.parse_args(); b0 = plclient.balance()["credits"]["usd"]
    with ThreadPoolExecutor(6) as ex:
        for r in ex.map(lambda t: one(*t, a.fx, a.fh), [(b, x) for b in a.bodies for x in plclient.DIRS8]): pass
    for b in a.bodies:
        s = os.path.join(DST, b, "rot_south.png")
        if os.path.exists(s): Image.open(s).save(os.path.join(DST, b, "ref96.png"))
    print("spent", round(b0 - plclient.balance()["credits"]["usd"], 2))
