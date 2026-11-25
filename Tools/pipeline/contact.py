"""Tile screenshots into one labelled contact sheet: contact.py out.png in1.png in2.png ... [--w=640]"""
import sys
from PIL import Image, ImageDraw
a = [x for x in sys.argv[1:] if not x.startswith("--")]; w = int(next((x[4:] for x in sys.argv if x.startswith("--w=")), 640))
out, ins = a[0], a[1:]; cols = 3; ims = [Image.open(p).convert("RGB") for p in ins]
h = int(w * ims[0].height / ims[0].width); sheet = Image.new("RGB", (cols * w, ((len(ims) + cols - 1) // cols) * h)); d = ImageDraw.Draw(sheet)
for i, (p, im) in enumerate(zip(ins, ims)):
    x, y = (i % cols) * w, (i // cols) * h; sheet.paste(im.resize((w, h), Image.LANCZOS), (x, y)); d.text((x + 6, y + 6), p.replace("\\", "/").split("/")[-1], fill=(255, 255, 0))
sheet.save(out)
