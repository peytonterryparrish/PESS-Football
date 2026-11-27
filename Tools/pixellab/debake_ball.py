"""v0.9.6: PixelLab sometimes draws a ball into a pose ("no ball" in the prompt or not); the game draws its own, so the
frame would show two. Finds white blobs (>= MIN px of near-white, low-saturation pixels) plus the grey/dark pentagon
pixels inside their box, and removes them: a pixel with body around it is filled with its most common opaque
neighbour, an outside one becomes transparent. Originals kept as <dir>/t<take>/_baked/<pose>.png.
  python Tools/pixellab/debake_ball.py ArtWork/raw_base_n/NK1 [--dry]"""
import os, shutil, sys
from collections import Counter
from PIL import Image

MIN = 8

def whiteish(c): return c[3] >= 128 and min(c[:3]) >= 178 and max(c[:3]) - min(c[:3]) < 40
def greyish(c): return c[3] >= 128 and max(c[:3]) - min(c[:3]) < 34 and max(c[:3]) >= 40

def find_blobs(im):
    px = im.load(); W, H = im.size; seen = set(); out = []
    for y in range(H):
        for x in range(W):
            if (x, y) in seen or not whiteish(px[x, y]): continue
            st = [(x, y)]; comp = []; seen.add((x, y))
            while st:
                a, b = st.pop(); comp.append((a, b))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        q = (a + dx, b + dy)
                        if 0 <= q[0] < W and 0 <= q[1] < H and q not in seen and whiteish(px[q]):
                            seen.add(q); st.append(q)
            if len(comp) >= MIN: out.append(comp)
    return out

def clean(im, blobs=True):
    im = im.convert("RGBA").copy(); px = im.load(); W, H = im.size; n = 0
    for comp in (find_blobs(im) if blobs else []):
        xs = [p[0] for p in comp]; ys = [p[1] for p in comp]
        x0, x1, y0, y1 = min(xs) - 1, max(xs) + 1, min(ys) - 1, max(ys) + 1
        if x1 - x0 > 14 or y1 - y0 > 14: continue      # too big for a ball: a sleeve or shorts highlight, leave it
        # a ball has its dark panels inside the white; a glove or a highlight doesn't
        dark = sum(1 for x in range(x0 + 1, x1) for y in range(y0 + 1, y1) if 0 <= x < W and 0 <= y < H and px[x, y][3] >= 128 and max(px[x, y][:3]) < 120 and max(px[x, y][:3]) - min(px[x, y][:3]) < 30)
        if dark < 2: continue
        kill = set(comp) | {(x, y) for x in range(max(0, x0), min(W, x1 + 1)) for y in range(max(0, y0), min(H, y1 + 1)) if greyish(px[x, y])}
        for _ in range(3):   # fill from the outside in
            for (x, y) in list(kill):
                nb = [px[x + dx, y + dy] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + dx < W and 0 <= y + dy < H and (x + dx, y + dy) not in kill and px[x + dx, y + dy][3] >= 128]
                if len(nb) >= 3: px[x, y] = Counter(nb).most_common(1)[0][0]; kill.discard((x, y))
        for (x, y) in kill: px[x, y] = (0, 0, 0, 0)
        n += 1
    # a loose ball: a mostly white island not joined to the body
    seen = set(); comps = []
    for y in range(H):
        for x in range(W):
            if (x, y) in seen or px[x, y][3] < 128: continue
            st = [(x, y)]; comp = []; seen.add((x, y))
            while st:
                a, b = st.pop(); comp.append((a, b))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        q = (a + dx, b + dy)
                        if 0 <= q[0] < W and 0 <= q[1] < H and q not in seen and px[q][3] >= 128: seen.add(q); st.append(q)
            comps.append(comp)
    if len(comps) > 1:
        big = max(comps, key=len)
        for comp in comps:
            if comp is big or len(comp) > 60: continue
            if sum(1 for q in comp if whiteish(px[q]) or greyish(px[q])) >= 0.6 * len(comp) and len(comp) >= 6:
                for q in comp: px[q] = (0, 0, 0, 0)
                n += 1
    return im, n

if __name__ == "__main__":
    import re
    root = sys.argv[1]; dry = "--dry" in sys.argv; tot = 0
    # keepers: never touch poses where his white gloves are out front (dives, catches); a ball held there is drawn over anyway
    skip = re.compile(r"dive|stretch|reach|push|load|ready|shuffle|catch|hold|scoop|punch") if os.path.basename(root.rstrip("/\\")).startswith(("NK", "BK")) else None
    for x in sorted(os.listdir(root)):
        dx = os.path.join(root, x)
        if not os.path.isdir(dx): continue
        for t in sorted(os.listdir(dx)):
            td = os.path.join(dx, t)
            if not (t.startswith("t") and os.path.isdir(td)): continue
            for f in sorted(os.listdir(td)):
                if not f.endswith(".png") or (skip and skip.search(f)): continue
                p = os.path.join(td, f); im, n = clean(Image.open(p))
                if not n: continue
                tot += 1; print(x, t, f[:-4], n)
                if dry: continue
                os.makedirs(os.path.join(td, "_baked"), exist_ok=True)
                if not os.path.exists(os.path.join(td, "_baked", f)): shutil.copy(p, os.path.join(td, "_baked", f))
                im.save(p)
    print("frames cleaned:", tot)
