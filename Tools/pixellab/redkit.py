"""v0.9.6: put a red-shirt / white-shorts / red-socks sprite (the art-reset candidates) into the template kit.
Red (hue 340-12, saturated) above the waist is shirt, below the knees socks; pale low-saturation pixels between the
waist and the knees are shorts. Skin, hair and boots are left alone."""
import colorsys, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pipeline"))
import kit

def hsv(c):
    h, s, v = colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255); return h * 360, s, v

def to_template(img):
    img = img.convert("RGBA").copy(); px = img.load(); W, H = img.size
    l, top, r, bot = img.getbbox(); n = {}
    for y in range(H):
        for x in range(W):
            c = px[x, y]
            if c[3] < 128: continue
            f = (y - top) / max(1, bot - top); h, s, v = hsv(c)
            red = (h >= 338 or h <= 14) and s > 0.42 and v > 0.18
            reg = None
            if red and 0.12 < f < 0.56: reg = "shirt"
            elif red and f >= 0.62: reg = "socks"
            elif red: reg = "shirt" if f < 0.59 else "socks"
            elif 0.42 < f < 0.74 and s < 0.24 and v > 0.42: reg = "shorts"
            if not reg: continue
            vv = v if reg != "shorts" else (v - 0.4) / 0.6
            i = 0 if vv < 0.32 else 1 if vv < 0.52 else 2 if vv < 0.80 else 3
            px[x, y] = kit.hx(kit.TEMPLATE[reg][i]) + (255,); n[reg] = n.get(reg, 0) + 1
    return img, n
