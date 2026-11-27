"""v0.9.5: the keepers' flat dive frames (owner's R08 pack) made as stocky as the widened base bodies: the body is
flat, so it thickens vertically (x1.3) and lengthens a little (x1.12), then pixen cleans the pixels.
Output: ArtWork/raw_base_w/<keeper>/pack/<name>.png (template kit), picked up by build_base_atlas.py."""
import json, os, sys
from concurrent.futures import ThreadPoolExecutor
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [HERE, os.path.join(ROOT, "Tools", "pipeline")]
import plclient, produce_base, build_club_atlas as BC, kit
CLEAN = ("clean up this pixel art diving goalkeeper sprite: fix jagged or doubled pixels, smooth outline and shading, "
         "hand-drawn 16-bit look; keep the exact proportions, pose and all colours; no black outline, no ground shadow")
def go(bid, name, cell):
    d = os.path.join(ROOT, "ArtWork", "raw_base_w", bid, "pack"); os.makedirs(d, exist_ok=True); out = os.path.join(d, name + ".png")
    if os.path.exists(out): return
    bb = cell.getbbox(); fig = cell.crop(bb)
    fig = fig.resize((int(round(fig.width * 1.12)), int(round(fig.height * 1.3))), Image.NEAREST)
    w = Image.new("RGBA", cell.size, (0, 0, 0, 0)); cx = (bb[0] + bb[2]) // 2
    w.alpha_composite(fig, (max(0, min(96 - fig.width, cx - fig.width // 2)), max(0, bb[3] - fig.height)))
    w.save(os.path.join(d, "_wide_" + name + ".png"))
    r = plclient.run_job("/edit-image-pixen", {"image": plclient.b64img(w), "description": CLEAN, "seed": 11, "no_background": True}, tag=f"widen/{bid}-{name}")
    im = plclient.to_img((r.get("images") or [r.get("image")])[0])
    nb = im.getbbox(); o = Image.new("RGBA", im.size, (0, 0, 0, 0)); o.paste(im, (0, w.getbbox()[3] - nb[3])); o.save(out)
kits = {k["slug"]: k for k in json.load(open(os.path.join(produce_base.IDLES, "Club_Kit_Palettes.json")))}
tasks = []
for bid in sys.argv[1:]:
    body = produce_base.BODIES[bid]; player = os.path.splitext(os.path.basename(body["src"]))[0]
    for name, cell in BC.pack_dives(body["club"], player, body["kit"]):
        cell, _ = kit.to_template(cell, kits[body["club"]][body["kit"]]); tasks.append((bid, name, cell))
with ThreadPoolExecutor(6) as ex: list(ex.map(lambda t: go(*t), tasks))
print("done", len(tasks))
