"""v0.8 sprite audit: every cell of every per-club atlas (GameData/Sprites/Clubs/<slug>_<kit>.png/.json), checked for the
defects the owner reported ("pixel bleed, missing pixels and grey spots").

  python -I Tools/pipeline/audit_sprites.py [--tag before|after] [--dir GameData/Sprites/Clubs] [--jobs N] [sets...]

Per cell (each physical atlas cell once; mirrored frames reuse a cell):
  a  islands     8-connected opaque components apart from the main body: kind ball (round, light/white with dark
                 patches), debris (small or grey), limb (body-coloured and close to the body: usually a legit hand/foot)
  b  grey        clusters of mid-grey pixels (saturation < 0.16, value 0.25-0.78; white and black excluded) at least
                 twice the size of the largest such cluster in the owner's south idle (and >= 8 px): the idle's grey is a
                 scattered dither speckle, so a solid grey patch is a ghost limb, ball debris or a mis-snapped shade
  c  alpha/halo  alpha other than 0/255; colour hidden under alpha 0; 1-px fringe = silhouette-edge pixels in a light
                 grey the idles never put on their outline
  d  holes       transparent pixels enclosed by the body (pinholes <= 2 px = missing pixels; larger = gaps, reported only)
                 and notches (a transparent pixel with 3-4 opaque 4-neighbours: a 1-px gap in the outline)
  e  edge        content in the cell's outermost row/column (cropped, or drawn into the neighbour cell)
  f  uv_bleed    content within 2 px of the border: M_PFSprite supersamples up to 1/3 screen px past the tile, i.e.
                 up to 0.67 texel at half-size players, so a neighbour's outer column can be sampled
Writes ArtWork/audit_v08/audit_<tag>.json, summary_<tag>.txt and worst_<defect>_<tag>.png contact sheets."""
import colorsys, json, os, sys
from collections import deque
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTDIR = os.path.join(ROOT, "ArtWork", "audit_v08")
CELL = 96
P = CELL + 2                     # padded width for flat-index neighbour walks
N8 = (-P - 1, -P, -P + 1, -1, 1, P - 1, P, P + 1)
N4 = (-P, -1, 1, P)

def comps(mask, nb=N8):
    """connected components of a (CELL, CELL) bool mask -> list of arrays of (y, x)"""
    m = np.zeros((P, P), bool); m[1:-1, 1:-1] = mask
    f = m.ravel(); seen = np.zeros_like(f); out = []
    for i0 in np.flatnonzero(f):
        if seen[i0]: continue
        seen[i0] = True; q = [i0]; k = 0
        while k < len(q):
            i = q[k]; k += 1
            for d in nb:
                j = i + d
                if f[j] and not seen[j]: seen[j] = True; q.append(j)
        q = np.array(q); out.append(np.stack([q // P - 1, q % P - 1], 1))
    return out

def hsv_arr(rgb):
    r, g, b = [rgb[..., i].astype(np.float64) / 255 for i in range(3)]
    mx = np.maximum(np.maximum(r, g), b); mn = np.minimum(np.minimum(r, g), b)
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0)
    return s, mx

def lowsat(rgb):
    s, v = hsv_arr(rgb)
    return (s < 0.16) & (v > 0.25) & (v < 0.78)

def enclosed(alpha_mask):
    """transparent pixels not 4-connected to the cell border"""
    t = ~alpha_mask
    out = np.zeros((P, P), bool); out[1:-1, 1:-1] = t
    out[0, :] = out[-1, :] = True; out[:, 0] = out[:, -1] = True
    f = out.ravel(); seen = np.zeros_like(f); q = deque([0]); seen[0] = True
    while q:
        i = q.popleft()
        for d in N4:
            j = i + d
            if 0 <= j < f.size and f[j] and not seen[j]: seen[j] = True; q.append(j)
    reach = seen.reshape(P, P)[1:-1, 1:-1]
    return t & ~reach

def edge_px(m):
    """opaque pixels with a transparent (or outside) 4-neighbour"""
    p = np.pad(m, 1)
    inner = p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
    return m & ~inner

def idle_profile(cell_rgba):
    """what the owner's south idle uses: colour shares, its largest desaturated cluster"""
    m = cell_rgba[..., 3] > 0
    cols = cell_rgba[..., :3][m]
    keys = (cols[:, 0].astype(np.int64) << 16) | (cols[:, 1].astype(np.int64) << 8) | cols[:, 2]
    u, n = np.unique(keys, return_counts=True)
    share = dict(zip(u.tolist(), (n / max(1, m.sum())).tolist()))
    g = lowsat(cell_rgba[..., :3]) & m
    gc = comps(g)
    return {"share": share, "grey_max": max([len(c) for c in gc], default=0)}

def ckey(rgb):
    return (rgb[..., 0].astype(np.int64) << 16) | (rgb[..., 1].astype(np.int64) << 8) | rgb[..., 2]

def classify_island(c, cell, main_mask):
    ys, xs = c[:, 0], c[:, 1]
    rgb = cell[ys, xs, :3]
    s, v = hsv_arr(rgb)
    h, w = ys.max() - ys.min() + 1, xs.max() - xs.min() + 1
    fill = len(c) / (h * w)
    white = float(((s < 0.2) & (v > 0.68)).mean()); grey = float(((s < 0.22)).mean())
    dark = float(((v < 0.32)).mean())
    # Chebyshev distance to the main body
    my, mx = np.nonzero(main_mask)
    dist = int(min(np.maximum(np.abs(my[:, None] - ys[None, :]), np.abs(mx[:, None] - xs[None, :])).min(), 99)) if len(my) else 99
    if len(c) >= 8 and 0.55 <= w / h <= 1.8 and fill >= 0.45 and white >= 0.2 and h <= 14 and w <= 14: kind = "ball"
    elif len(c) < 6 or grey >= 0.6: kind = "debris"
    elif dist <= 3: kind = "limb"
    else: kind = "debris"
    return {"n": int(len(c)), "bbox": [int(xs.min()), int(ys.min()), int(w), int(h)], "dist": dist, "kind": kind,
            "white": round(white, 2), "grey": round(grey, 2), "dark": round(dark, 2)}

def audit_cell(cell, prof, idle_edge_cols):
    a = cell[..., 3]; m = a > 0; rgb = cell[..., :3]
    r = {}
    soft = int(((a > 0) & (a < 255)).sum()); hidden = int(((a == 0) & (rgb.sum(-1) > 0)).sum())
    if not m.any(): return {"empty": True}
    cs = sorted(comps(m), key=len, reverse=True)
    main = np.zeros_like(m); main[cs[0][:, 0], cs[0][:, 1]] = True
    isl = [classify_island(c, cell, main) for c in cs[1:]]
    # grey blobs: desaturated clusters made of colours the idle barely uses, or bigger than any in the idle
    g = lowsat(rgb) & m
    share = prof["share"]
    keys = ckey(rgb)
    grey = []
    for c in comps(g):
        if len(c) < 6: continue
        ks = keys[c[:, 0], c[:, 1]]
        rare = float(np.mean([share.get(k, 0.0) < 0.01 for k in ks.tolist()]))
        if len(c) >= max(8, 2 * prof["grey_max"]):
            ys, xs = c[:, 0], c[:, 1]
            grey.append({"n": int(len(c)), "bbox": [int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1)], "rare": round(rare, 2)})
    # halo: light-grey silhouette pixels the idles never use on their outline
    e = edge_px(m)
    s, v = hsv_arr(rgb)
    lg = e & (s < 0.2) & (v > 0.6)
    ek = keys[lg]
    fringe = int(sum(1 for k in ek.tolist() if k not in idle_edge_cols))
    # holes
    enc = enclosed(m)
    holes = comps(enc, N4)
    pin = int(sum(len(h) for h in holes if len(h) <= 2))
    gaps = int(sum(1 for h in holes if len(h) > 2))
    t = ~m; pm = np.pad(m, 1)
    nopq = pm[:-2, 1:-1].astype(int) + pm[2:, 1:-1] + pm[1:-1, :-2] + pm[1:-1, 2:]
    notch = int((t & ~enc & (nopq >= 3)).sum())
    ys, xs = np.nonzero(m)
    margin = int(min(ys.min(), xs.min(), CELL - 1 - ys.max(), CELL - 1 - xs.max()))
    r = {"soft_alpha": soft, "hidden_rgb": hidden, "islands": isl, "grey": grey, "fringe": fringe, "pinholes": pin,
         "gaps": gaps, "notches": notch, "margin": margin, "touch_edge": margin == 0, "uv_bleed": margin < 2,
         "opaque": int(m.sum())}
    return r

def audit_set(args):
    png, js = args
    J = json.load(open(js))
    A = np.asarray(Image.open(png).convert("RGBA"))
    raw = Image.open(png)
    if raw.mode != "RGBA":
        pass
    cells = {}
    for k, (c, rr, fl) in J["frames"].items():
        if fl: continue
        cells.setdefault((c, rr), k)
    def cell(c, rr): return A[rr * CELL:(rr + 1) * CELL, c * CELL:(c + 1) * CELL]
    sc, sr, _ = J["frames"]["south/idle"]
    prof = idle_profile(cell(sc, sr))
    idle_edge = set()
    for k, (c, rr, fl) in J["frames"].items():
        if k.endswith("/idle") and not fl:
            ce = cell(c, rr); m = ce[..., 3] > 0
            idle_edge.update(ckey(ce[..., :3])[edge_px(m)].tolist())
    out = {}
    for (c, rr), k in sorted(cells.items(), key=lambda t: (t[0][1], t[0][0])):
        res = audit_cell(cell(c, rr), prof, idle_edge)
        res["cell"] = [c, rr]
        out[k] = res
    # stale content in cells no frame uses
    used = set(cells)
    stale = 0
    for rr in range(A.shape[0] // CELL):
        for c in range(A.shape[1] // CELL):
            if (c, rr) not in used and cell(c, rr)[..., 3].any(): stale += 1
    return J["id"], {"frames": out, "stale_cells": stale}

KINDS = ["island_ball", "island_debris", "island_limb", "grey_blob", "soft_alpha", "hidden_rgb", "fringe", "pinhole",
         "hole_gap", "notch", "touch_edge", "uv_bleed"]

def cell_counts(r):
    if r.get("empty"): return {}
    isl = r["islands"]
    return {"island_ball": sum(1 for i in isl if i["kind"] == "ball"),
            "island_debris": sum(1 for i in isl if i["kind"] == "debris"),
            "island_limb": sum(1 for i in isl if i["kind"] == "limb"),
            "grey_blob": len(r["grey"]), "soft_alpha": int(r["soft_alpha"] > 0), "hidden_rgb": int(r["hidden_rgb"] > 0),
            "fringe": int(r["fringe"] >= 3), "pinhole": r["pinholes"], "hole_gap": r["gaps"], "notch": r["notches"],
            "touch_edge": int(r["touch_edge"]), "uv_bleed": int(r["uv_bleed"])}

WEIGHT = {"island_ball": 12, "island_debris": 6, "island_limb": 1, "grey_blob": 5, "soft_alpha": 3, "hidden_rgb": 1,
          "fringe": 2, "pinhole": 2, "hole_gap": 1, "notch": 0.2, "touch_edge": 8, "uv_bleed": 3}

def score(cc): return sum(WEIGHT[k] * v for k, v in cc.items())

def sheet(items, data, atlas_dir, path, title, n=40):
    """zoomed (x3) cells, each shown plain on grass and with the flagged pixels marked in magenta"""
    Z = 3; T = CELL * Z
    items = items[:n]
    if not items: return
    cols = 5
    rows = (len(items) + cols - 1) // cols
    sh = Image.new("RGB", (cols * (2 * T + 8), rows * (T + 16) + 18), (30, 30, 34))
    d = ImageDraw.Draw(sh); d.text((4, 3), title, fill=(240, 240, 240))
    cache = {}
    for i, (sid, key, sc) in enumerate(items):
        if sid not in cache: cache[sid] = Image.open(os.path.join(atlas_dir, sid + ".png")).convert("RGBA")
        r = data[sid]["frames"][key]; c, rr = r["cell"]
        im = cache[sid].crop((c * CELL, rr * CELL, (c + 1) * CELL, (rr + 1) * CELL))
        bg = Image.new("RGBA", im.size, (52, 120, 58, 255)); bg.alpha_composite(im)
        mk = bg.copy(); px = mk.load()
        a = np.asarray(im)
        m = a[..., 3] > 0
        cs = sorted(comps(m), key=len, reverse=True)
        for cc in cs[1:]:
            for y, x in cc: px[int(x), int(y)] = (255, 0, 255, 255)
        for g in r.get("grey", []):
            x0, y0, w, h = g["bbox"]
            ImageDraw.Draw(mk).rectangle((x0 - 1, y0 - 1, x0 + w, y0 + h), outline=(0, 255, 255, 255))
        for y, x in np.argwhere(enclosed(m)): px[int(x), int(y)] = (255, 255, 0, 255)
        for x in (0, CELL - 1):
            for y in range(CELL):
                if m[y, x]: px[x, y] = (255, 0, 0, 255)
        for y in (0, CELL - 1):
            for x in range(CELL):
                if m[y, x]: px[x, y] = (255, 0, 0, 255)
        X = (i % cols) * (2 * T + 8); Y = 18 + (i // cols) * (T + 16)
        sh.paste(bg.resize((T, T), Image.NEAREST).convert("RGB"), (X, Y))
        sh.paste(mk.resize((T, T), Image.NEAREST).convert("RGB"), (X + T, Y))
        d.text((X + 2, Y + T + 2), f"{sid} {key} ({sc:g})", fill=(230, 230, 230))
    sh.save(path)

def main():
    args = sys.argv[1:]; tag = "before"; adir = os.path.join(ROOT, "GameData", "Sprites", "Clubs"); jobs = os.cpu_count() or 4
    while args and args[0].startswith("--"):
        if args[0] == "--tag": tag = args[1]
        elif args[0] == "--dir": adir = os.path.abspath(args[1])
        elif args[0] == "--jobs": jobs = int(args[1])
        args = args[2:]
    ids = args or sorted(f[:-5] for f in os.listdir(adir) if f.endswith(".json") and os.path.exists(os.path.join(adir, f[:-5] + ".png")))
    work = [(os.path.join(adir, i + ".png"), os.path.join(adir, i + ".json")) for i in ids]
    data = {}
    if jobs > 1:
        from multiprocessing import Pool
        with Pool(jobs) as pool:
            for sid, res in pool.imap_unordered(audit_set, work): data[sid] = res
    else:
        for w in work:
            sid, res = audit_set(w); data[sid] = res
    os.makedirs(OUTDIR, exist_ok=True)
    json.dump(data, open(os.path.join(OUTDIR, f"audit_{tag}.json"), "w"), indent=0)
    tot = {k: 0 for k in KINDS}; cells_hit = {k: 0 for k in KINDS}; per_set = []; ncell = 0; cell_rank = []
    idle_hit = {k: 0 for k in KINDS}
    for sid, res in data.items():
        ss = 0; sc_cnt = {k: 0 for k in KINDS}
        for key, r in res["frames"].items():
            cc = cell_counts(r); ncell += 1
            for k, v in cc.items():
                tot[k] += v; cells_hit[k] += int(v > 0); sc_cnt[k] += v
                if key.endswith("/idle") and v: idle_hit[k] += 1
            s = score(cc); ss += s
            if s > 0: cell_rank.append((s, sid, key, cc))
        per_set.append((ss, sid, sc_cnt, res["stale_cells"]))
    per_set.sort(reverse=True); cell_rank.sort(key=lambda t: -t[0])
    L = [f"Sprite audit ({tag}): {len(data)} sets, {ncell} cells, dir {adir}", "",
         f"{'defect':<14}{'total':>8}{'cells':>8}{'idle cells':>12}"]
    for k in KINDS: L.append(f"{k:<14}{tot[k]:>8}{cells_hit[k]:>8}{idle_hit[k]:>12}")
    L.append(f"{'stale_cells':<14}{sum(p[3] for p in per_set):>8}")
    L += ["", "(island_limb = detached but body-coloured and within 3 px of the body: usually a legit hand/foot, not removed)",
          "", "Worst 30 sets (weighted score; counts per defect):"]
    for ss, sid, scc, st in per_set[:30]:
        L.append(f"{ss:8.1f}  {sid:<34} " + " ".join(f"{k}={v}" for k, v in scc.items() if v))
    L += ["", "Worst 40 cells:"]
    for s, sid, key, cc in cell_rank[:40]:
        L.append(f"{s:7.1f}  {sid:<34} {key:<32} " + " ".join(f"{k}={v}" for k, v in cc.items() if v))
    open(os.path.join(OUTDIR, f"summary_{tag}.txt"), "w").write("\n".join(L) + "\n")
    print("\n".join(L[:20 + len(KINDS)]))
    # contact sheets of the worst cells overall and per main defect
    sheet([(sid, key, s) for s, sid, key, cc in cell_rank], data, adir, os.path.join(OUTDIR, f"worst_cells_{tag}.png"),
          f"worst cells ({tag}): left plain, right marked (magenta island, cyan grey blob, yellow hole, red edge)")
    for k in ("island_ball", "island_debris", "grey_blob", "pinhole", "touch_edge", "fringe"):
        it = sorted(((cc[k], sid, key) for s, sid, key, cc in cell_rank if cc.get(k)), reverse=True)
        sheet([(sid, key, v) for v, sid, key in it], data, adir, os.path.join(OUTDIR, f"worst_{k}_{tag}.png"), f"{k} ({tag})", 20)

if __name__ == "__main__":
    main()
