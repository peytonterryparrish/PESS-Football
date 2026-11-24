"""Contact sheet: sheet.py out.png scale cols img1 img2 ... (labels = file stems)."""
import sys, os
from PIL import Image, ImageDraw
def make(paths, out, scale=4, cols=8, bg=(46, 110, 52, 255)):
    ims = [Image.open(p).convert("RGBA") for p in paths]
    w = max(i.width for i in ims) * scale; h = max(i.height for i in ims) * scale
    rows = (len(ims) + cols - 1) // cols
    sh = Image.new("RGBA", (cols * w, rows * (h + 14)), bg); d = ImageDraw.Draw(sh)
    for n, (p, i) in enumerate(zip(paths, ims)):
        x, y = (n % cols) * w, (n // cols) * (h + 14)
        sh.alpha_composite(i.resize((i.width * scale, i.height * scale), Image.NEAREST), (x, y + 14))
        d.text((x + 2, y + 1), os.path.splitext(os.path.basename(p))[0][:28], fill=(255, 255, 255, 255))
    sh.save(out)
if __name__ == "__main__":
    make(sys.argv[4:], sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
