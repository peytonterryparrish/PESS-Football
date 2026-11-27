"""v0.9.8: record every animation candidate (ArtWork/anim_candidates) as a looping GIF on grass at 2x, at the action's
game timing, and write the approval gallery ArtWork/anim_gallery/index.html (template: anim_gallery_tpl.html).
  python Tools/pixellab/rec_candidates.py"""
import os, sys, json, glob, html
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from anim_candidates import SPEC
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "ArtWork", "anim_candidates"); OUT = os.path.join(ROOT, "ArtWork", "anim_gallery")
GRASS = (61, 125, 63)
TITLE = {"idle": "Idle", "walk": "Walk", "run": "Run", "sprint": "Sprint", "dribble": "Dribble", "pass": "Pass", "shot": "Shot",
         "volley": "Volley", "header": "Header", "chest": "Chest control", "tackle": "Standing tackle", "slide": "Slide tackle",
         "fall": "Fall and get up", "throwin": "Throw-in", "celebrate": "Goal celebration", "gk_ready": "Keeper ready stance",
         "gk_catch": "Keeper chest catch", "gk_high": "Keeper high catch", "gk_scoop": "Keeper low gather", "gk_punch": "Keeper punch",
         "gk_dive_low": "Keeper dive, low", "gk_dive_high": "Keeper dive, high", "gk_throw": "Keeper throw", "gk_punt": "Keeper punt",
         "gk_goalkick": "Keeper goal kick"}
GROUP = [("Moving", ["idle", "walk", "run", "sprint", "dribble"]), ("Goalkeeper dives", ["gk_dive_low", "gk_dive_high"]),
         ("Goal kick", ["gk_goalkick"]), ("On the ball", ["pass", "shot", "volley", "header", "chest", "throwin"]),
         ("Defending and falls", ["tackle", "slide", "fall"]), ("Goalkeeper", ["gk_ready", "gk_catch", "gk_high", "gk_scoop", "gk_punch", "gk_throw", "gk_punt"]),
         ("Celebrations", ["celebrate"])]

def gif(d, out):
    m = json.load(open(os.path.join(d, "meta.json")))
    fr = [Image.open(f).convert("RGBA") for f in sorted(glob.glob(os.path.join(d, "f*.png")))]
    if m["loop"] and len(fr) > 2: fr = fr[:-1]
    box = None
    for f in fr:
        b = f.getbbox()
        if b: box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    W, H = fr[0].size; pad = 6
    box = (max(0, box[0] - pad), max(0, box[1] - pad), min(W, box[2] + pad), min(H, box[3] + pad))
    bw, bh = box[2] - box[0], box[3] - box[1]
    side = max(bw, bh, 72)   # square cell, figure centred, feet kept low
    ims = []
    for f in fr:
        bg = Image.new("RGBA", (side, side), GRASS + (255,))
        bg.alpha_composite(f.crop(box), ((side - bw) // 2, side - bh - 2))
        ims.append(bg.convert("RGB").resize((side * 2, side * 2), Image.NEAREST).quantize(64, dither=Image.Dither.NONE))
    dt = int(m["dt"] * 1000)
    durs = [dt] * len(ims)
    if not m["loop"]: durs[0] = 350; durs[-1] = 700
    ims[0].save(out, save_all=True, append_images=ims[1:], duration=durs, loop=0, disposal=1, optimize=True)
    return m

if __name__ == "__main__":
    secs = []; n = 0
    for gname, acts in GROUP:
        parts = []
        for a in acts:
            ds = sorted(glob.glob(os.path.join(SRC, a, "[0-9][0-9]")))
            ids = []
            for d in ds:
                if not os.path.exists(os.path.join(d, "meta.json")): continue
                cid = f"{a}_{os.path.basename(d)}"
                m = gif(d, os.path.join(OUT, "img", cid + ".gif")); ids.append(cid); n += 1
            if not ids: continue
            sp = SPEC[a]
            parts.append(f'<section id="s-{a}"><div class="head"><span class="lab">{html.escape(gname)} · {len(ids)} takes · {"loop" if sp[2] else "plays once"} · {sp[3]} frames at {int(sp[5]*1000)} ms</span>'
                         f'<h2>{html.escape(TITLE[a])}</h2><p>{html.escape(sp[6][0].upper() + sp[6][1:])}.</p></div><div class="grid" data-ids="{",".join(ids)}"></div></section>')
        secs += parts
    nav = " ".join(f'<a href="#s-{a}">{html.escape(TITLE[a])}</a>' for g, acts in GROUP for a in acts if os.path.isdir(os.path.join(SRC, a)))
    tpl = open(os.path.join(os.path.dirname(__file__), "anim_gallery_tpl.html"), encoding="utf8").read()
    open(os.path.join(OUT, "index.html"), "w", encoding="utf8").write(tpl.replace("%%SECTIONS%%", "\n".join(secs)).replace("%%NAV%%", nav).replace("%%N%%", str(n)))
    print("gifs", n)
