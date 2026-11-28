"""v0.9.8 art approval round: 20-30 animation candidates per game action for ONE character (N01, and NK1 his keeper),
each a different take on the motion (animate-with-text-v3 from the body's own frame), for the owner to approve one per
action. Loops start and end on the same mid-cycle frame so the cycle closes. One direction per action (the one that
reads best); the approved take is produced in every direction afterwards.
  python Tools/pixellab/anim_candidates.py [action ...] [--n=20]
Output: ArtWork/anim_candidates/<action>/<NN>/f00.png... + meta.json   (timings: docs + fable advice, see SPEC dt)"""
import os, sys, json, random, concurrent.futures as cf
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import plclient as pl
import debake_ball

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RAW = os.path.join(ROOT, "ArtWork", "raw_base_n")
OUT = os.path.join(ROOT, "ArtWork", "anim_candidates")
WHO = {"N01": "a stocky retro 16-bit footballer, blue shirt, green shorts, red socks, black boots",
       "NK1": "a stocky retro 16-bit goalkeeper, blue shirt with long magenta sleeves, grey gloves, green shorts, red socks"}
RULES = "Keep his exact look, colours, size and pixel style. His head stays clearly visible in every frame. No ball, no shadow."

# action: body, first frame (dir/pose or rot_<dir>), loop, frames, canvas, dt (s per frame in the recording), take, variations
SPEC = {
 "idle":      ("N01", "rot_south", True, 8, 96, 0.14, "standing idle on the spot, facing the camera, breathing, tiny weight shifts",
               ["relaxed, arms loose by his sides", "alert on his toes, slight bounce", "hands on hips for a moment, then loose", "shoulders rolling, ready",
                "feet shoulder-width, calm breathing", "bobbing lightly like waiting for a pass", "shaking out his arms", "slight sway side to side"]),
 "walk":      ("N01", "east/walk_contactR", True, 8, 96, 0.12, "walking in place, side view, a natural walk cycle",
               ["upright and relaxed, small arm swing", "purposeful stride, arms swinging", "slow casual walk, hands low", "brisk walk, slight lean",
                "tired walk, shoulders down", "confident walk, chest out"]),
 "run":       ("N01", "east/run_contactR", True, 8, 96, 0.07, "running in place, side view, a smooth looping run cycle",
               ["15 degree forward lean, rear heel kicking up to the backside", "steady jog, arms bent at 90 degrees", "fast run, both feet off the ground mid-stride",
                "athletic run, knees high, strong arm drive", "relaxed run, loose arms", "powerful run, long stride, compact arms", "bouncy run, springy steps"]),
 "sprint":    ("N01", "east/sprint_contactR", True, 6, 96, 0.085, "sprinting flat out in place, side view, a looping sprint cycle",
               ["25 degree lean, arms driving to chin height", "long powerful stride, fists pumping", "explosive sprint, high knees",
                "all-out sprint, head steady, body low", "sprinter form, arms straight and fast"]),
 "dribble":   ("N01", "east/dribble_touchR", True, 8, 96, 0.09, "dribbling a football in place, side view, short quick steps with touches, but draw no ball",
               ["head down, slightly crouched, arms out wide for balance", "close control, tiny touches, body over the ball", "dribbling at speed, pushing the ball ahead",
                "jinking dribble, light on his feet", "upright dribble, head up scanning", "low centre of gravity, quick feet"]),
 "pass":      ("N01", "rot_east", False, 8, 96, 0.07, "playing a side-foot pass, side view: plant the standing foot, swing the kicking leg, follow through",
               ["inside of the foot, hips open, opposite arm out", "crisp short pass, compact swing", "driven pass, bigger back-swing", "first-time pass while moving"]),
 "shot":      ("N01", "rot_east", False, 10, 96, 0.07, "striking a powerful shot, side view: plant, big back-swing with heel to backside, strike, high follow-through",
               ["laces drive, both arms out, body leaving the ground", "full-blooded shot, chest opened on the back-swing", "curled shot, body leaning over",
                "thunderbolt, follow-through leg above the hip", "compact powerful strike, landing on the kicking foot"]),
 "volley":    ("N01", "rot_east", False, 8, 96, 0.08, "hitting a volley, side view: body tilted away, kicking leg swings horizontal at hip height",
               ["arms wide for balance", "side volley, leaning far back", "half-volley, knee over the ball", "jumping volley"]),
 "header":    ("N01", "rot_east", False, 8, 96, 0.09, "jumping header, side view: jump with back arched and arms up, snap forward at the waist, head leads, land",
               ["powerful leap", "glancing header", "standing header, small jump", "towering header, knees up"]),
 "chest":     ("N01", "rot_east", False, 8, 96, 0.1, "chest control, side view: chest out, arms back and wide, knees soft, then looking down to his feet",
               ["leaning back to cushion", "quick chest-down then step", "upright cushion, arms wide"]),
 "tackle":    ("N01", "rot_east", False, 8, 96, 0.08, "standing tackle, side view: long lunging step, leg stabbed across to poke the ball, recover upright",
               ["body upright, poking toe", "block tackle, planting both feet", "lunging tackle, arms out"]),
 "slide":     ("N01", "rot_east", False, 12, 128, 0.07, "sliding tackle, side view: drop and slide along the grass, lead leg straight, trailing leg tucked, one arm trailing, then get up",
               ["body 20 degrees off the grass", "long slide on the hip", "aggressive slide, both legs low", "clean slide, hand down then spring up"]),
 "fall":      ("N01", "rot_east", False, 12, 128, 0.09, "tripped and falling, side view: stumble, fall flat on the grass, lie, kneel, stand back up",
               ["falls forward onto his front", "falls sideways and rolls", "stumbles and goes down hard", "dramatic fall, arms out"]),
 "slide2":    ("N01", "rot_east", False, 12, 160, 0.07, "slide tackle seen from the side: he runs in, drops onto his left hip and slides feet-first along the grass to the right, body low and leaning back, right leg straight out in front at ankle height, left leg bent under him, left hand trailing on the grass, then he stops and pushes himself back up",
               ["fast and low, a long clean slide", "aggressive slide, studs showing", "controlled slide, arm out for balance", "sliding block, both legs together",
                "classic 1990s slide tackle", "late lunge into a slide"]),

 # ---- v0.10.0 approval round: more takes for play on the ball, defending, keepers and transitions (25 each) ----
 "dribble2":  ("N01", "east/dribble_touchR", True, 8, 96, 0.09, "dribbling a football in place, side view, close control with quick touches, but draw no ball",
               ["head down, slightly crouched, arms out wide for balance", "dribbling at pace, pushing the ball ahead with the laces", "tight close control, body over the ball",
                "shifting weight side to side, ready to beat a man", "upright dribble, head up scanning the pitch", "skilful light-footed dribble", "powerful driving run with the ball"]),
 "pass_short": ("N01", "rot_east", False, 8, 96, 0.08, "a short side-foot pass, side view: small step, plant the standing foot beside the ball, open the hips, push the ball with the inside of the foot, short follow-through",
               ["crisp and compact", "relaxed one-touch pass", "weighted pass, arms out for balance", "quick pass while moving"]),
 "pass_long": ("N01", "rot_east", False, 10, 96, 0.08, "a long driven pass, side view: two-step approach, plant, kicking leg swings back then through with the laces, body leaning back slightly, high follow-through",
               ["raking 40-yard pass", "driven low pass", "lofted switch of play", "powerful long ball"]),
 "through_ball": ("N01", "rot_east", False, 8, 96, 0.08, "a disguised through ball, side view: head up looking, small plant, slides the ball forward with the inside of the foot into space, follow-through pointing where it goes",
               ["clever reverse pass", "weighted through pass", "quick threaded pass", "chipped through ball"]),
 "cross":     ("N01", "rot_east", False, 10, 96, 0.08, "crossing the ball from the wing, side view: run-up, plant, wraps his foot around the ball, body leaning back and away, kicking leg swings across his body, arms out wide",
               ["whipped in-swinging cross", "high hanging cross", "low driven cross", "early cross on the run"]),
 "corner":    ("N01", "rot_east", False, 14, 128, 0.09, "taking a corner kick, side view facing right: stands by the ball, raises one arm, steps back, short run-up, plants and swings his kicking leg to curl the ball in, follow-through",
               ["arm raised to signal, then a curling delivery", "short run-up, whipped corner", "longer run-up, big swing", "calm, deliberate corner"]),
 "jockey":    ("N01", "rot_east", True, 8, 96, 0.1, "defender jockeying in place, side view: low side-on stance, knees bent, arms out, small shuffling steps, ready to tackle",
               ["patient jockey, eyes on the ball", "aggressive closing down", "staying on his toes, side-stepping", "crouched and ready"]),
 "block":     ("N01", "rot_east", False, 8, 96, 0.08, "defender blocking a shot, side view: steps across, turns his body side-on, leg stretched out to block, arms tucked behind his back",
               ["brave sliding block", "standing block, leg out", "throws his body in the way", "hands behind back, side-on block"]),
 "getup":     ("N01", "raw12:east/fall/f06", False, 10, 128, 0.1, "football player lying flat on the grass, side view: pushes up onto his hands and knees, gets one foot under him and stands back up, ready",
               ["slowly, a bit dazed", "quick and sharp back on his feet", "rolls over then stands", "pushes up with both arms"]),
 "stop":      ("N01", "east/run_contactR", False, 8, 96, 0.08, "running player coming to a stop, side view: braking steps, leaning back, arms out, settling into a balanced standing stance",
               ["sharp stop, plant and brake", "gradual slow-down to a jog then stand", "skidding stop", "controlled stop, ready to move again"]),
 "turn":      ("N01", "east/run_contactR", False, 8, 96, 0.08, "running player turning around to run the other way, side view: plants his foot, drops his hips, swivels round and pushes off in the new direction",
               ["sharp 180 degree turn", "rounded turn, staying low", "quick spin", "check back on the plant foot"]),
 "gk_goalkick2": ("NK1", "rot_east", False, 16, 160, 0.15, "goalkeeper taking a goal kick, side view, facing right: walks backwards to the left for four steps while still facing right, settles, then runs forward three strides, plants his standing foot and kicks long with a big follow-through",
               ["measured steps back, then an accelerating run-up", "relaxed back-pedal, then a powerful strike", "short sharp steps back, arms wide on the kick",
                "slow deliberate walk back, confident run-up", "big plant, follow-through carries him forward"]),
 "gk_hold_walk": ("NK1", "east/gk_hold_ball", True, 8, 96, 0.12, "goalkeeper walking in place, side view, holding the football in both hands against his chest, calm steady steps",
               ["ball tucked into his chest", "ball held out in front with both gloves", "relaxed walk, ball in one hand at his side then both", "looking up the pitch as he walks"]),
 "gk_getup":  ("NK1", "raw12:south/gk_dive_low/f10", False, 10, 160, 0.1, "goalkeeper lying on his side on the grass after a dive, facing the camera: stays down a moment, pushes up onto one knee, then stands back up into his ready stance",
               ["frustrated, slow to get up", "springs straight back up", "rolls and rises onto one knee first", "pushes up with both hands"]),
 "throwin":   ("N01", "rot_east", False, 10, 96, 0.1, "throw-in, side view: both hands bring the ball behind his head with back arched, both feet down, throw forward, arms finish straight out in front, but draw no ball",
               ["long throw, big arch", "short throw, quick", "step-in throw"]),
 "celebrate": ("N01", "rot_south", False, 12, 96, 0.12, "goal celebration facing the camera",
               ["both arms up, then fist pump", "running with one finger pointing up", "jumping fist pump", "arms out wide like an aeroplane", "kissing the badge then arms up",
                "double fist pump, roaring"]),
 "gk_ready":  ("NK1", "south/gk_ready", True, 8, 96, 0.12, "goalkeeper ready stance facing the camera: wide stance, knees bent, hands at hip height out in front, bouncing lightly on his toes",
               ["calm small bounce", "alert, shifting weight", "side-step shuffle left and right, feet never crossing", "crouched lower, gloves open"]),
 "gk_catch":  ("NK1", "south/gk_ready", False, 8, 96, 0.1, "goalkeeper catching a ball at his chest facing the camera, elbows in, shoulders rounded over it, but draw no ball",
               ["clean chest catch", "catch and clutch to the body", "stepping forward into the catch"]),
 "gk_high":   ("NK1", "south/gk_ready", False, 8, 96, 0.1, "goalkeeper jumping to catch a high ball facing the camera: one knee raised, arms up in front of his face line, head visible below the hands, land, but draw no ball",
               ["claims it at full stretch", "confident high catch", "jumps off one foot"]),
 "gk_scoop":  ("NK1", "south/gk_ready", False, 8, 96, 0.12, "goalkeeper gathering a low rolling ball facing the camera: one knee to the grass, scoop it to the chest, stand up, but draw no ball",
               ["kneeling scoop", "bending down to scoop", "smother then rise"]),
 "gk_punch":  ("NK1", "south/gk_ready", False, 8, 96, 0.09, "goalkeeper jumping to punch the ball away facing the camera: one fist driven up above his head, other arm down for balance, land",
               ["one-fist punch", "two-fist punch", "leaping punch with knee up"]),
 "gk_dive_low":  ("NK1", "south/gk_ready", False, 14, 160, 0.075, "goalkeeper diving low to his right (to the left of the picture), facing the camera: set his weight, push off the near leg, fly sideways close to the grass with his body in a straight line, arms in a narrow V reaching for the ball, head visible between the arms, land on his side, lie, then kneel and get up",
               ["body nearly flat to the grass, quick and precise", "full stretch along the ground", "strong push off, arms together", "smothering dive, landing on the forearm",
                "intentional athletic dive, legs slightly scissored", "spring off the near foot, fingertips reaching"]),
 "gk_dive_high": ("NK1", "south/gk_ready", False, 14, 160, 0.08, "goalkeeper diving high to his right (to the left of the picture), facing the camera: set his weight, push off, fly through the air with his body angled 35 degrees up, top arm reaching high, head visible below the top arm, land on his side, lie, then kneel and get up",
               ["spectacular full-stretch save", "fingertip save to the top corner", "powerful leap, both arms up", "acrobatic dive, legs together",
                "precise, intentional flying save", "one-handed reach to the corner"]),
 "gk_throw":  ("NK1", "east/gk_throw_back", False, 8, 96, 0.1, "goalkeeper overarm throw, side view: ball arm straight back, front arm pointing at the target, long step, release, follow through, but draw no ball",
               ["bowling overarm throw", "javelin-style throw", "quick roll-out underarm"]),
 "gk_punt":   ("NK1", "rot_east", False, 10, 96, 0.08, "goalkeeper punt kick, side view: holding the ball at waist height, drops it, kicks it on the volley with the laces, kicking leg finishes at chest height, but draw no ball",
               ["big booming punt", "drop-kick", "low driven punt"]),
 "gk_punt2":  ("NK1", "east/gk_hold_ball", False, 10, 128, 0.08, "goalkeeper punt seen from the side, facing right: holding the ball out in front with both hands, he takes two quick steps, drops the ball and swings his right leg through hard, kicking leg finishing up at chest height, arms out wide, then he lands",
               ["big booming punt up the field", "drop-kick, ball and foot meet at the grass", "low driven punt, flat follow-through", "side-volley clearance",
                "relaxed rhythmic punt", "powerful punt, body leaning back"]),
 "gk_goalkick": ("NK1", "rot_east", False, 16, 160, 0.15, "goalkeeper taking a goal kick, side view, always facing right: he walks BACKWARDS to the left of the picture for four clear steps while still facing right, stops and settles, then runs forward to the right for three strides, plants his standing foot and swings his kicking leg through with a big follow-through",
               ["measured steps back, then an accelerating run-up", "relaxed back-pedal, then a powerful strike", "short sharp steps back, arms wide on the kick",
                "slow deliberate walk back, confident run-up", "big plant, follow-through carries him forward"]),
}
FIGX = {"gk_goalkick2": 0.62, "corner": 0.4, "slide2": 0.25, "gk_punt2": 0.35, "gk_goalkick": 0.62, "slide": 0.3, "fall": 0.35}
COUNT = {"gk_dive_low": 15, "gk_dive_high": 15}

def first_frame(body, src, canvas, action="", turn=None):
    """the body's own frame (96 px) for `src`, optionally turned by `turn(im)` (12-direction production), placed on the
    action's canvas. "raw12:<dir>/<anim>/fNN" starts from a frame of an approved animation (already on its canvas)."""
    if src.startswith("raw12:"):
        im = Image.open(os.path.join(ROOT, "ArtWork", "raw_12", body, src[6:] + ".png")).convert("RGBA")
        return turn(im) if turn else im
    if src.startswith("rot_"): im = Image.open(os.path.join(RAW, body, src + ".png")).convert("RGBA")
    else:
        d, p = src.split("/"); im = Image.open(os.path.join(RAW, body, d, "t0", p + ".png")).convert("RGBA")
    if turn: im = turn(im)
    if canvas == 96: return im
    l, t, r, b = im.getbbox(); cv = Image.new("RGBA", (canvas, canvas if canvas < 160 else 128), (0, 0, 0, 0))
    W, H = cv.size; fig = im.crop((l, t, r, b)); cx = int(W * FIGX.get(action, 0.5))
    cv.paste(fig, (cx - (r - l) // 2, H - 10 - (b - t)), fig); return cv

UPRIGHT = {"idle", "walk", "run", "sprint", "dribble", "pass", "shot", "volley", "chest", "tackle", "throwin", "gk_ready", "gk_catch", "gk_throw", "gk_punt", "gk_goalkick"}

def clean(im, body):
    """remove a drawn ball: loose white islands always; ball blobs (white with dark panels) on outfielders only (the
    keeper's grey gloves look like a ball to the blob test)"""
    out, n = debake_ball.clean(im, blobs=(body != "NK1"))
    return pl.lone(out)    # and any other detached island (a small dark ball, stray pixels): the body is one piece

def qa(action, ims):
    """automatic reject: the body changes size (PixelLab sometimes zooms in mid-clip) or hardly moves"""
    hs = [im.getbbox() for im in ims]
    if any(b is None for b in hs): return "empty frame"
    h0 = hs[0][3] - hs[0][1]
    if action in UPRIGHT:
        for b in hs[1:]:
            if not (0.8 <= (b[3] - b[1]) / h0 <= 1.2) and action not in ("shot", "volley", "gk_punt"): return "size %.2f" % ((b[3] - b[1]) / h0)
    from PIL import ImageChops
    moved = max(ImageChops.difference(ims[0], im).getbbox() is not None and sum(1 for p in ImageChops.difference(ims[0], im).getchannel("A").point(lambda v: 255 if v > 100 else 0).getdata() if p) for im in ims[1:])
    if moved < 40: return "no motion"
    return ""

def prompts(action, n):
    body, src, loop, fc, canvas, dt, take, var = SPEC[action]
    rnd = random.Random(action); extras = ["", "smooth, clean motion", "snappy, readable key poses", "weighty and grounded", "ISS Deluxe style SNES timing",
                                           "exaggerated anticipation and follow-through", "subtle and realistic"]
    out = []
    for i in range(n):
        v = var[i % len(var)]; e = extras[(i // len(var) + i) % len(extras)] if i >= len(var) else ""
        out.append(", ".join(x for x in [take, v, e] if x))
    return out

def one(action, i, text, seed):
    body, src, loop, fc, canvas, dt, take, var = SPEC[action]
    d = os.path.join(OUT, action, "%02d" % i)
    if os.path.exists(os.path.join(d, "meta.json")): return action, i, "kept"
    ff = first_frame(body, src, canvas, action)
    full = f"{WHO[body]}: {text}. {RULES}" + (" The animation loops: the last frame leads back into the first." if loop else "")
    body_ = {"first_frame": pl.b64img(ff), "action": full[:1000], "frame_count": fc, "seed": seed, "no_background": True}
    if loop: body_["last_frame"] = pl.b64img(ff)
    for attempt in range(3):
        r = pl.run_job("/animate-with-text-v3", body_, tag=f"cand_{action}_{i:02d}")
        ims = [clean(x, body) for x in (pl.to_img(y) for y in r["images"])]
        why = qa(action, ims)
        if not why: break
        print("  re-roll", action, i, why, flush=True); body_["seed"] += 1000
    os.makedirs(d, exist_ok=True)
    for k, im in enumerate(ims): im.save(os.path.join(d, "f%02d.png" % k))
    json.dump({"action": action, "body": body, "first": src, "loop": loop, "frames": len(ims), "dt": dt, "prompt": full, "seed": seed},
              open(os.path.join(d, "meta.json"), "w"), indent=1)
    return action, i, len(ims)

if __name__ == "__main__":
    acts = [a for a in sys.argv[1:] if not a.startswith("--")] or list(SPEC)
    n = next((int(a[4:]) for a in sys.argv if a.startswith("--n=")), 20)
    jobs = [(a, i, t, 900 + i * 7) for a in acts for i, t in enumerate(prompts(a, COUNT.get(a, n)))]
    print(len(jobs), "jobs", flush=True)
    with cf.ThreadPoolExecutor(int(os.environ.get("PF_PAR", "12"))) as ex:
        for f in cf.as_completed([ex.submit(one, *j) for j in jobs]):
            try: print(*f.result(), flush=True)
            except Exception as e: print("FAILED", str(e)[:300], flush=True)
