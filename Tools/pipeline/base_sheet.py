"""Contact sheet of a base-body take: rows = directions, columns = poses (2x zoom, grass background).
  python -I Tools/pipeline/base_sheet.py B01 0 run_contactR run_downR ... [--out path]"""
import os, sys
from PIL import Image, ImageDraw
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
args = [a for a in sys.argv[1:] if not a.startswith("--")]; out = next((a[6:] for a in sys.argv if a.startswith("--out=")), None)
bid, take, poses = args[0], args[1], args[2:]
dirs = ["south", "south-east", "east", "north-east", "north"]
sheet = Image.new("RGBA", (96 * (len(poses) + 1), 96 * len(dirs)), (58, 118, 48, 255)); d = ImageDraw.Draw(sheet)
for j, x in enumerate(dirs):
    rot = os.path.join(ROOT, "ArtWork", "raw_base", bid, f"rot_{x}.png")
    if os.path.exists(rot): sheet.alpha_composite(Image.open(rot).convert("RGBA"), (0, j * 96))
    for i, p in enumerate(poses):
        f = os.path.join(ROOT, "ArtWork", "raw_base", bid, x, f"t{take}", f"{p}.png")
        if os.path.exists(f): sheet.alpha_composite(Image.open(f).convert("RGBA"), ((i + 1) * 96, j * 96))
sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
out = out or os.path.join(ROOT, "ArtWork", "base_pilot", f"{bid}_t{take}_{poses[0]}.png"); os.makedirs(os.path.dirname(out), exist_ok=True)
sheet.save(out); print(out)
