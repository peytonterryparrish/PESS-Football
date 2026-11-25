"""v0.7 per-club sprite atlases: ArtWork/raw_clubs/<slug>/<kit>_<player>/ -> GameData/Sprites/Clubs/<slug>_<kit>.png/.json

  python -I Tools/pipeline/build_club_atlas.py            # every club character that has finished all its poses
  python -I Tools/pipeline/build_club_atlas.py newcastle-united

Same QA as build_atlas.py (hard alpha, baked-in balls, size, doubled figures), then every pixel is snapped
to the club's OWN palette: the colours of the owner's south idle for that kit plus PixelLab's 8 rotations of it.
So stripes, trim and crest colours stay exactly the club's. The atlas is not kit-keyed: the game draws it as is.
Contact sheets: ArtWork/sheets_clubs/<slug>_<kit>.png (one row per direction).
v0.8 (owner: "pixel bleed, missing pixels and grey spots"): scrub() no longer eats grey/navy kits or white shirts and
sock bands, islands() drops stray balls and debris but keeps detached hands/feet, tidy() fills 1-2 px holes, forces hard
alpha and keeps a 2-texel empty border in every cell. Check with Tools/pipeline/audit_sprites.py."""
import json, os, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path[:0] = [os.path.join(ROOT, "Tools", "pipeline"), os.path.join(ROOT, "Tools", "pixellab")]
import build_atlas as BA
import poses_x

RAW = os.path.join(ROOT, "ArtWork", "raw_clubs")
OUT = os.path.join(ROOT, "GameData", "Sprites", "Clubs")
SHEETS = os.path.join(ROOT, "ArtWork", "sheets_clubs")
CELL, COLS = 96, 20
DIRS8 = BA.DIRS8
GEN = ["south", "south-east", "east", "north-east", "north"]
MIRROR = {"south-west": "south-east", "west": "east", "north-west": "north-east"}
# poses whose left/right meaning matters: a mirrored direction takes the OTHER side's frame, flipped
SWAP = {"gk_shuffleR": "gk_shuffleL", "gk_dive_low_R1": "gk_dive_low_L1", "gk_dive_high_R1": "gk_dive_high_L1", "gk_stretch_R": "gk_stretch_L"}
SWAP.update({v: k for k, v in list(SWAP.items())})

def hsv(c):
    import colorsys
    return colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255)

def _novel_frac(cols, idle_cols, t=30):
    """share of these colours farther than t from every colour of the owner's idle"""
    if not cols: return 0.0
    C = np.array(cols, dtype=np.float64)[:, None, :3]; Q = np.array(idle_cols, dtype=np.float64)[None, :, :]
    return float((((C - Q) ** 2).sum(-1).min(1) > t * t).mean())

def _big_parts(img, n=6):
    return sum(1 for c in BA.components(img) if len(c) >= n)

def scrub(img, idle_cols, keeper=False):
    """v0.7 clean-up PixelLab leaves behind (the owner's "players standing in their legs", ghost balls):
    1. mannequin ghosts: solid blue-grey limbs from the skeleton guide, a colour the player never wears;
    2. baked-in balls: a round white blob with dark patches, at the silhouette's edge (feet, hands, over the head).
    v0.8 (owner: "missing pixels"): both used to bite into the kit itself. Grey and navy kits lost whole shirts and socks
    as "ghosts", and white shirts / white sock bands were punched out as "balls", leaving floating heads, arms and
    boots. Now a ghost must be a colour the owner's idle does NOT use, and a ball must sit on the silhouette's edge and
    come away without cutting the figure in two. Detached islands are then judged by islands(). Returns (image, notes)."""
    from collections import deque
    img = img.copy(); px = img.load(); W, H = img.size; notes = []
    ghost = 0
    grey = set()
    for y in range(H):
        for x in range(W):
            c = px[x, y]
            if not c[3]: continue
            h, sat, v = hsv(c)
            if sat < 0.27 and 0.28 < v < 0.8 and c[2] >= c[0] + 6: grey.add((x, y))
    seen = set()
    for p0 in list(grey):
        if p0 in seen: continue
        comp = []; q = deque([p0]); seen.add(p0)
        while q:
            a0, b0 = q.popleft(); comp.append((a0, b0))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (a0 + dx, b0 + dy)
                    if n in grey and n not in seen: seen.add(n); q.append(n)
        # a solid grey limb (not the idle's scattered grey speckle) in a grey the player never wears (not his kit)
        if len(comp) >= 10 and _novel_frac([px[p] for p in comp], idle_cols) >= 0.6:
            for p in comp: px[p] = (0, 0, 0, 0)
            ghost += len(comp)
    if ghost >= 6: notes.append(f"ghost{ghost}")
    # balls
    white = {(x, y) for y in range(H) for x in range(W) if px[x, y][3] and hsv(px[x, y])[1] < 0.2 and hsv(px[x, y])[2] > 0.6}
    seen = set()
    for p0 in list(white):
        if p0 in seen: continue
        comp = []; q = deque([p0]); seen.add(p0)
        while q:
            a, b = q.popleft(); comp.append((a, b))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    n = (a + dx, b + dy)
                    if n in white and n not in seen: seen.add(n); q.append(n)
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        w, h = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
        if not (4 <= len(comp) <= 70 and 4 <= w <= 11 and 4 <= h <= 11 and 0.6 <= w / h <= 1.65): continue
        cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
        r = max(w, h) / 2.0 + 1.4
        ring = [(x, y) for y in range(max(0, int(cy - r)), min(H, int(cy + r) + 1)) for x in range(max(0, int(cx - r)), min(W, int(cx + r) + 1))
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r]
        dark = sum(1 for (x, y) in ring if px[x, y][3] and hsv(px[x, y])[2] < 0.32 and hsv(px[x, y])[1] < 0.4)
        # on the silhouette's edge: the band just outside the disc is mostly empty (a white shirt panel is not)
        band = [(x, y) for y in range(max(0, int(cy - r - 3)), min(H, int(cy + r + 4))) for x in range(max(0, int(cx - r - 3)), min(W, int(cx + r + 4)))
                if r * r < (x - cx) ** 2 + (y - cy) ** 2 <= (r + 2.5) ** 2]
        out = sum(1 for (x, y) in band if not px[x, y][3]) + (len(band) == 0)
        if dark < 2 or out < 0.4 * max(1, len(band)): continue
        cut = [(x, y) for (x, y) in ring if px[x, y][3] and (hsv(px[x, y])[1] < 0.3 or (x, y) in white)]
        before = _big_parts(img)
        old = [px[p] for p in cut]
        for p in cut: px[p] = (0, 0, 0, 0)
        if _big_parts(img) > before:              # it cut a leg or an arm in two: a white sock band or cuff, not a ball
            for p, c in zip(cut, old): px[p] = c
            continue
        notes.append("ball_removed")
    img, n2 = islands(img, idle_cols, keeper); notes += n2
    return img, notes

def islands(img, idle_cols, keeper=False):
    """v0.8: opaque islands apart from the main body. Kept: detached hands, feet, gloves and boots (body-coloured, near
    the body). Dropped: specks, stray balls, grey debris in colours the player never wears, small far-off fragments."""
    px = img.load(); notes = []
    comps = sorted(BA.components(img), key=len, reverse=True)
    if len(comps) < 2: return img, notes
    main = np.array(comps[0])
    for c in comps[1:]:
        a = np.array(c); n = len(c)
        dist = int(np.maximum(np.abs(main[:, None, 0] - a[None, :, 0]), np.abs(main[:, None, 1] - a[None, :, 1])).min())
        cols = [px[p] for p in c]
        hs = [hsv(q) for q in cols]
        white = sum(1 for q in hs if q[1] < 0.2 and q[2] > 0.6) / n
        grey = sum(1 for q in hs if q[1] < 0.25) / n
        w = a[:, 0].max() - a[:, 0].min() + 1; h = a[:, 1].max() - a[:, 1].min() + 1
        ball = n >= 8 and w <= 14 and h <= 14 and 0.55 <= w / h <= 1.8 and n >= 0.6 * w * h and (white >= 0.12 or grey >= 0.5)
        why = None
        if n < 6: why = "speck"
        elif ball and dist >= (4 if keeper else 2): why = "ball"        # a keeper's glove close to his body stays
        elif grey >= 0.5 and _novel_frac(cols, idle_cols) >= 0.5: why = "debris"
        elif n < 25 and dist >= (4 if keeper else 3) and sum(1 for q in hs if q[1] < 0.25 and q[2] > 0.35) / n >= 0.6: why = "debris"   # grey/white crumbs
        elif dist > 10 and n < 0.08 * len(main): why = "far"
        if why:
            for p in c: px[p] = (0, 0, 0, 0)
            if n >= 6: notes.append(f"island_{why}{n}")
    return img, notes

MARGIN = 2      # transparent texels kept round every cell: M_PFSprite's 3x3 supersample reaches ~1/3 screen px past the tile

def tidy(img, fill=True):
    """v0.8, after the palette snap: 1-2 px holes enclosed by the body filled with the colour round them (the idle's
    grain is untouched: the fill is a colour already next to it), hard alpha with no colour under transparent texels,
    and a MARGIN-texel empty border (the figure is nudged up to MARGIN px if it fits, else the overhang is cut).
    Returns (image, notes)."""
    from collections import Counter, deque
    a = np.array(img.convert("RGBA")); notes = []
    m = a[..., 3] >= 128
    a[..., 3] = np.where(m, 255, 0); a[~m] = 0
    H, W = m.shape
    if fill and m.any():
        out = np.zeros((H + 2, W + 2), bool); out[0, :] = out[-1, :] = out[:, 0] = out[:, -1] = True
        reach = out.copy(); t = np.pad(~m, 1, constant_values=True)
        q = deque(zip(*np.nonzero(out)))
        while q:
            y, x = q.popleft()
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                yy, xx = y + dy, x + dx
                if 0 <= yy < H + 2 and 0 <= xx < W + 2 and t[yy, xx] and not reach[yy, xx]: reach[yy, xx] = True; q.append((yy, xx))
        enc = (~m) & ~reach[1:-1, 1:-1]
        seen = np.zeros_like(enc); filled = 0
        for y0, x0 in zip(*np.nonzero(enc)):
            if seen[y0, x0]: continue
            comp = [(y0, x0)]; seen[y0, x0] = True; k = 0
            while k < len(comp):
                y, x = comp[k]; k += 1
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    yy, xx = y + dy, x + dx
                    if enc[yy, xx] and not seen[yy, xx]: seen[yy, xx] = True; comp.append((yy, xx))
            if len(comp) > 2: continue                 # a real gap (between arm and body, between the legs)
            for y, x in comp:
                nb = Counter(tuple(a[yy, xx, :3]) for yy in (y - 1, y, y + 1) for xx in (x - 1, x, x + 1) if m[yy, xx])
                a[y, x, :3] = nb.most_common(1)[0][0]; a[y, x, 3] = 255; filled += 1
        m = a[..., 3] > 0
        if filled: notes.append(f"filled{filled}")
    if m.any():
        ys, xs = np.nonzero(m)
        def nudge(lo, hi, n):
            need_lo, need_hi = max(0, MARGIN - lo), max(0, hi - (n - 1 - MARGIN))
            if need_lo and not need_hi and hi + need_lo <= n - 1 - MARGIN: return need_lo
            if need_hi and not need_lo and lo - need_hi >= MARGIN: return -need_hi
            return 0
        dx, dy = nudge(xs.min(), xs.max(), W), nudge(ys.min(), ys.max(), H)
        if dx or dy:
            b = np.zeros_like(a)
            b[max(0, dy):H + min(0, dy), max(0, dx):W + min(0, dx)] = a[max(0, -dy):H - max(0, dy), max(0, -dx):W - max(0, dx)]
            a = b; notes.append(f"nudge{dx:+d},{dy:+d}")
        band = np.zeros((H, W), bool); band[:MARGIN] = band[-MARGIN:] = True; band[:, :MARGIN] = band[:, -MARGIN:] = True
        cut = int((band & (a[..., 3] > 0)).sum())
        if cut: a[band] = 0; notes.append(f"cut{cut}")
    return Image.fromarray(a, "RGBA"), notes

import numpy as np
_NOISE = None
def _noise():
    global _NOISE
    if _NOISE is None:
        y, x = np.mgrid[0:CELL, 0:CELL].astype(np.uint64)
        h = ((x * np.uint64(73856093)) ^ (y * np.uint64(19349663))) * np.uint64(2654435761)
        h = (h >> np.uint64(13)) & np.uint64(1023)
        _NOISE = (h.astype(np.float64) / 1023.0) * 2.0 - 1.0        # fixed in sprite space: the grain never crawls
    return _NOISE

def grain(arr, m):
    """mean luminance step between horizontal neighbours, inside the body only (outlines excluded)"""
    L = arr[..., 0] * 0.3 + arr[..., 1] * 0.59 + arr[..., 2] * 0.11
    inner = m.copy()
    for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0), (0, 2)):
        inner &= np.roll(np.roll(m, dy, 0), dx, 1)
    d = np.abs(L - np.roll(L, -1, 1))[inner]
    return float(d.mean()) if d.size else 0.0

def _neutral_light(a):
    """(mask, luminance) of the light neutral pixels (whites and light greys of the kit, socks, boots' trim):
    saturation < 0.15 (tinted greys, e.g. a purple-grey keeper kit, stay out), above an Otsu split of the neutral luminances (so black shorts and boots stay out)"""
    rgb = a[..., :3].astype(np.float64); m = a[..., 3] > 0
    mx = rgb.max(-1); mn = rgb.min(-1); sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0)
    L = rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11
    neu = m & (sat < 0.15)
    v = L[neu]
    if v.size < 20: return None, L
    h, e = np.histogram(v, 64, (0, 256)); c = (e[:-1] + e[1:]) / 2
    w0 = np.cumsum(h); w1 = w0[-1] - w0; m0 = np.cumsum(h * c)
    mu0 = m0 / np.maximum(w0, 1); mu1 = (m0[-1] - m0) / np.maximum(w1, 1)
    t = c[int(np.argmax(w0 * w1 * (mu0 - mu1) ** 2))]
    t = max(t, 70.0)
    return neu & (L > t), L

def idle_tone(src):
    """the owner's south idle: sorted luminances of its light neutral pixels and their share of the figure"""
    a = np.asarray(src.convert("RGBA"))
    k, L = _neutral_light(a)
    return None if k is None or k.sum() < 20 else (np.sort(L[k]), k.sum() / max(1, (a[..., 3] > 0).sum()))

def tone_match(img, ref):
    """v0.8 (owner: "grey spots"): PixelLab shades a pose's whites darker than the hand-painted idle, so the palette
    snap turned white shirts, shorts and socks into the idle's GREY dither. The frame's light-neutral luminances are
    quantile-matched to the south idle's (monotonic, so the order of the dither and shading survives) before the
    snap; chroma is kept (each pixel scaled). Returns (image, mean luminance shift)."""
    if ref is None: return img, 0.0
    ref, share = ref
    a = np.asarray(img.convert("RGBA")).astype(np.float64)
    k, L = _neutral_light(a.astype(np.uint8))
    if k is None or k.sum() < 12: return img, 0.0
    fs = k.sum() / max(1, (a[..., 3] > 0).sum())
    if not 0.4 <= fs / share <= 2.5: return img, 0.0      # not the same white areas as the idle's: leave it alone
    v = L[k]; order = np.argsort(np.argsort(v)); q = (order + 0.5) / v.size
    tgt = np.interp(q, (np.arange(ref.size) + 0.5) / ref.size, ref)
    tgt = np.clip(tgt, v, v + 48)                        # only lift whites PixelLab shaded too dark, at most 48 levels
    f = np.ones_like(L); f[k] = tgt / np.maximum(v, 1.0)
    a[..., :3] = np.clip(a[..., :3] * f[..., None], 0, 255)
    return Image.fromarray(a.astype(np.uint8), "RGBA"), float((tgt - v).mean())

def snap_idle(img, pal, target=None):
    """Every pixel onto the owner's south-idle palette, with the idle's TEXTURE (owner, v0.7: directions must keep the
    south idle's dither, texture and shading). PixelLab's turned and posed frames come back smoother than the
    hand-dithered idles, so a fixed per-pixel grain is added before the snap, scaled until the frame's inner texture
    measures the same as the idle's (`target`)."""
    a = np.asarray(img.convert("RGBA")).astype(np.float64)
    m = a[..., 3] > 0
    P = np.array(pal, dtype=np.float64)
    N = _noise()[:a.shape[0], :a.shape[1]]
    def snap(amp):
        rgb = a[..., :3] + N[..., None] * amp
        d = ((rgb[:, :, None, :] - P[None, None, :, :]) ** 2).sum(-1)
        return P[d.argmin(-1)], d.min(-1)
    best = None
    for amp in ([0.0] if target is None else [0.0, 8.0, 14.0, 20.0, 28.0, 36.0]):
        out, dm = snap(amp)
        score = abs(grain(out, m) - target) if target is not None else 0
        if best is None or score < best[0] - 0.3: best = (score, out, amp)
    res = np.zeros_like(a); res[..., :3] = best[1]; res[..., 3] = np.where(m, 255, 0)
    res[~m] = 0
    md = float(np.sqrt(snap(0.0)[1][m].max())) if m.any() else 0.0
    return Image.fromarray(res.astype(np.uint8), "RGBA"), {"maxdist": md, "amp": best[2]}

PACK2 = os.path.join(ROOT, "ArtSource", "Packs", "R08_93_Clubs_Both_Kits_Complete")
PACK_DIVES = {"gk_fly_high_L": "GK026", "gk_fly_high_R": "GK027", "gk_fly_low_L": "GK028", "gk_fly_low_R": "GK029",
              "gk_fly_take": "GK030", "gk_smother": "GK031"}
_EXP = None
def pack_dives(slug, player, kit):
    global _EXP
    if not os.path.isdir(PACK2): return []
    if _EXP is None:
        sys.path.insert(0, PACK2)
        import export_poses as E
        _EXP = E
    out = []
    for name, aid in PACK_DIVES.items():
        try: im = _EXP.frame(slug, player, aid, None, kit).convert("RGBA")
        except Exception as e:
            print("  pack frame", slug, player, aid, kit, "unavailable:", e); continue
        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.paste(im, ((CELL - im.width) // 2, 88 - 82), im)      # pack pivot (48,82) onto ours (48,88)
        out.append((name, cell))
    return out

def complete(d, kind):
    names = [n for b in poses_x.SETS_X[kind] for n in b]
    return all(os.path.exists(os.path.join(d, x, n + ".png")) for x in GEN for n in names)

def build(slug, cdir):
    kit, player = cdir.split("_", 1)
    kind = "keeper" if kit.startswith("keeper") else "outfield"
    d = os.path.join(RAW, slug, cdir)
    rots = {x: Image.open(os.path.join(d, f"rot_{x}.png")).convert("RGBA") for x in DIRS8}
    src = Image.open(os.path.join(d, "ref96.png")).convert("RGBA")
    # owner (v0.7): every direction must keep the south idle's dither, texture and shading, so the palette is the
    # owner's idle ONLY (PixelLab's rotations drift toward smooth gradients and new shades)
    pal = list({c[:3] for c in src.get_flattened_data() if c[3] >= 128})
    idle_cols = pal
    sa = np.asarray(src).astype(np.float64); tgt = grain(sa, sa[..., 3] > 0)   # the idle's own texture
    tone = idle_tone(src)                                                       # the idle's whites (v0.8 tone match)
    names = [n for b in poses_x.SETS_X[kind] for n in b]
    cells, qa = [], {"rejected": {}, "notes": {}}
    for x in DIRS8:
        im, _ = BA.clean(rots[x], rots[x].getbbox())
        if x != "south":
            im, _ = islands(im, idle_cols, kind == "keeper")   # PixelLab turns can carry a ball over the head
            im, _ = tone_match(im, tone)
        im, _ = snap_idle(im, pal, tgt)
        im, n3 = tidy(im, fill=(x != "south"))             # the owner's south idle is never retouched
        if n3: qa["notes"][f"{x}/idle"] = n3
        cells.append((f"{x}/idle", im))
    for x in GEN:
        idle_n = sum(1 for c in rots[x].get_flattened_data() if c[3] >= 128)
        for n in names:
            p = os.path.join(d, x, n + ".png")
            if not os.path.exists(p): continue
            im, notes = BA.clean(Image.open(p), rots[x].getbbox(), specks=False)
            im, n2 = scrub(im, idle_cols, kind == "keeper"); notes += n2
            bb = im.getbbox(); n_op = sum(1 for c in im.get_flattened_data() if c[3])
            bad = None
            if not bb: bad = "empty"
            elif n_op < 0.45 * idle_n: bad = f"too_small {n_op}/{idle_n}"
            elif n_op > 2.8 * idle_n: bad = f"too_big {n_op}/{idle_n}"
            if "double_figure" in notes: bad = "double_figure"
            if bad: qa["rejected"][f"{x}/{n}"] = bad; continue
            im, dl = tone_match(im, tone)
            if abs(dl) >= 8: notes.append(f"tone{dl:+.0f}")
            im, st = snap_idle(im, pal, tgt)
            im, n3 = tidy(im); notes += n3
            if st["maxdist"] > 90: notes.append(f"colour_drift{int(st['maxdist'])}")
            if notes: qa["notes"][f"{x}/{n}"] = notes
            cells.append((f"{x}/{n}", im))
    if kind == "keeper":
        # owner's R08 action pack (v0.7 audit): only the flat dives and the smother passed (the outfield actions and the
        # standing keeper poses have scrambled legs). They are texel-exact remaps of this player's own idle, so the art
        # matches by construction; they're drawn for the in-flight part of every dive, in every facing (ISS-style).
        for name, img in pack_dives(slug, player, kit):
            img, n3 = tidy(img, fill=False)                # the owner's own art: margin and hard alpha only
            if n3: qa["notes"][f"south/{name}"] = n3
            cells.append((f"south/{name}", img))
    rows = (len(cells) + COLS - 1) // COLS
    atlas = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
    frames = {}
    for i, (key, im) in enumerate(cells):
        c, r = i % COLS, i // COLS
        atlas.paste(im, (c * CELL, r * CELL)); frames[key] = [c, r, 0]
    for dst, srcd in MIRROR.items():
        for n in names:
            s = f"{srcd}/{SWAP.get(n, n)}"
            if s in frames: frames[f"{dst}/{n}"] = frames[s][:2] + [1]
    os.makedirs(OUT, exist_ok=True)
    cid = f"{slug}_{kit}"
    atlas.save(os.path.join(OUT, cid + ".png"), optimize=True)
    json.dump({"id": cid, "kind": kind, "cell": CELL, "pivot": [48, 88], "size": list(atlas.size), "kit_keyed": False,
               "atlas": f"GameData/Sprites/Clubs/{cid}.png", "player": player, "frames": frames},
              open(os.path.join(OUT, cid + ".json"), "w"))
    os.makedirs(SHEETS, exist_ok=True)
    qa["frames"] = len(cells); qa["palette"] = len(pal)
    json.dump(qa, open(os.path.join(SHEETS, cid + "_qa.json"), "w"), indent=1)
    # contact sheet: one row per generated direction, every pose, on grass
    order = ["idle"] + names
    sheet = Image.new("RGBA", (len(order) * CELL, len(GEN) * CELL), (44, 112, 52, 255))
    for r, x in enumerate(GEN):
        for i, n in enumerate(order):
            k = f"{x}/{n}"
            if k in frames:
                c, rr, _ = frames[k]
                im = atlas.crop((c * CELL, rr * CELL, c * CELL + CELL, rr * CELL + CELL))
                sheet.paste(im, (i * CELL, r * CELL), im)
    sheet.convert("RGB").save(os.path.join(SHEETS, cid + ".png"))
    print(cid, "frames", len(cells), "rejected", len(qa["rejected"]), "palette", len(pal))

def build_club(s):
    out = []
    for cdir in sorted(os.listdir(os.path.join(RAW, s))):
        kit = cdir.split("_")[0]
        kind = "keeper" if kit.startswith("keeper") else "outfield"
        if not complete(os.path.join(RAW, s, cdir), kind): out.append(f"{s} {cdir} incomplete"); continue
        try: build(s, cdir); out.append(f"{s} {cdir} ok")
        except Exception as e: out.append(f"{s} {cdir} ERROR {e}")
    return out

if __name__ == "__main__":
    # python -I build_club_atlas.py [--jobs N] [slugs...]   (clubs in parallel processes)
    args = sys.argv[1:]; jobs = 1
    if args[:1] == ["--jobs"]: jobs = int(args[1]); args = args[2:]
    slugs = args or sorted(os.listdir(RAW))
    if jobs > 1:
        from multiprocessing import Pool
        with Pool(jobs) as pool:
            for lines in pool.imap_unordered(build_club, slugs):
                for l in lines: print(l, flush=True)
    else:
        for s in slugs:
            for l in build_club(s): print(l, flush=True)
