"""v0.7 goals at real proportion: the owner's goal art (R08 stadium pack, Close_6x) is drawn at true scale, but the
players are drawn at half height (HeightScale 0.5), so a true-scale goal stood 2.7 players tall. The owner asked for
realistic goals, so every layer is squashed to half height along the camera's vertical axis (Az) about the goal line,
keeping the art's own pixels (nearest sampling, nothing redrawn):

  output pixel -> the point on the goal-line plane x = +-52.5 m it shows under the half-height projection (y, z/2)
              -> that point's pixel in the original art (y, z)

The rear net and shadow are treated as lying on the same plane; the net's 2 m depth runs almost horizontally in this
camera, so the error is a fraction of a pixel.

  python -I Tools/pipeline/goals.py      -> GameData/Goals/<Stage>/<Goal>_<Layer>.png (same placement as the pack)"""
import json, os
import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
G = os.path.join(ROOT, "ArtSource", "Packs", "R08_Final_93_Club_Stadiums_4K", "Goals")
OUT = os.path.join(ROOT, "GameData", "Goals")
O = np.array([1920.0, 1080.0]); AX = np.array([21.889, -1.960]); AY = np.array([-10.298, -8.840]); AZ = np.array([2.136, -22.533])
K = 0.5          # HeightScale: the goal's height as drawn, relative to true scale
S = 6.0          # Close_6x: art pixels per atlas pixel

def warp(path, xywh, gx):
    src = np.asarray(Image.open(path).convert("RGBA"))
    H, W = src.shape[:2]
    v, u = np.mgrid[0:H, 0:W].astype(np.float64)
    ax = xywh[0] + (u + 0.5) / S; ay = xywh[1] + (v + 0.5) / S           # atlas position of each output pixel
    base = O + AX * gx
    # solve  [AY, K*AZ] [y, z] = a - base: the point drawn here at half height is at true height z in the art
    M = np.array([[AY[0], K * AZ[0]], [AY[1], K * AZ[1]]]); Mi = np.linalg.inv(M)
    rx, ry = ax - base[0], ay - base[1]
    y = Mi[0, 0] * rx + Mi[0, 1] * ry
    zd = Mi[1, 0] * rx + Mi[1, 1] * ry
    z = zd                     # solving against K*AZ gives the true height directly: the pixel shows the art at that height
    sx = base[0] + AY[0] * y + AZ[0] * z; sy = base[1] + AY[1] * y + AZ[1] * z
    pu = np.floor((sx - xywh[0]) * S).astype(int); pv = np.floor((sy - xywh[1]) * S).astype(int)
    ok = (pu >= 0) & (pu < W) & (pv >= 0) & (pv < H)
    out = np.zeros_like(src)
    out[ok] = src[pv[ok], pu[ok]]
    return Image.fromarray(out, "RGBA")

def main():
    lay = json.load(open(os.path.join(G, "Goal_Layout.json")))
    for stage, goals in lay["stages"].items():
        for gname, g in goals.items():
            gx = lay["goal_line_world_x_m"][gname]
            for layer, info in g["Close_6x"].items():
                if layer == "Composite": continue
                im = warp(os.path.join(G, info["filename"]), info["xywh_4k_px"], gx)
                d = os.path.join(OUT, stage); os.makedirs(d, exist_ok=True)
                im.save(os.path.join(d, f"{gname}_{layer}.png"), optimize=True)
                print(stage, gname, layer, im.size)

if __name__ == "__main__":
    main()
