"""v0.10 sprite audit of the bodies the match draws (N01 outfield, NK1 keeper by default): every frame the game can
look up in GameData/Sprites/<id>.json, the approved 12-direction animations ("<anim>_fNN") and the older 8-direction
poses, checked for the defects the owner reported ("a long brown streak above their head ... something sticking out").

  python -I Tools/pipeline/audit_anim.py [--tag before|after] [--no-sheets] [ids...]

Per physical cell (mirrored frames reuse a cell):
  strays    8-connected opaque islands apart from the main body (detached pixels, loose fragments)
  spur      thin protrusions: pixels a 3x3 opening removes that form a 1-2 px wide run of >= 4 px sticking out of the
            silhouette (hair-line above the head, a line off the neck or arm); "spur_head" when it is above/beside the
            head and hair/skin/brown coloured (the PixelLab rotate artefact)
  ball      a round light blob with dark panels left in an outfield frame
  edge      opaque pixels on the cell's outer 1 px (cut off / bleeding into the neighbour)
  rawkit    non-key pixels in template-kit hues (red socks, magenta trim, blue shirt, green shorts) that will NOT
            recolour with the club kit; keyhead = key (kit) pixels inside the head
Per animation and direction (frames in order, mirrors applied as the game draws them):
  size      opaque area / height off the clip's median (a frame that grows or shrinks)
  head      the head's hair+skin pixels well below the clip's median (head missing or covered)
  jump      low overlap with the previous frame vs the clip's usual overlap (a big pose jump; loops wrap)
  facing    the face (skin) sits on the wrong side of the head vs the direction's own idle (facing the wrong way)
  mirror    a mirrored direction that isn't the exact flip of its source, or a missing mirror
Writes ArtWork/audit_v100/audit_<tag>.json, summary_<tag>.txt and contact sheets
ArtWork/audit_v100/sheets/<id>_<anim>_<kit>.png (rows = the 7 drawn directions, flagged cells boxed red with codes)."""
import colorsys, json, os, sys
from collections import defaultdict
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ND

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "Tools", "pipeline"))
import kit

OUT = os.path.join(ROOT, "ArtWork", "audit_v100"); SPR = os.path.join(ROOT, "GameData", "Sprites")
CELL = 96
DRAWN12 = ["south", "a030", "a060", "east", "a120", "a150", "north"]
MIRROR12 = {"a330": "a030", "a300": "a060", "west": "east", "a240": "a120", "a210": "a150"}
DIRS8 = ["south", "south-east", "east", "north-east", "north", "north-west", "west", "south-west"]
MIRROR8 = {"south-west": "south-east", "west": "east", "north-west": "north-east"}
# facing sign of the face's x offset: +1 = faces screen right, -1 = left, 0 = front/back
FACE = {"south": 0, "a030": 1, "a060": 1, "east": 1, "a120": 1, "a150": 1, "north": 0,
        "south-east": 1, "north-east": 1}
KEYS = set(kit.REGION_CODE.values())
EIGHT = np.ones((3, 3), bool)
LYING = ("fall", "gk_dive", "slide", "sit_ground", "getup", "header_dive", "celebrate_knee")

def hsv_arr(rgb):
    f = rgb.astype(np.float64) / 255
    r, g, b = f[..., 0], f[..., 1], f[..., 2]
    mx = f.max(-1); mn = f.min(-1); d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-9
    rr = m & (mx == r); gg = m & (mx == g) & ~rr; bb = m & ~rr & ~gg
    h[rr] = (60 * ((g - b)[rr] / d[rr])) % 360
    h[gg] = 60 * ((b - r)[gg] / d[gg]) + 120
    h[bb] = 60 * ((r - g)[bb] / d[bb]) + 240
    s = np.where(mx > 0, d / np.maximum(mx, 1e-9), 0)
    return h, s, mx

def classes(a):
    """per-pixel masks: op (opaque), key (kit key), skin, hair (dark warm), brown (mid warm), light, rawkit"""
    op = a[..., 3] > 0
    rgb = a[..., :3].astype(int)
    key = op & (a[..., 2] == 1) & np.isin(a[..., 0], list(KEYS))
    h, s, v = hsv_arr(a[..., :3])
    nk = op & ~key
    warm = (rgb[..., 0] >= rgb[..., 1]) & (rgb[..., 1] >= rgb[..., 2] - 4)
    skin = nk & warm & (v > 0.55) & (s > 0.15) & (s < 0.6) & (h < 40)
    hair = nk & (v <= 0.36) & ~((s > 0.6) & ((h < 12) | (h > 340)))
    brown = nk & warm & (v > 0.3) & (v <= 0.75) & (s > 0.2) & (h < 45) & ~((s > 0.6) & (h < 12))
    rawkit = nk & (((s > 0.6) & ((h >= 338) | (h <= 10)) & (v > 0.3))           # template sock red, unkeyed
                   | ((s > 0.35) & (h >= 280) & (h < 338) & (v > 0.25))          # template trim magenta
                   | ((s > 0.4) & (h >= 195) & (h <= 240) & (v > 0.25))          # template shirt blue
                   | ((s > 0.3) & (h >= 85) & (h <= 165) & (v > 0.2)))           # template shorts green
    return dict(op=op, key=key, skin=skin, hair=hair, brown=brown, rawkit=rawkit, h=h, s=s, v=v)

def head_box(op):
    """(top, bottom, left, right) of the head: the top rows of an upright figure down to the first wide row (shoulders)"""
    ys, xs = np.nonzero(op)
    if not len(ys): return None
    top, bot = ys.min(), ys.max()
    if bot - top < 40: return None                       # lying / crouched: no reliable head zone
    rows = [(y, np.nonzero(op[y])[0]) for y in range(top, top + 16)]
    w0 = None; hb = top + 10
    for y, x in rows:
        if not len(x): continue
        w = x.max() - x.min() + 1
        if w0 is None: w0 = w
        if y - top >= 6 and w >= 2.2 * max(5, min(w0, 9)): hb = y; break
    sub = op[top:hb]
    cols = np.nonzero(sub.any(0))[0]
    return top, hb, cols.min(), cols.max()

def cell_checks(a, keeper):
    C = classes(a); op = C["op"]; out = []
    if not op.any(): return [("empty", 1, None)], C
    lab, n = ND.label(op, EIGHT)
    if n > 1:
        sizes = ND.sum(op, lab, range(1, n + 1)); main = 1 + int(np.argmax(sizes))
        for i in range(1, n + 1):
            if i == main: continue
            ys, xs = np.nonzero(lab == i)
            out.append(("stray", int(len(ys)), (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))))
    # thin protrusions
    opened = ND.binary_opening(op, EIGHT)
    thin = op & ~opened
    tl, tn = ND.label(thin, EIGHT)
    hb = head_box(op)
    for i in range(1, tn + 1):
        m = tl == i; k = int(m.sum())
        if k < 4: continue
        ys, xs = np.nonzero(m)
        ext = max(xs.max() - xs.min(), ys.max() - ys.min()) + 1
        # thickness: pixels per unit length
        if ext < 4 or k / ext > 2.2: continue
        # sticks out: few contacts with the opened body
        touch = int((ND.binary_dilation(m, EIGHT) & opened).sum())
        if touch > max(3, k // 2): continue
        col = (C["hair"] | C["brown"] | C["skin"])[m].mean()
        code = "spur"
        if hb is not None and col >= 0.6 and ys.min() <= hb[1] + 2 and ys.max() <= hb[1] + 6: code = "spur_head"
        out.append((code, k, (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))))
    # baked ball (outfield)
    if not keeper:
        light = op & ~C["key"] & (C["s"] < 0.26) & (C["v"] > 0.6)
        ll, ln = ND.label(light, EIGHT)
        for i in range(1, ln + 1):
            ys, xs = np.nonzero(ll == i); w = np.ptp(xs) + 1; h = np.ptp(ys) + 1
            if len(ys) >= 8 and 4 <= w <= 13 and 4 <= h <= 13 and 0.6 <= w / h <= 1.6 and len(ys) >= 0.45 * w * h:
                out.append(("ball", int(len(ys)), (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))))
    # cell edge
    e = int(op[0].sum() + op[-1].sum() + op[:, 0].sum() + op[:, -1].sum())
    if e: out.append(("edge", e, None))
    rk = int(C["rawkit"].sum())
    if rk >= 3:
        ys, xs = np.nonzero(C["rawkit"])
        out.append(("rawkit", rk, (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))))
    if hb is not None:
        t, b, l, r = hb
        kh = int(C["key"][t:b - 1, l:r + 1].sum())
        if kh >= 4: out.append(("keyhead", kh, (int(l), int(t), int(r), int(b))))
    return out, C

def head_feats(C):
    hb = head_box(C["op"])
    if hb is None: return None
    t, b, l, r = hb
    sk = C["skin"][t:b, l:r + 1]; hr = C["hair"][t:b, l:r + 1] | C["brown"][t:b, l:r + 1]
    tot = int(sk.sum() + hr.sum())
    if tot < 6: return {"n": tot, "skin": 0.0, "dx": 0.0}
    xs_all = np.nonzero((sk | hr))[1]; cx = xs_all.mean()
    dx = (np.nonzero(sk)[1].mean() - cx) / max(1.0, (r - l + 1) / 2) if sk.sum() >= 3 else 0.0
    return {"n": tot, "skin": float(sk.sum()) / tot, "dx": float(dx)}

def load(bid):
    meta = json.load(open(os.path.join(SPR, bid + ".json")))
    A = np.array(Image.open(os.path.join(SPR, bid + ".png")).convert("RGBA"))
    return meta, A

def get(A, cr, flip=0):
    c, r = cr[:2]; a = A[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL].copy()
    return a[:, ::-1].copy() if flip else a

def group(meta):
    anims = meta.get("anims", {}); g = defaultdict(lambda: defaultdict(dict)); poses = defaultdict(dict)
    for k, v in meta["frames"].items():
        d, n = k.split("/")
        if "_f" in n and n.rsplit("_f", 1)[0] in anims and n.rsplit("_f", 1)[1].isdigit():
            an, i = n.rsplit("_f", 1); g[an][d][int(i)] = v
        else: poses[n][d] = v
    return g, poses

def audit(bid):
    meta, A = load(bid); keeper = meta.get("kind") == "keeper"
    g, poses = group(meta); issues = []
    cellres = {}
    for k, v in meta["frames"].items():
        cr = (v[0], v[1])
        if cr in cellres: continue
        a = get(A, cr)
        cellres[cr] = cell_checks(a, keeper)
    def add(kind, key, val, box=None, why=""):
        issues.append({"body": bid, "key": key, "code": kind, "val": val, "box": box, "why": why})
    # cell-level, reported against the unflipped (drawn) key
    owner = {}
    for k, v in meta["frames"].items():
        if v[2] == 0: owner.setdefault((v[0], v[1]), k)
    for cr, (res, _) in cellres.items():
        for code, val, box in res: add(code, owner.get(cr, f"cell{cr}"), val, box)
    # idle facing reference per direction
    ref = {}
    for d in DRAWN12 + DIRS8:
        v = meta["frames"].get(f"{d}/idle")
        if v: ref[d] = head_feats(cellres[(v[0], v[1])][1])
    # animation checks
    for an, dirs in g.items():
        lying = an.startswith(LYING)
        for d in DRAWN12:
            fr = dirs.get(d)
            if not fr:
                if len(dirs) > 1: add("missing_dir", f"{d}/{an}", 0)    # front-on-only takes (the dives) are south by design
                continue
            idx = sorted(fr)
            Cs = [cellres[(fr[i][0], fr[i][1])][1] for i in idx]
            area = np.array([C["op"].sum() for C in Cs], float)
            hts = np.array([np.ptp(np.nonzero(C["op"])[0]) + 1 if C["op"].any() else 0 for C in Cs], float)
            ma, mh = np.median(area), np.median(hts)
            for j, i in enumerate(idx):
                key = f"{d}/{an}_f{i:02d}"
                if not lying and (area[j] > 1.4 * ma or area[j] < 0.65 * ma): add("size", key, round(area[j] / ma, 2), why=f"area x{area[j] / ma:.2f} of clip median")
                if not lying and (hts[j] > mh + 9 or hts[j] < mh - 12): add("size", key, int(hts[j] - mh), why=f"height {hts[j] - mh:+.0f}px vs clip median")
            # pose jumps: IoU with previous frame
            ious = []
            for j in range(1, len(idx) + (1 if meta["anims"][an]["loop"] else 0)):
                p, q = Cs[j - 1]["op"], Cs[j % len(idx)]["op"]
                ious.append((p & q).sum() / max(1, (p | q).sum()))
            if ious:
                med = float(np.median(ious))
                for j, u in enumerate(ious):
                    if u < 0.55 * med and u < 0.35:
                        add("jump", f"{d}/{an}_f{idx[(j + 1) % len(idx)]:02d}", round(float(u), 2), why=f"overlap {u:.2f} with previous frame (clip median {med:.2f})")
            # head
            hf = [head_feats(C) for C in Cs]
            ns = [h["n"] for h in hf if h]
            if ns and not lying:
                mn = np.median(ns)
                for j, h in enumerate(hf):
                    if h and h["n"] < 0.45 * mn: add("head", f"{d}/{an}_f{idx[j]:02d}", h["n"], why=f"head pixels {h['n']} vs clip median {mn:.0f}")
            # facing vs the direction's idle
            r0 = ref.get(d)
            if r0 and not lying:
                for j, h in enumerate(hf):
                    if not h or h["n"] < 12: continue
                    if FACE[d] > 0 and h["dx"] < -0.35 and h["skin"] > 0.15:
                        add("facing", f"{d}/{an}_f{idx[j]:02d}", round(h["dx"], 2), why="face on the left side of the head in a right-facing direction")
                    if d == "north" and h["skin"] > 0.45 and r0["skin"] < 0.3:
                        add("facing", f"{d}/{an}_f{idx[j]:02d}", round(h["skin"], 2), why="face visible in the back view")
                    if d == "south" and h["skin"] < 0.12 and r0["skin"] > 0.3:
                        add("facing", f"{d}/{an}_f{idx[j]:02d}", round(h["skin"], 2), why="no face in the front view")
        for dst, src in MIRROR12.items():
            for i, v in dirs.get(src, {}).items():
                m = dirs.get(dst, {}).get(i)
                if m is None or m[:2] != v[:2] or m[2] != 1 - v[2]: add("mirror", f"{dst}/{an}_f{i:02d}", 0, why=f"not the flip of {src}")
    # older 8-direction poses: mirrors and facing
    for pn, dirs in poses.items():
        for dst, src in MIRROR8.items():
            if src in dirs and dst not in dirs and pn != "idle": add("mirror", f"{dst}/{pn}", 0, why="missing mirror")
        for d, v in dirs.items():
            if d not in FACE or v[2]: continue
            C = cellres[(v[0], v[1])][1]; h = head_feats(C)
            if h and h["n"] >= 12 and FACE[d] > 0 and h["dx"] < -0.35 and h["skin"] > 0.15 and not pn.startswith(LYING):
                add("facing", f"{d}/{pn}", round(h["dx"], 2), why="face on the left side of the head in a right-facing direction")
    return meta, A, g, poses, issues

KITS = None
def kits():
    global KITS
    if KITS is None:
        KITS = {k["slug"]: k for k in json.load(open(os.path.join(ROOT, "ArtSource", "Packs", "R08_93_Clubs_South_Idle_Only", "Club_Kit_Palettes.json")))}
    return KITS

def sheets(bid, meta, A, g, poses, issues, sc=2):
    keeper = meta.get("kind") == "keeper"; slot = "keeper1" if keeper else "home"
    flag = defaultdict(list)
    for it in issues: flag[it["key"]].append(it["code"])
    d0 = os.path.join(OUT, "sheets"); os.makedirs(d0, exist_ok=True)
    paths = []
    def cellimg(v, kk):
        a = get(A, v, v[2]); im = Image.fromarray(a, "RGBA")
        return kit.preview_kit(im, kk)
    for kname, club in (("liverpool", "liverpool"), ("newcastle", "newcastle-united")):
        kk = kits()[club][slot]
        for an in sorted(g):
            dirs = g[an]; n = max(len(f) for f in dirs.values())
            sh = Image.new("RGBA", (CELL * sc * n + 60, CELL * sc * len(DRAWN12)), (96, 140, 84, 255)); dr = ImageDraw.Draw(sh)
            for ri, d in enumerate(DRAWN12):
                dr.text((4, ri * CELL * sc + 4), d, fill=(255, 255, 255, 255))
                for i, v in sorted(dirs.get(d, {}).items()):
                    im = cellimg(v, kk).resize((CELL * sc, CELL * sc), Image.NEAREST)
                    x0, y0 = 60 + i * CELL * sc, ri * CELL * sc
                    sh.alpha_composite(im, (x0, y0))
                    dr.rectangle([x0, y0, x0 + CELL * sc - 1, y0 + CELL * sc - 1], outline=(70, 110, 60, 255))
                    dr.text((x0 + 3, y0 + 3), f"{i:02d}", fill=(230, 230, 230, 255))
                    fl = flag.get(f"{d}/{an}_f{i:02d}")
                    if fl:
                        dr.rectangle([x0, y0, x0 + CELL * sc - 1, y0 + CELL * sc - 1], outline=(255, 30, 30, 255), width=2)
                        dr.text((x0 + 3, y0 + CELL * sc - 14), ",".join(sorted(set(fl)))[:30], fill=(255, 255, 80, 255))
            p = os.path.join(d0, f"{bid}_{an}_{kname}.png"); sh.convert("RGB").save(p); paths.append(p)
        # the older 8-direction poses: one sheet, rows = 8 directions as drawn (mirrors flipped), cols = poses
        names = sorted(poses)
        for part in range(0, len(names), 16):
            nn = names[part:part + 16]
            sh = Image.new("RGBA", (CELL * sc * len(nn) + 70, CELL * sc * len(DIRS8) + 16), (96, 140, 84, 255)); dr = ImageDraw.Draw(sh)
            for ci, pn in enumerate(nn): dr.text((70 + ci * CELL * sc + 3, 2), pn[:24], fill=(255, 255, 255, 255))
            for ri, d in enumerate(DIRS8):
                dr.text((4, 16 + ri * CELL * sc + 4), d, fill=(255, 255, 255, 255))
                for ci, pn in enumerate(nn):
                    v = poses[pn].get(d)
                    if not v: continue
                    x0, y0 = 70 + ci * CELL * sc, 16 + ri * CELL * sc
                    sh.alpha_composite(cellimg(v, kk).resize((CELL * sc, CELL * sc), Image.NEAREST), (x0, y0))
                    fl = flag.get(f"{d}/{pn}")
                    if fl:
                        dr.rectangle([x0, y0, x0 + CELL * sc - 1, y0 + CELL * sc - 1], outline=(255, 30, 30, 255), width=2)
                        dr.text((x0 + 3, y0 + CELL * sc - 14), ",".join(sorted(set(fl)))[:30], fill=(255, 255, 80, 255))
            p = os.path.join(d0, f"{bid}_poses{part // 16}_{kname}.png"); sh.convert("RGB").save(p); paths.append(p)
    return paths

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    tag = sys.argv[sys.argv.index("--tag") + 1] if "--tag" in sys.argv else "now"
    if tag in args: args.remove(tag)
    ids = args or ["N01", "NK1"]
    os.makedirs(OUT, exist_ok=True); allis = []; lines = []
    for bid in ids:
        meta, A, g, poses, issues = audit(bid); allis += issues
        cnt = defaultdict(int)
        for it in issues: cnt[it["code"]] += 1
        lines.append(f"{bid}: {len(meta['frames'])} frame keys, {len({(v[0], v[1]) for v in meta['frames'].values()})} cells, issues {dict(sorted(cnt.items()))}")
        for it in sorted(issues, key=lambda t: (t["code"], t["key"])):
            lines.append(f"  {it['code']:<10} {it['key']:<34} {it['val']}  {it['box'] or ''} {it['why']}")
        if "--no-sheets" not in sys.argv: sheets(bid, meta, A, g, poses, issues)
    json.dump(allis, open(os.path.join(OUT, f"audit_{tag}.json"), "w"), indent=0)
    open(os.path.join(OUT, f"summary_{tag}.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(l for l in lines if not l.startswith("  ")))

if __name__ == "__main__":
    main()
