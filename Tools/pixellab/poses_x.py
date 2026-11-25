"""v0.7 pose library: the same pose names as poses.py / poses_more.py, pushed much further (ISS Deluxe reads at
a glance because every action is exaggerated: high knees, big arm pumps, huge backswings, full-stretch dives).
Importing this module overwrites the entries in poses.P, so the game's clip names stay the same.

Per-club production (Tools/pixellab/produce_clubs.py) uses SETS_X: the poses the match actually shows."""
from poses import P
import poses_more  # noqa: F401  (fills P with the v0.1 library first; anything not redefined here keeps it)

# ---- locomotion: long strides, high back heel, arms pumping across the body ----
P["run_contactR"] = dict(lean=18, r_hip=55, r_knee=8, l_hip=-45, l_knee=95, r_sh=-65, r_el=95, l_sh=70, l_el=95)
P["run_downR"]    = dict(lean=18, drop=0.08, r_hip=25, r_knee=45, l_hip=-25, l_knee=120, r_sh=-40, r_el=95, l_sh=45, l_el=95)
P["run_passR"]    = dict(lean=18, rise=0.06, r_hip=-30, r_knee=40, l_hip=75, l_knee=115, r_sh=60, r_el=90, l_sh=-60, l_el=90)
P["run_contactL"] = dict(lean=18, l_hip=55, l_knee=8, r_hip=-45, r_knee=95, l_sh=-65, l_el=95, r_sh=70, r_el=95)
P["run_downL"]    = dict(lean=18, drop=0.08, l_hip=25, l_knee=45, r_hip=-25, r_knee=120, l_sh=-40, l_el=95, r_sh=45, r_el=95)
P["run_passL"]    = dict(lean=18, rise=0.06, l_hip=-30, l_knee=40, r_hip=75, r_knee=115, l_sh=60, l_el=90, r_sh=-60, r_el=90)
P["sprint_contactR"] = dict(lean=28, r_hip=70, r_knee=15, l_hip=-55, l_knee=110, r_sh=-80, r_el=100, l_sh=85, l_el=95)
P["sprint_passR"]  = dict(lean=28, rise=0.09, r_hip=-40, r_knee=55, l_hip=90, l_knee=125, r_sh=75, r_el=95, l_sh=-75, l_el=95)
P["sprint_contactL"] = dict(lean=28, l_hip=70, l_knee=15, r_hip=-55, r_knee=110, l_sh=-80, l_el=100, r_sh=85, r_el=95)
P["sprint_passL"]  = dict(lean=28, rise=0.09, l_hip=-40, l_knee=55, r_hip=90, r_knee=125, l_sh=75, l_el=95, r_sh=-75, r_el=95)
P["jog_contactR"]  = dict(lean=10, r_hip=35, r_knee=10, l_hip=-25, l_knee=65, r_sh=-40, r_el=85, l_sh=40, l_el=85)
P["jog_contactL"]  = dict(lean=10, l_hip=35, l_knee=10, r_hip=-25, r_knee=65, l_sh=-40, l_el=85, r_sh=40, r_el=85)
P["walk_contactR"] = dict(lean=3, r_hip=25, r_knee=5, l_hip=-18, l_knee=20, r_sh=-20, l_sh=20, r_el=20, l_el=20)
P["walk_contactL"] = dict(lean=3, l_hip=25, l_knee=5, r_hip=-18, r_knee=20, l_sh=-20, r_sh=20, r_el=20, l_el=20)
# dribble: bent over the ball, the touching foot reaching out in front
P["dribble_touchR"] = dict(lean=22, drop=0.07, r_hip=50, r_knee=30, l_hip=-25, l_knee=70, r_sh=-35, r_el=60, l_sh=40, l_el=60, r_shabd=30, l_shabd=30)
P["dribble_touchL"] = dict(lean=22, drop=0.07, l_hip=50, l_knee=30, r_hip=-25, r_knee=70, l_sh=-35, l_el=60, r_sh=40, r_el=60, r_shabd=30, l_shabd=30)
P["turn_plant"]    = dict(lean=12, drop=0.10, side=0.16, r_abd=40, r_knee=45, l_knee=30, l_abd=-8, r_shabd=70, l_shabd=35, twist=-40)

# ---- kicks: the backswing heel near the backside, the follow-through foot above the waist, arms flung wide ----
P["shot_plant"]   = dict(lean=12, l_hip=30, l_knee=30, r_hip=-45, r_knee=85, l_sh=45, l_shabd=60, r_sh=-30, r_shabd=45, twist=-12)
P["shot_back"]    = dict(lean=6, drop=0.06, l_hip=15, l_knee=35, r_hip=-75, r_knee=135, l_sh=35, l_shabd=85, r_sh=-45, r_shabd=60, twist=-25)
P["shot_strike"]  = dict(lean=-10, drop=0.04, l_hip=8, l_knee=30, r_hip=55, r_knee=15, l_sh=20, l_shabd=90, r_sh=-35, r_shabd=70, twist=15)
P["shot_follow"]  = dict(lean=-22, rise=0.08, l_hip=0, l_knee=10, r_hip=115, r_knee=5, l_sh=60, l_shabd=80, r_sh=-60, r_shabd=55, twist=30)
P["pass_back"]    = dict(lean=8, l_hip=12, l_knee=25, r_hip=-40, r_knee=60, r_abd=30, l_shabd=45, r_shabd=35, twist=-10)
P["pass_strike"]  = dict(lean=0, l_hip=8, l_knee=25, r_hip=40, r_knee=10, r_abd=40, l_shabd=45, r_shabd=35, twist=15)
P["volley_strike"] = dict(lean=-35, drop=0.02, l_knee=25, r_hip=105, r_knee=10, r_abd=40, l_shabd=90, r_shabd=70, twist=25)
P["chest_control"] = dict(lean=-28, drop=0.06, r_knee=30, l_knee=30, r_shabd=75, l_shabd=75, r_sh=15, l_sh=15, head=-15)
# challenges
P["slide"]        = dict(lean=-40, drop=0.48, r_hip=85, r_knee=0, l_hip=25, l_knee=110, r_sh=-25, r_shabd=70, l_sh=-60, l_shabd=45, l_el=30)
P["tackle_stand"] = dict(lean=16, drop=0.12, l_hip=15, l_knee=45, r_hip=60, r_knee=5, r_abd=35, l_shabd=65, r_shabd=55)
P["block"]        = dict(lean=-8, drop=0.10, r_abd=45, r_knee=15, l_knee=35, r_sh=-15, l_sh=-15, r_shabd=10, l_shabd=10)
P["shield"]       = dict(lean=8, drop=0.12, r_abd=25, l_abd=25, r_knee=35, l_knee=35, r_shabd=75, l_shabd=75, r_el=45, l_el=45)
# headers
P["header"]       = dict(lean=-14, rise=0.45, r_hip=35, r_knee=95, l_hip=10, l_knee=60, r_sh=40, r_shabd=90, l_sh=40, l_shabd=90, head=35)
P["header_stand"] = dict(lean=-22, r_knee=20, l_knee=20, r_shabd=60, l_shabd=60, head=30)
# throw-in
P["throwin_back"]  = dict(lean=-25, r_hip=-10, l_hip=25, l_knee=25, r_sh=175, r_el=80, l_sh=175, l_el=80)
P["throwin_release"] = dict(lean=22, r_hip=-20, r_knee=20, l_hip=30, l_knee=20, r_sh=115, r_el=5, l_sh=115, l_el=5)
# reactions
P["stumble"]      = dict(lean=45, drop=0.14, r_hip=50, r_knee=40, l_hip=-40, l_knee=65, r_sh=80, r_el=20, l_sh=-40, l_shabd=60)
P["celebrate_arms_up"] = dict(lean=-12, rise=0.08, r_sh=175, l_sh=175, r_shabd=30, l_shabd=30, r_el=5, l_el=5, head=-20)
P["celebrate_fist"] = dict(lean=-8, rise=0.04, r_knee=15, l_knee=15, r_sh=170, r_el=35, l_sh=-20, l_el=100, head=-15)
P["celebrate_airplane"] = dict(lean=15, r_hip=30, l_hip=-15, l_knee=40, r_shabd=95, l_shabd=95, r_el=0, l_el=0, twist=-25)

# ---- goalkeepers: a crouched, wide ready stance and full-stretch dives ----
_GKA = dict(r_sh=55, l_sh=55, r_shabd=50, l_shabd=50, r_el=55, l_el=55)
P["gk_ready"]      = dict(lean=18, drop=0.16, r_abd=20, l_abd=20, r_knee=50, l_knee=50, r_hip=25, l_hip=25, **_GKA)
P["gk_shuffleR"]   = dict(lean=18, drop=0.16, side=0.12, r_abd=38, l_abd=6, r_knee=45, l_knee=55, r_hip=22, l_hip=22, **_GKA)
P["gk_shuffleL"]   = dict(lean=18, drop=0.16, side=-0.12, l_abd=38, r_abd=6, l_knee=45, r_knee=55, r_hip=22, l_hip=22, **_GKA)
# the dives are posed UPRIGHT at full stretch (arms over the head, leaning toward the dive, the far leg kicked out);
# the pipeline rotates them onto their side (rotsprite) so the body flies flat, the way ISS draws it. PixelLab
# draws an upright figure far more reliably than a horizontal one.
P["gk_dive_low_R1"]  = dict(lean=10, drop=0.10, side=0.10, r_abd=35, l_abd=10, r_knee=35, l_knee=15, r_sh=150, r_shabd=40, l_sh=150, l_shabd=30, r_el=0, l_el=0, head=5)
P["gk_dive_high_R1"] = dict(lean=-5, r_abd=25, l_abd=30, l_knee=60, l_hip=20, r_sh=175, r_shabd=20, l_sh=175, l_shabd=15, r_el=0, l_el=0, head=-15)
P["gk_dive_low_L1"]  = dict(lean=10, drop=0.10, side=-0.10, l_abd=35, r_abd=10, l_knee=35, r_knee=15, l_sh=150, l_shabd=40, r_sh=150, r_shabd=30, r_el=0, l_el=0, head=5)
P["gk_dive_high_L1"] = dict(lean=-5, l_abd=25, r_abd=30, r_knee=60, r_hip=20, l_sh=175, l_shabd=20, r_sh=175, r_shabd=15, r_el=0, l_el=0, head=-15)
P["gk_stretch_R"]    = dict(r_abd=20, l_abd=35, r_sh=175, r_shabd=35, l_sh=175, l_shabd=25, r_el=0, l_el=0, head=-5)   # rotated 90 deg: flat out
P["gk_stretch_L"]    = dict(l_abd=20, r_abd=35, l_sh=175, l_shabd=35, r_sh=175, r_shabd=25, r_el=0, l_el=0, head=-5)
P["gk_catch_chest"] = dict(lean=20, drop=0.08, r_knee=30, l_knee=30, r_sh=50, l_sh=50, r_el=120, l_el=120, r_shabd=12, l_shabd=12)
P["gk_catch_high"] = dict(lean=-14, rise=0.25, r_hip=35, r_knee=80, r_sh=178, l_sh=178, r_el=8, l_el=8, head=-30)
P["gk_scoop"]      = dict(lean=55, drop=0.22, r_knee=70, l_knee=40, r_hip=40, l_hip=20, r_sh=80, l_sh=80, r_el=20, l_el=20, r_shabd=8, l_shabd=8, head=10)
P["gk_punch"]      = dict(lean=-12, rise=0.35, r_hip=45, r_knee=95, r_sh=175, r_el=0, l_sh=70, l_el=70, head=-25)
P["gk_hold_ball"]  = dict(lean=-3, r_sh=40, l_sh=40, r_el=110, l_el=110, r_shabd=10, l_shabd=10)
P["gk_throw_back"] = dict(lean=-18, r_hip=-15, l_hip=30, l_knee=20, r_sh=170, r_el=40, r_shabd=40, l_sh=80, l_el=20, twist=-20)
P["gk_throw_release"] = dict(lean=28, r_hip=-30, r_knee=30, l_hip=35, l_knee=20, r_sh=80, r_el=0, l_sh=-30, twist=20)
P["gk_punt_drop"]  = dict(lean=-8, r_hip=-45, r_knee=70, l_knee=15, r_sh=60, r_el=30, l_sh=60, l_el=30)
P["gk_punt_strike"] = dict(lean=-35, rise=0.08, r_hip=120, r_knee=0, l_knee=10, r_shabd=60, l_shabd=75, l_sh=35)

# what per-club production generates (15 poses per PixelLab job): everything the match draws for a player
XO1 = ["run_contactR", "run_downR", "run_passR", "run_contactL", "run_downL", "run_passL",
       "shot_plant", "shot_back", "shot_strike", "shot_follow", "pass_back", "pass_strike",
       "slide", "tackle_stand", "header"]
XO2 = ["idle_breathe", "sprint_contactR", "sprint_passR", "sprint_contactL", "sprint_passL", "jog_contactR", "jog_contactL",
       "dribble_touchR", "dribble_touchL", "turn_plant", "chest_control", "volley_strike", "throwin_back", "throwin_release", "header_stand"]
XO3 = ["stumble", "fall_ground", "getup_kneel", "celebrate_arms_up", "celebrate_fist", "celebrate_knee_slide", "celebrate_airplane",
       "block", "walk_contactR", "walk_contactL", "hands_on_head", "shield", "header_dive", "sit_ground", "celebrate_point"]
XG1 = ["gk_ready", "gk_shuffleR", "gk_shuffleL", "gk_dive_low_R1", "gk_dive_high_R1", "gk_dive_low_L1", "gk_dive_high_L1",
       "gk_stretch_R", "gk_stretch_L", "gk_catch_chest", "gk_catch_high", "gk_scoop", "gk_punch", "gk_hold_ball", "getup_kneel"]
XG2 = ["gk_throw_back", "gk_throw_release", "gk_punt_drop", "gk_punt_strike", "run_contactR", "run_passR",
       "run_contactL", "run_passL", "pass_back", "pass_strike", "celebrate_arms_up", "hands_on_head", "walk_contactR", "walk_contactL", "idle_breathe"]
SETS_X = {"outfield": [XO1, XO2, XO3], "keeper": [XG1, XG2]}
