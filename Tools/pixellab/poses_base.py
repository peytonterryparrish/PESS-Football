"""v0.8 base-body pilot: pose sets for a few hand-checked bodies whose kits are coloured by code (kit-keyed).

The v0.7 lesson: posing every frame on its own (and mixing unrelated poses in one generation job) gives cycles whose
frames don't belong together. Here every ANIMATION is generated in its own job, in order, so PixelLab sees the whole
cycle at once, and the cycles are natural (gait-data angles, ISS-Deluxe timings: docs/ISS_STUDY.md), not exaggerated.

Angles in degrees on the stick figure of poses.py (hip flexion + = thigh forward, knee + = bent)."""
from poses import P
import poses_more  # noqa: F401  natural v0.1 library for the one-off actions
_NATURAL = dict(P)
import poses_x as _X  # noqa: F401,E402  (mutates P: v0.7's exaggerated set; only its keeper poses are used here)

# pilot library: the natural outfield set, v0.7's keeper poses (they read well at this size), then the new cycles below
B = dict(_NATURAL)
B.update({k: v for k, v in P.items() if k.startswith("gk_")})

def mirror(p):
    """left <-> right of a pose dict"""
    q = {}
    for k, v in p.items():
        if k.startswith("r_"): q["l_" + k[2:]] = v
        elif k.startswith("l_"): q["r_" + k[2:]] = v
        elif k in ("side", "twist", "roll"): q[k] = -v
        else: q[k] = v
    return q

def cycle(name, half):
    """half: [(suffix, pose)] for the right-leg half; the left half is mirrored"""
    for suf, p in half: B[f"{name}_{suf}R"] = p; B[f"{name}_{suf}L"] = mirror(p)
    return [f"{name}_{s}R" for s, _ in half] + [f"{name}_{s}L" for s, _ in half]

# run: 8 frames (contact, loading, push-off, flight) per two steps; arms swing opposite the legs
RUN = cycle("run", [
    ("contact", dict(lean=10, r_hip=32, r_knee=12, l_hip=-22, l_knee=72, r_sh=-32, r_el=88, l_sh=34, l_el=88)),
    ("down",    dict(lean=11, drop=0.05, r_hip=10, r_knee=38, l_hip=18, l_knee=105, r_sh=-10, r_el=88, l_sh=12, l_el=88)),
    ("pass",    dict(lean=11, r_hip=-16, r_knee=16, l_hip=40, l_knee=88, r_sh=20, r_el=88, l_sh=-20, l_el=88)),
    ("up",      dict(lean=10, rise=0.04, r_hip=-30, r_knee=55, l_hip=50, l_knee=42, r_sh=34, r_el=88, l_sh=-34, l_el=88)),
])
# sprint: 6 frames, longer reach, more lean, bigger arms
SPRINT = cycle("sprint", [
    ("contact", dict(lean=16, r_hip=42, r_knee=14, l_hip=-30, l_knee=95, r_sh=-45, r_el=90, l_sh=50, l_el=90)),
    ("down",    dict(lean=17, drop=0.06, r_hip=14, r_knee=42, l_hip=30, l_knee=120, r_sh=-12, r_el=90, l_sh=15, l_el=90)),
    ("up",      dict(lean=16, rise=0.05, r_hip=-34, r_knee=62, l_hip=66, l_knee=58, r_sh=48, r_el=90, l_sh=-48, l_el=90)),
])
# dribble: 6 frames, upright, short steps, arms out a little for balance; the touch is on the R/L "touch" frame
DRIBBLE = cycle("dribble", [
    ("touch", dict(lean=6, r_hip=24, r_knee=14, r_abd=6, l_hip=-14, l_knee=46, r_sh=-16, r_shabd=22, r_el=60, l_sh=18, l_shabd=22, l_el=60, head=10)),
    ("mid",   dict(lean=7, drop=0.04, r_hip=6, r_knee=30, l_hip=12, l_knee=72, r_sh=-4, r_shabd=22, r_el=60, l_sh=6, l_shabd=22, l_el=60, head=10)),
    ("push",  dict(lean=7, r_hip=-14, r_knee=20, l_hip=30, l_knee=52, r_sh=12, r_shabd=22, r_el=60, l_sh=-12, l_shabd=22, l_el=60, head=10)),
])
# walk: 4 frames
WALK = cycle("walk", [
    ("contact", dict(r_hip=20, r_knee=5, l_hip=-16, l_knee=12, r_sh=-14, r_el=20, l_sh=14, l_el=20)),
    ("pass",    dict(drop=0.02, r_hip=2, r_knee=12, l_hip=12, l_knee=48, r_sh=0, r_el=20, l_sh=0, l_el=20)),
])
# slide tackle: drop, slide (one leg out, sitting back), sit, up (ISS: 4 poses, 0.73 s)
B["slide_drop"] = dict(lean=-8, drop=0.22, r_hip=55, r_knee=10, l_hip=5, l_knee=80, r_sh=-10, r_shabd=40, l_sh=-25, l_shabd=35)
B["slide"]      = dict(lean=-30, drop=0.42, r_hip=80, r_knee=4, l_hip=25, l_knee=100, r_sh=-15, r_shabd=50, l_sh=-40, l_shabd=35, l_el=30)
B["sit_ground"] = dict(lean=-18, drop=0.48, r_hip=85, r_knee=6, l_hip=70, l_knee=95, r_sh=-30, r_shabd=30, l_sh=-35, l_shabd=30, r_el=20, l_el=20)

# generation jobs: one animation (or a few related stills) per job, in playing order
OUT_JOBS = [
    ("run", RUN + ["idle_breathe"], "football player running cycle, natural stride, no ball"),
    ("sprint_walk", SPRINT + WALK, "football player sprinting cycle then walking cycle, no ball"),
    ("dribble", DRIBBLE + ["turn_plant", "shield"], "football player dribbling with short steps, no ball"),
    ("tackle", ["tackle_stand", "slide_drop", "slide", "sit_ground", "getup_kneel", "block", "stumble", "fall_ground"],
     "football player slide tackle sequence then falling, no ball"),
    ("kick", ["pass_back", "pass_strike", "shot_plant", "shot_back", "shot_strike", "shot_follow", "volley_strike",
              "header_stand", "header", "chest_control"], "football player kicking and heading, no ball"),
    ("misc", ["celebrate_arms_up", "celebrate_fist", "celebrate_point", "celebrate_knee_slide", "hands_on_head",
              "throwin_back", "throwin_release", "header_dive"], "football player celebrations and throw-in, no ball"),
]
KEEP_JOBS = [
    ("gk_stand", ["gk_ready", "gk_shuffleR", "gk_shuffleL", "gk_catch_chest", "gk_hold_ball", "gk_catch_high", "gk_punch",
                  "gk_scoop", "idle_breathe", "getup_kneel"], "goalkeeper set position and catches, no ball"),
    ("gk_dist", ["gk_throw_back", "gk_throw_release", "gk_punt_drop", "gk_punt_strike", "gk_dive_low_R1", "gk_dive_low_L1",
                 "gk_dive_high_R1", "gk_dive_high_L1", "gk_stretch_R", "gk_stretch_L"], "goalkeeper throwing, kicking and diving, no ball"),
    ("gk_move", RUN[:3] + RUN[4:7] + WALK + ["celebrate_arms_up", "hands_on_head"], "goalkeeper running and walking, no ball"),
]

# ---------------------------------------------------------------------------------------------------------------
# v0.9 arcade pilot (owner: animations should be "more dramatic, more arcade"; slides "look really bad"; keepers need
# "better dives"). docs/ANIMATION_PLAN_v0.9.md. Fewer, stronger key poses, pushed past v0.7's exaggerated set.
# The slide is only ever drawn side-on (PFMatch Draw), so it is generated for the east view only; keeper dive
# take-offs are drawn front-on (south) like the owner's flat R08 flight frames.
_F = dict(air=0.001)   # re-ground the figure on its lowest joint (lying and sliding poses)
B["slide_go"]   = dict(lean=-20, drop=0.34, r_hip=65, r_knee=8, l_hip=5, l_knee=75, r_sh=-20, r_shabd=55, l_sh=-35, l_shabd=45, **_F)
B["slide_flat"] = dict(lean=-68, drop=0.70, r_hip=95, r_knee=0, l_hip=35, l_knee=120, r_sh=45, r_shabd=70, r_el=20, l_sh=-75, l_shabd=25, l_el=5, head=30, **_F)
B["slide_hand"] = dict(lean=-48, drop=0.62, r_hip=80, r_knee=25, l_hip=45, l_knee=125, r_sh=10, r_shabd=40, l_sh=-60, l_shabd=20, l_el=5, head=15, **_F)
B["slide_up"]   = dict(lean=32, drop=0.36, r_hip=70, r_knee=95, l_hip=10, l_knee=115, r_sh=40, r_el=40, l_sh=55, l_el=40, **_F)
B["tackle_lunge"] = dict(lean=24, drop=0.22, l_hip=20, l_knee=70, r_hip=72, r_knee=5, r_abd=25, l_sh=-30, l_shabd=60, r_sh=20, r_shabd=55)
B["tackle_poke"]  = dict(lean=14, drop=0.20, l_hip=10, l_knee=60, r_hip=55, r_knee=0, r_abd=55, l_shabd=75, r_shabd=40, twist=-20)
B["shot_a_plant"]  = dict(lean=16, l_hip=35, l_knee=30, r_hip=-50, r_knee=95, l_sh=50, l_shabd=70, r_sh=-40, r_shabd=50, twist=-15)
B["shot_a_back"]   = dict(lean=12, drop=0.06, l_hip=18, l_knee=35, r_hip=-85, r_knee=145, l_sh=45, l_shabd=95, r_sh=-55, r_shabd=65, twist=-30)
B["shot_a_strike"] = dict(lean=-14, drop=0.04, l_hip=8, l_knee=30, r_hip=65, r_knee=10, l_sh=25, l_shabd=95, r_sh=-45, r_shabd=75, twist=20)
B["shot_a_follow"] = dict(lean=-30, rise=0.12, l_hip=-5, l_knee=10, r_hip=130, r_knee=5, l_sh=70, l_shabd=90, r_sh=-70, r_shabd=60, twist=35)
RUNA = cycle("runa", [
    ("contact", dict(lean=18, r_hip=50, r_knee=10, l_hip=-40, l_knee=95, r_sh=-60, r_el=95, l_sh=65, l_el=95)),
    ("down",    dict(lean=19, drop=0.07, r_hip=20, r_knee=45, l_hip=20, l_knee=125, r_sh=-20, r_el=95, l_sh=25, l_el=95)),
    ("pass",    dict(lean=19, r_hip=-25, r_knee=25, l_hip=60, l_knee=105, r_sh=35, r_el=95, l_sh=-35, l_el=95)),
    ("up",      dict(lean=18, rise=0.07, r_hip=-45, r_knee=70, l_hip=75, l_knee=55, r_sh=60, r_el=95, l_sh=-60, l_el=95)),
])
# keeper take-off: the whole body thrown sideways off the near foot, arms already reaching past the head
B["gk_push_R"] = dict(roll=38, air=0.12, lean=6, r_abd=30, l_abd=15, r_knee=20, l_knee=45, l_hip=15, r_sh=170, r_shabd=35, l_sh=165, l_shabd=25, r_el=0, l_el=0, head=-5)
B["gk_push_L"] = mirror(B["gk_push_R"])
B["gk_load_R"] = dict(roll=12, lean=14, drop=0.22, side=0.12, r_abd=30, l_abd=10, r_knee=65, l_knee=40, r_hip=30, l_hip=15, r_sh=90, r_shabd=40, l_sh=90, l_shabd=30, r_el=20, l_el=20)
B["gk_reach_R"] = dict(roll=62, air=0.22, r_abd=20, l_abd=30, l_knee=30, r_sh=175, r_shabd=30, l_sh=175, l_shabd=20, r_el=0, l_el=0, head=-5)
# (name, poses, action, directions) - the 4th entry limits the views generated (default: all five)
ARC_OUT = [
    ("arc_slide", ["slide_go", "slide_flat", "slide_hand", "slide_up"], "football player sliding tackle on the grass, body low and flat, leg stretched out, no ball", ["east"]),
    ("arc_kick", ["shot_a_plant", "shot_a_back", "shot_a_strike", "shot_a_follow", "tackle_lunge", "tackle_poke"], "football player powerful shot with a big backswing, then a lunging tackle, no ball", None),
    ("arc_run", RUNA, "football player running hard, leaning forward, arms pumping, no ball", None),
]
ARC_KEEP = [
    # PixelLab needs 3+ frames a job: load, take-off, reach (to his right); the left dive is the mirror (build_base_atlas)
    ("gk_push", ["gk_load_R", "gk_push_R", "gk_reach_R"], "goalkeeper diving sideways to his right, throwing his body into the air, arms reaching out, no ball", ["south"]),
]
OUT_JOBS = OUT_JOBS + ARC_OUT
KEEP_JOBS = KEEP_JOBS + ARC_KEEP
