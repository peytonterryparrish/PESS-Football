"""Turn -pfrecord bursts (Saved/Screenshots/WindowsEditor/rec_<burst>_<frame>.png) into half-size GIF clips.
  python -I Tools/pipeline/rec_gifs.py <out_dir>"""
import glob, os, re, sys
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Saved", "Screenshots", "WindowsEditor")
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Saved", "rec")
os.makedirs(out, exist_ok=True)
bursts = {}
for f in glob.glob(os.path.join(SRC, "rec_*_*.png")):
    m = re.match(r"rec_(\d+)_(\d+)\.png", os.path.basename(f))
    if m: bursts.setdefault(int(m.group(1)), []).append((int(m.group(2)), f))
for b, fr in sorted(bursts.items()):
    ims = [Image.open(f).convert("RGB").resize((960, 540), Image.LANCZOS) for _, f in sorted(fr)]
    ims[0].save(os.path.join(out, f"clip_{b:03d}.gif"), save_all=True, append_images=ims[1:], duration=83, loop=0)
print(len(bursts), "clips ->", out)
