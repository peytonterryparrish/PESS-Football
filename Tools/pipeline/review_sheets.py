"""Review sheets for the owner: per character, 8 idle directions + every pose facing south-east (from the built atlas,
kit-previewed), plus one sheet of a single body in many club kits.  -> ArtWork/review/*.png"""
import json, os, sys
from PIL import Image, ImageDraw
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "pipeline"))
import kit

SPR = os.path.join(ROOT, "GameData", "Sprites")
OUT = os.path.join(ROOT, "ArtWork", "review"); os.makedirs(OUT, exist_ok=True)
DIRS8 = ["south", "south-east", "east", "north-east", "north", "north-west", "west", "south-west"]
kits = {k["slug"]: k for k in json.load(open(os.path.join(ROOT, "ArtSource/Packs/R08_93_Clubs_South_Idle_Only/Club_Kit_Palettes.json")))}
BG = (52, 118, 58, 255)

def cell(atlas, meta, key):
    f = meta["frames"].get(key)
    if not f: return None
    c, r, fl = f; C = meta["cell"]
    im = atlas.crop((c * C, r * C, c * C + C, r * C + C))
    if fl: im = im.transpose(Image.FLIP_LEFT_RIGHT)
    if meta.get("kit_keyed"): im = kit.preview_kit(im, kits["newcastle-united"]["home"])
    return im

def sheet(cid):
    meta = json.load(open(os.path.join(SPR, cid + ".json")))
    atlas = Image.open(os.path.join(SPR, cid + ".png")).convert("RGBA")
    poses = sorted({k.split("/", 1)[1] for k in meta["frames"]} - {"idle"})
    rows = [("8 directions", [(d, cell(atlas, meta, f"{d}/idle")) for d in DIRS8])]
    se = [(p, cell(atlas, meta, f"south-east/{p}")) for p in poses]
    for i in range(0, len(se), 12): rows.append(("south-east" if i == 0 else "", se[i:i + 12]))
    S = 2; C = meta["cell"] * S; LH = 12
    W = 12 * C + 110; H = len(rows) * (C + LH) + 30
    img = Image.new("RGBA", (W, H), BG); d = ImageDraw.Draw(img)
    d.text((8, 8), f"{cid}  ({meta['kind']}, {len(meta['frames'])} frames incl. mirrored)", fill=(255, 255, 255, 255))
    y = 30
    for label, items in rows:
        d.text((6, y + C // 2), label, fill=(255, 240, 160, 255))
        for i, (name, im) in enumerate(items):
            x = 110 + i * C
            d.text((x + 2, y), name[:16], fill=(230, 230, 230, 255))
            if im: img.alpha_composite(im.resize((C, C), Image.NEAREST), (x, y + LH))
        y += C + LH
    img.save(os.path.join(OUT, cid + ".png"))

def kit_sheet(cid="O01", pose="south-east/run_contactR"):
    meta = json.load(open(os.path.join(SPR, cid + ".json")))
    atlas = Image.open(os.path.join(SPR, cid + ".png")).convert("RGBA")
    c, r, fl = meta["frames"][pose]; C = meta["cell"]
    base = atlas.crop((c * C, r * C, c * C + C, r * C + C))
    slugs = ["arsenal", "newcastle-united", "liverpool", "aston-villa", "brighton-hove-albion", "blackburn-rovers", "sheffield-united", "west-bromwich-albion",
             "queens-park-rangers", "norwich-city", "wolverhampton-wanderers", "coventry-city", "leeds-united", "everton", "portsmouth", "bradford-city"]
    slugs = [s for s in slugs if s in kits]
    S = 3; W = len(slugs) // 2 * C * S; img = Image.new("RGBA", (W, 2 * C * S), BG)
    for i, s in enumerate(slugs):
        k = dict(kits[s]["home"]); k["pattern"] = k.get("pattern", "solid")
        im = kit.preview_kit(base, k)
        img.alpha_composite(im.resize((C * S, C * S), Image.NEAREST), ((i % (len(slugs) // 2)) * C * S, (i // (len(slugs) // 2)) * C * S))
    img.save(os.path.join(OUT, "kits.png"))

if __name__ == "__main__":
    ids = sorted(f[:-5] for f in os.listdir(SPR) if f.endswith(".json"))
    for cid in ids: sheet(cid)
    kit_sheet()
    print("review sheets", len(ids))
