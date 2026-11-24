"""Pose sets beyond batch 1: locomotion variety, falls, celebrations, goalkeepers, officials, coaches.
Imported by poses.py (adds to poses.P). Angles in degrees, distances in metres."""
from poses import P, BATCH1

# ---- batch 2: locomotion variety + ball skills ----
P["idle_breathe"]  = dict(drop=0.015, r_sh=3, l_sh=3, r_el=15, l_el=15)
P["sprint_contactR"] = dict(lean=22, r_hip=50, r_knee=20, l_hip=-35, l_knee=75, r_sh=-50, r_el=90, l_sh=55, l_el=90)
P["sprint_passR"]  = dict(lean=22, rise=0.05, r_hip=-25, r_knee=30, l_hip=65, l_knee=110, r_sh=45, r_el=90, l_sh=-45, l_el=90)
P["sprint_contactL"] = dict(lean=22, l_hip=50, l_knee=20, r_hip=-35, r_knee=75, l_sh=-50, l_el=90, r_sh=55, r_el=90)
P["sprint_passL"]  = dict(lean=22, rise=0.05, l_hip=-25, l_knee=30, r_hip=65, r_knee=110, l_sh=45, l_el=90, r_sh=-45, r_el=90)
P["jog_contactR"]  = dict(lean=6, r_hip=22, r_knee=12, l_hip=-15, l_knee=40, r_sh=-20, r_el=70, l_sh=22, l_el=70)
P["jog_contactL"]  = dict(lean=6, l_hip=22, l_knee=12, r_hip=-15, r_knee=40, l_sh=-20, l_el=70, r_sh=22, r_el=70)
P["dribble_touchR"] = dict(lean=10, drop=0.03, r_hip=30, r_knee=35, l_hip=-10, l_knee=30, r_shabd=25, l_shabd=25)
P["dribble_touchL"] = dict(lean=10, drop=0.03, l_hip=30, l_knee=35, r_hip=-10, r_knee=30, r_shabd=25, l_shabd=25)
P["turn_plant"]    = dict(lean=6, drop=0.06, side=0.10, r_abd=25, r_knee=30, l_knee=20, l_abd=-5, r_shabd=40, l_shabd=20, twist=-25)
P["chest_control"] = dict(lean=-18, drop=0.04, r_knee=20, l_knee=20, r_shabd=55, l_shabd=55, r_sh=10, l_sh=10, head=-10)
P["volley_strike"] = dict(lean=-25, drop=0.02, l_knee=20, r_hip=85, r_knee=15, r_abd=30, l_shabd=70, r_shabd=50, twist=15)
P["throwin_back"]  = dict(lean=-15, r_hip=-5, l_hip=15, l_knee=20, r_sh=170, r_el=60, l_sh=170, l_el=60)
P["throwin_release"] = dict(lean=15, r_hip=-15, r_knee=15, l_hip=20, l_knee=15, r_sh=110, r_el=10, l_sh=110, l_el=10)
P["shield"]        = dict(lean=4, drop=0.08, r_abd=18, l_abd=18, r_knee=25, l_knee=25, r_shabd=50, l_shabd=50, r_el=40, l_el=40)
BATCH2 = ["idle_breathe", "sprint_contactR", "sprint_passR", "sprint_contactL", "sprint_passL", "jog_contactR", "jog_contactL",
          "dribble_touchR", "dribble_touchL", "turn_plant", "chest_control", "volley_strike", "throwin_back", "throwin_release", "shield"]

# ---- batch 3: falls, celebrations, emotions ----
P["stumble"]       = dict(lean=35, drop=0.10, r_hip=40, r_knee=30, l_hip=-30, l_knee=50, r_sh=60, r_el=20, l_sh=-30, l_shabd=40)
P["fall_ground"]   = dict(pitch=80, r_sh=150, l_sh=150, r_el=20, l_el=20, r_knee=10, l_knee=25)
P["sit_ground"]    = dict(drop=0.80, lean=-20, r_hip=85, l_hip=85, r_knee=40, l_knee=60, r_sh=-40, l_sh=-40, r_shabd=20, l_shabd=20)
P["getup_kneel"]   = dict(drop=0.50, lean=15, r_hip=90, r_knee=90, l_hip=-10, l_knee=100, r_sh=20, l_sh=20)
P["celebrate_arms_up"] = dict(lean=-8, rise=0.05, r_sh=170, l_sh=170, r_shabd=20, l_shabd=20, r_el=5, l_el=5, head=-15)
P["celebrate_fist"] = dict(lean=-5, r_knee=10, l_knee=10, r_sh=160, r_el=40, l_sh=-10, l_el=90, head=-10)
P["celebrate_knee_slide"] = dict(drop=0.48, lean=-25, r_hip=85, r_knee=110, l_hip=-5, l_knee=100, r_shabd=80, l_shabd=80, r_el=10, l_el=10, head=-25)
P["celebrate_point"] = dict(lean=-3, r_sh=100, r_el=0, r_shabd=10, l_shabd=10, twist=10)
P["celebrate_airplane"] = dict(lean=10, r_hip=20, l_hip=-10, l_knee=30, r_shabd=90, l_shabd=90, r_el=0, l_el=0, twist=-15)
P["header_stand"]  = dict(lean=-15, r_knee=10, l_knee=10, r_shabd=40, l_shabd=40, head=20)
P["header_dive"]   = dict(pitch=70, air=0.40, r_sh=40, l_sh=40, r_shabd=50, l_shabd=50, r_knee=30, l_knee=40)
P["block"]         = dict(lean=-5, drop=0.06, r_abd=30, r_knee=10, l_knee=25, r_sh=-10, l_sh=-10, r_shabd=5, l_shabd=5)
P["walk_contactR"] = dict(lean=2, r_hip=18, r_knee=5, l_hip=-12, l_knee=15, r_sh=-12, l_sh=12, r_el=15, l_el=15)
P["walk_contactL"] = dict(lean=2, l_hip=18, l_knee=5, r_hip=-12, r_knee=15, l_sh=-12, r_sh=12, r_el=15, l_el=15)
P["hands_on_head"] = dict(lean=-5, r_sh=150, r_el=140, r_shabd=60, l_sh=150, l_el=140, l_shabd=60, head=-20)
BATCH3 = ["stumble", "fall_ground", "sit_ground", "getup_kneel", "celebrate_arms_up", "celebrate_fist", "celebrate_knee_slide",
          "celebrate_point", "celebrate_airplane", "header_stand", "header_dive", "block", "walk_contactR", "walk_contactL", "hands_on_head"]

# ---- goalkeeper ----
_GKA = dict(r_sh=40, l_sh=40, r_shabd=35, l_shabd=35, r_el=50, l_el=50)
P["gk_ready"]      = dict(lean=12, drop=0.10, r_abd=14, l_abd=14, r_knee=35, l_knee=35, r_hip=15, l_hip=15, **_GKA)
P["gk_shuffleR"]   = dict(lean=12, drop=0.10, side=0.08, r_abd=28, l_abd=4, r_knee=30, l_knee=40, r_hip=12, l_hip=12, **_GKA)
P["gk_shuffleL"]   = dict(lean=12, drop=0.10, side=-0.08, l_abd=28, r_abd=4, l_knee=30, r_knee=40, r_hip=12, l_hip=12, **_GKA)
P["gk_dive_low_R1"]  = dict(roll=45, r_abd=30, l_knee=40, r_sh=90, r_shabd=100, l_sh=90, l_shabd=60, r_el=0, l_el=10)
P["gk_dive_low_R2"]  = dict(roll=85, r_abd=10, l_knee=30, r_sh=90, r_shabd=150, l_sh=90, l_shabd=130, r_el=0, l_el=0)
P["gk_dive_high_R1"] = dict(roll=40, air=0.35, r_abd=20, l_abd=10, l_knee=50, r_shabd=160, l_shabd=150, r_el=0, l_el=0)
P["gk_dive_high_R2"] = dict(roll=75, air=0.55, r_abd=10, l_knee=40, r_shabd=170, l_shabd=165, r_el=0, l_el=0)
P["gk_dive_low_L1"]  = dict(roll=-45, l_abd=30, r_knee=40, l_sh=90, l_shabd=100, r_sh=90, r_shabd=60, r_el=10, l_el=0)
P["gk_dive_low_L2"]  = dict(roll=-85, l_abd=10, r_knee=30, l_sh=90, l_shabd=150, r_sh=90, r_shabd=130, r_el=0, l_el=0)
P["gk_dive_high_L1"] = dict(roll=-40, air=0.35, l_abd=20, r_abd=10, r_knee=50, l_shabd=160, r_shabd=150, r_el=0, l_el=0)
P["gk_dive_high_L2"] = dict(roll=-75, air=0.55, l_abd=10, r_knee=40, l_shabd=170, r_shabd=165, r_el=0, l_el=0)
P["gk_catch_chest"] = dict(lean=15, drop=0.05, r_knee=20, l_knee=20, r_sh=40, l_sh=40, r_el=110, l_el=110, r_shabd=10, l_shabd=10)
P["gk_catch_high"] = dict(lean=-10, rise=0.15, r_hip=20, r_knee=50, r_sh=170, l_sh=170, r_el=10, l_el=10, head=-25)
P["gk_punch"]      = dict(lean=-8, rise=0.25, r_hip=30, r_knee=70, r_sh=160, r_el=0, l_sh=60, l_el=60, head=-20)
P["gk_lying_ball"] = dict(roll=88, r_sh=80, l_sh=80, r_el=100, l_el=100, r_knee=60, l_knee=60, r_hip=40, l_hip=40)
GK1 = ["gk_ready", "gk_shuffleR", "gk_shuffleL", "gk_dive_low_R1", "gk_dive_low_R2", "gk_dive_high_R1", "gk_dive_high_R2",
       "gk_dive_low_L1", "gk_dive_low_L2", "gk_dive_high_L1", "gk_dive_high_L2", "gk_catch_chest", "gk_catch_high", "gk_punch", "gk_lying_ball"]
P["gk_throw_back"] = dict(lean=-10, r_hip=-10, l_hip=20, l_knee=15, r_sh=160, r_el=30, r_shabd=30, l_sh=60, l_el=20)
P["gk_throw_release"] = dict(lean=20, r_hip=-20, r_knee=20, l_hip=25, l_knee=15, r_sh=70, r_el=0, l_sh=-20)
P["gk_punt_drop"]  = dict(lean=-5, r_hip=-30, r_knee=50, l_knee=10, r_sh=50, r_el=30, l_sh=50, l_el=30)
P["gk_punt_strike"] = dict(lean=-25, rise=0.05, r_hip=95, r_knee=5, l_knee=10, r_shabd=50, l_shabd=60, l_sh=30)
P["gk_hold_ball"]  = dict(r_sh=35, l_sh=35, r_el=100, l_el=100, r_shabd=8, l_shabd=8)
GK2 = ["gk_throw_back", "gk_throw_release", "gk_punt_drop", "gk_punt_strike", "gk_hold_ball", "run_contactR", "run_passR",
       "run_contactL", "run_passL", "pass_back", "pass_strike", "celebrate_arms_up", "getup_kneel", "hands_on_head", "walk_contactR"]

# ---- officials ----
P["ref_whistle"]   = dict(r_sh=80, r_el=130, r_shabd=10, head=-5)
P["ref_point"]     = dict(r_sh=90, r_el=0, r_shabd=40, twist=10)
P["ref_card"]      = dict(lean=-4, r_sh=175, r_el=0, r_shabd=5, l_sh=10)
P["ref_advantage"] = dict(r_sh=80, l_sh=80, r_el=0, l_el=0, r_shabd=10, l_shabd=10)
P["lino_flag_up"]  = dict(r_sh=175, r_el=0, r_shabd=5)
P["lino_flag_side"] = dict(r_sh=90, r_el=0, r_shabd=60)
REF = ["walk_contactR", "walk_contactL", "jog_contactR", "jog_contactL", "run_contactR", "run_passR", "run_contactL", "run_passL",
       "ref_whistle", "ref_point", "ref_card", "ref_advantage", "lino_flag_up", "lino_flag_side", "idle_breathe"]

# ---- managers / coaches ----
P["coach_shout"]   = dict(lean=10, r_sh=60, r_el=60, l_sh=60, l_el=100, r_shabd=40, l_shabd=30, head=-5)
P["coach_arms_folded"] = dict(r_sh=40, l_sh=40, r_el=130, l_el=130, r_shabd=-10, l_shabd=-10)
P["coach_frustrated"] = dict(lean=-5, r_sh=150, r_el=140, r_shabd=60, l_sh=150, l_el=140, l_shabd=60, head=-20)
P["coach_point_forward"] = dict(lean=6, r_sh=90, r_el=5, r_shabd=10)
P["coach_celebrate"] = dict(rise=0.08, r_sh=170, l_sh=170, r_el=10, l_el=10, r_shabd=20, l_shabd=20)
COACH = ["idle_breathe", "walk_contactR", "walk_contactL", "coach_shout", "coach_arms_folded", "coach_frustrated",
         "coach_point_forward", "coach_celebrate", "celebrate_fist", "hands_on_head", "ref_whistle", "ref_advantage", "block", "sit_ground", "jog_contactR"]

SETS = {"outfield": [BATCH1, BATCH2, BATCH3], "keeper": [GK1, GK2], "official": [REF], "coach": [COACH]}
GEN_DIRS = ["south", "south-east", "east", "north-east", "north"]                  # generated by PixelLab
MIRROR = {"south-west": "south-east", "west": "east", "north-west": "north-east"}   # horizontally flipped in code
