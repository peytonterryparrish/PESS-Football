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
