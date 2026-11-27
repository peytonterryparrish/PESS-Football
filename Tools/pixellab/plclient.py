"""Minimal PixelLab v2 REST client. Token from env PIXELLAB_API_TOKEN (never stored in the repo).
Every call is logged to ArtWork/pixellab_log.jsonl (endpoint, seconds, job id) so spend is auditable."""
import base64, io, json, os, time, urllib.request, urllib.error
from PIL import Image

API = "https://api.pixellab.ai/v2"
LOG = os.path.join(os.path.dirname(__file__), "..", "..", "ArtWork", "pixellab_log.jsonl")

def _tok():
    t = os.environ.get("PIXELLAB_API_TOKEN")
    if not t: raise SystemExit("PIXELLAB_API_TOKEN not set")
    return t

def _req(method, path, body=None, timeout=120):
    data = json.dumps(body).encode() if body is not None else None
    r = urllib.request.Request(API + path, data=data, method=method,
        headers={"Authorization": "Bearer " + _tok(), "Content-Type": "application/json"})
    for attempt in range(6):
        try:
            with urllib.request.urlopen(r, timeout=timeout) as resp:
                return json.loads(resp.read().decode())
        except urllib.error.HTTPError as e:
            msg = e.read().decode()[:800]
            if e.code in (429, 500, 502, 503, 504) and attempt < 5:
                time.sleep(5 * (attempt + 1)); continue
            raise RuntimeError(f"{method} {path} -> {e.code}: {msg}")
        except (urllib.error.URLError, TimeoutError):
            if attempt < 5: time.sleep(5 * (attempt + 1)); continue
            raise

def b64img(img):
    buf = io.BytesIO(); img.save(buf, "PNG")
    return {"type": "base64", "base64": base64.b64encode(buf.getvalue()).decode(), "format": "png"}

def to_img(obj):
    if isinstance(obj, dict): obj = obj.get("base64")
    if obj.startswith("data:"): obj = obj.split(",", 1)[1]
    return Image.open(io.BytesIO(base64.b64decode(obj))).convert("RGBA")

def balance(): return _req("GET", "/balance")

def _log(rec):
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "a") as f: f.write(json.dumps(rec) + "\n")

def run_job(path, body, poll=5, max_wait=900, tag=""):
    t0 = time.time()
    r = _req("POST", path, body)
    jid = r.get("background_job_id")
    if not jid:
        _log({"path": path, "tag": tag, "sync": True, "s": round(time.time() - t0, 1)}); return r
    while True:
        time.sleep(poll)
        j = _req("GET", f"/background-jobs/{jid}")
        st = j.get("status")
        if st == "completed":
            _log({"path": path, "tag": tag, "job": jid, "s": round(time.time() - t0, 1)})
            return j.get("last_response") or j
        if st == "failed":
            _log({"path": path, "tag": tag, "job": jid, "failed": True})
            raise RuntimeError(f"job failed: {json.dumps(j)[:800]}")
        if time.time() - t0 > max_wait: raise TimeoutError(jid)

DIRS8 = ["south", "south-east", "east", "north-east", "north", "north-west", "west", "south-west"]

def rotations8(img, description=None, seed=0, tag=""):
    body = {"first_frame": b64img(img), "first_frame_direction": "south", "no_background": True, "seed": seed}
    if description: body["description"] = description
    r = run_job("/generate-8-rotations-v3", body, tag=tag)
    return [to_img(x) for x in r["images"]]

def estimate_skeleton(img):
    r = _req("POST", "/estimate-skeleton", {"image": b64img(img)})
    return r

def pose_frames(img, first_kps, frames_kps, description, action, direction, view="low top-down", seed=0, tag=""):
    body = {"description": description, "action": action, "direction": direction, "view": view,
            "first_frame": b64img(img), "first_frame_keypoints": first_kps, "keypoints": frames_kps,
            "template_id": "mannequin", "seed": seed, "no_background": True}
    r = run_job("/animate-with-skeleton-v3", body, poll=8, tag=tag)
    return [to_img(x) for x in r["images"]], r

# v0.9.8: 12 directions, 30 degrees apart, clockwise from south (facing the camera). The seven left-to-right unique ones
# are drawn; the other five are their mirror images (a330 = mirrored a030, ...).
DIRS12 = ["south", "a030", "a060", "east", "a120", "a150", "north", "a210", "a240", "west", "a300", "a330"]
DRAWN12 = ["south", "a030", "a060", "east", "a120", "a150", "north"]
MIRROR12 = {"a330": "a030", "a300": "a060", "west": "east", "a240": "a120", "a210": "a150"}

def rotate(img, degrees, seed=0, tag=""):
    """turn a figure by `degrees` (clockwise seen from above: south -> east is +90) with /rotate; img up to 128 px"""
    s = 128 if max(img.size) > 64 else 64
    cv = Image.new("RGBA", (s, s), (0, 0, 0, 0)); cv.paste(img, ((s - img.width) // 2, (s - img.height) // 2), img)
    t0 = time.time()
    r = _req("POST", "/rotate", {"image_size": {"width": s, "height": s}, "from_image": b64img(cv), "direction_change": int(degrees),
                                 "from_view": "low top-down", "to_view": "low top-down", "seed": seed})
    _log({"path": "/rotate", "tag": tag, "sync": True, "s": round(time.time() - t0, 1)})
    out = to_img(r["image"])
    return out.crop(((s - img.width) // 2, (s - img.height) // 2, (s - img.width) // 2 + img.width, (s - img.height) // 2 + img.height))

def lone(img, max_px=80):
    """drop small detached islands (stray pixels, lines, a drawn ball) next to the figure; islands bigger than max_px are
    kept (a limb the model drew a pixel apart)"""
    px = img.load(); W, H = img.size; seen = set(); comps = []
    for y in range(H):
        for x in range(W):
            if (x, y) in seen or px[x, y][3] < 100: continue
            st = [(x, y)]; c = []; seen.add((x, y))
            while st:
                a, b = st.pop(); c.append((a, b))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        q = (a + dx, b + dy)
                        if 0 <= q[0] < W and 0 <= q[1] < H and q not in seen and px[q][3] >= 100: seen.add(q); st.append(q)
            comps.append(c)
    if len(comps) > 1:
        big = max(comps, key=len)
        for c in comps:
            if c is not big and len(c) <= max_px:
                for q in c: px[q] = (0, 0, 0, 0)
    return img

def turns12(rots8, seed=0, tag=""):
    """the 12-direction standing turns from the 8-direction ones (dict name -> image): south/east/north are reused.
    /rotate's sign is "towards the east side" from both the front and the back view (checked against the 8-direction
    turns), so: a030/a060 = south +30/+60, a120 = east -30, a150 = north +30. Stray islands are dropped."""
    out = {"south": rots8["south"], "east": rots8["east"], "north": rots8["north"]}
    out["a030"] = lone(rotate(rots8["south"], 30, seed, tag + "_a030")); out["a060"] = lone(rotate(rots8["south"], 60, seed, tag + "_a060"))
    out["a120"] = lone(rotate(rots8["east"], -30, seed, tag + "_a120")); out["a150"] = lone(rotate(rots8["north"], 30, seed, tag + "_a150"))
    for m, s in MIRROR12.items(): out[m] = out[s].transpose(Image.FLIP_LEFT_RIGHT)
    return out
