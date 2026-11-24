"""Pose library: football poses defined as joint angles on a 3D stick figure (metres),
projected into each of the 8 PixelLab directions as 2D keypoint DELTAS added to the
skeleton PixelLab estimates on that direction's idle frame. Each pose is one STATIC still;
the game builds animation from these stills in code."""
import math

TILT = math.radians(28)          # 'low top-down' camera pitch (fitted visually)
DIR_ANGLE = {"south": 0, "south-east": 45, "east": 90, "north-east": 135,
             "north": 180, "north-west": 225, "west": 270, "south-west": 315}
L = dict(thigh=0.44, shin=0.44, uarm=0.29, farm=0.27, sh=0.19, hip=0.10, spine=0.52, neck=0.10)
HEIGHT_FRAC = 0.62               # fraction of canvas a 1.8 m player occupies (64 px figure / 96 canvas, approx.)

def v_add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def v_mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)

def rot_x(v, a):  # pitch about character's right axis (forward flex positive -> toward +f)
    x, y, z = v; c, s = math.cos(a), math.sin(a)
    return (x, y * c - z * s, y * s + z * c)
def rot_z(v, a):  # roll about forward axis (abduction)
    x, y, z = v; c, s = math.cos(a), math.sin(a)
    return (x * c - y * s, x * s + y * c, z)
def rot_y(v, a):  # yaw about up
    x, y, z = v; c, s = math.cos(a), math.sin(a)
    return (x * c + z * s, y, -x * s + z * c)

NEUTRAL = dict(lean=0, twist=0, drop=0, side=0, rise=0,
               r_hip=0, r_knee=0, r_abd=0, l_hip=0, l_knee=0, l_abd=0,
               r_sh=0, r_el=10, r_shabd=8, l_sh=0, l_el=10, l_shabd=8, head=0)

def fk(p):
    """Local frame: x = character's right, y = up, z = forward. Returns joint dict (metres)."""
    q = dict(NEUTRAL); q.update(p); r = math.radians
    hipc = (q["side"], L["thigh"] + L["shin"] - q["drop"] + q["rise"], 0.0)
    J = {}
    def leg(sign, hipf, knee, abd, pre):
        h = v_add(hipc, rot_y((sign * L["hip"], 0, 0), r(q["twist"]) * 0.4))
        thigh = rot_z(rot_x((0, -L["thigh"], 0), -r(hipf)), sign * r(abd))
        k = v_add(h, thigh)
        shin = rot_z(rot_x((0, -L["shin"], 0), -r(hipf) + r(knee)), sign * r(abd))
        a = v_add(k, shin)
        J[pre + "HIP"], J[pre + "KNEE"], J[pre + "LEG"] = h, k, a
    leg(+1, q["r_hip"], q["r_knee"], q["r_abd"], "RIGHT ")
    leg(-1, q["l_hip"], q["l_knee"], q["l_abd"], "LEFT ")
    spine = rot_y(rot_x((0, L["spine"], 0), r(q["lean"])), r(q["twist"]))
    neck = v_add(hipc, spine)
    J["NECK"] = neck
    head = v_add(neck, rot_x((0, 0.16, 0.03), r(q["lean"] + q["head"])))
    for name, off in [("NOSE", (0, 0, 0.09)), ("RIGHT EYE", (0.03, 0.03, 0.08)), ("LEFT EYE", (-0.03, 0.03, 0.08)),
                      ("RIGHT EAR", (0.08, 0.01, 0)), ("LEFT EAR", (-0.08, 0.01, 0))]:
        J[name] = v_add(head, rot_y(off, r(q["twist"])))
    def arm(sign, sh, el, abd, pre):
        s = v_add(neck, rot_y(rot_x((sign * L["sh"], -0.03, 0), r(q["lean"])), r(q["twist"])))
        ua = rot_y(rot_z(rot_x((0, -L["uarm"], 0), -r(sh)), sign * r(abd)), r(q["twist"]))
        e = v_add(s, ua)
        fa = rot_y(rot_z(rot_x((0, -L["farm"], 0), -r(sh) - r(el)), sign * r(abd)), r(q["twist"]))
        w = v_add(e, fa)
        J[pre + "SHOULDER"], J[pre + "ELBOW"], J[pre + "ARM"] = s, e, w
    arm(+1, q["r_sh"], q["r_el"], q["r_shabd"], "RIGHT ")
    arm(-1, q["l_sh"], q["l_el"], q["l_shabd"], "LEFT ")
    return J

def body_roll(J, p):
    """Whole-body roll (deg, + = toward the character's right; dives) and pitch (deg, + = face-down; falls),
    then re-grounded so the lowest joint touches the floor, plus optional `air` lift (m)."""
    rl, pt = math.radians(p.get("roll", 0)), math.radians(p.get("pitch", 0))
    if not rl and not pt and not p.get("air"): return J
    out = {}
    for k, (x, y, z) in J.items():
        if pt: y, z = y * math.cos(pt) - z * math.sin(pt), y * math.sin(pt) + z * math.cos(pt)
        if rl: x, y = x * math.cos(rl) + y * math.sin(rl), -x * math.sin(rl) + y * math.cos(rl)
        out[k] = (x, y, z)
    lift = -min(v[1] for v in out.values()) + p.get("air", 0)
    return {k: (v[0], v[1] + lift, v[2]) for k, v in out.items()}

def project(j, direction):
    """local joint -> screen (x right, y down) in metres, for a character facing `direction`."""
    a = math.radians(DIR_ANGLE[direction])
    f = (math.sin(a), 0, -math.cos(a)); rgt = (-math.cos(a), 0, -math.sin(a))
    x, y, z = j
    wx = rgt[0] * x + f[0] * z; wz = rgt[2] * x + f[2] * z
    return (wx, -y * math.cos(TILT) - wz * math.sin(TILT))

def cam_depth(j, direction):
    """how near the camera a joint is (bigger = nearer), used as PixelLab z_index."""
    a = math.radians(DIR_ANGLE[direction])
    f = (math.sin(a), 0, -math.cos(a)); rgt = (-math.cos(a), 0, -math.sin(a))
    x, y, z = j
    wz = rgt[2] * x + f[2] * z
    return -wz * math.cos(TILT) + y * math.sin(TILT)

def keypoints_for(pose, direction, rest_kps):
    """rest_kps: PixelLab estimate on this direction's idle. Returns keypoints for `pose`."""
    J0, J1 = fk({}), body_roll(fk(pose), pose)
    s = HEIGHT_FRAC / 1.8
    out = []
    for k in rest_kps:
        lab = k["label"]
        if lab not in J0: out.append(dict(k)); continue
        p0, p1 = project(J0[lab], direction), project(J1[lab], direction)
        nk = dict(k)
        nk["x"] = min(1, max(0, k["x"] + (p1[0] - p0[0]) * s))
        nk["y"] = min(1, max(0, k["y"] + (p1[1] - p0[1]) * s))
        nk["z_index"] = int(round(cam_depth(J1[lab], direction) * 20))
        nk.pop("depth", None)
        out.append(nk)
    for nk in out: nk["z_index"] = int(round(nk.get("z_index", 0)))
    return out

# ---- the pose library (angles in degrees). Right foot is the striking foot. ----
P = {}
# run cycle keyframes (6 stills): contact / down / passing on each leg
P["run_contactR"] = dict(lean=12, r_hip=35, r_knee=15, l_hip=-25, l_knee=60, r_sh=-35, r_el=80, l_sh=40, l_el=80)
P["run_downR"]    = dict(lean=12, drop=0.06, r_hip=15, r_knee=40, l_hip=-10, l_knee=95, r_sh=-20, r_el=85, l_sh=25, l_el=85)
P["run_passR"]    = dict(lean=12, rise=0.03, r_hip=-15, r_knee=20, l_hip=45, l_knee=90, r_sh=30, r_el=80, l_sh=-30, l_el=80)
P["run_contactL"] = dict(lean=12, l_hip=35, l_knee=15, r_hip=-25, r_knee=60, l_sh=-35, l_el=80, r_sh=40, r_el=80)
P["run_downL"]    = dict(lean=12, drop=0.06, l_hip=15, l_knee=40, r_hip=-10, r_knee=95, l_sh=-20, l_el=85, r_sh=25, r_el=85)
P["run_passL"]    = dict(lean=12, rise=0.03, l_hip=-15, l_knee=20, r_hip=45, r_knee=90, l_sh=30, l_el=80, r_sh=-30, r_el=80)
# shot: plant, backswing, strike, follow-through
P["shot_plant"]   = dict(lean=8, l_hip=20, l_knee=25, r_hip=-30, r_knee=70, l_sh=30, l_shabd=40, r_sh=-20, r_shabd=30)
P["shot_back"]    = dict(lean=4, drop=0.04, l_hip=10, l_knee=30, r_hip=-50, r_knee=110, l_sh=20, l_shabd=60, r_sh=-30, r_shabd=40, twist=-10)
P["shot_strike"]  = dict(lean=-6, drop=0.03, l_hip=5, l_knee=25, r_hip=35, r_knee=20, l_sh=10, l_shabd=70, r_sh=-20, r_shabd=50, twist=10)
P["shot_follow"]  = dict(lean=-10, rise=0.04, l_hip=0, l_knee=10, r_hip=80, r_knee=10, l_sh=40, l_shabd=60, r_sh=-40, r_shabd=40, twist=20)
# pass: short side-foot
P["pass_back"]    = dict(lean=6, l_hip=8, l_knee=20, r_hip=-25, r_knee=40, r_abd=20, l_shabd=30, r_shabd=25)
P["pass_strike"]  = dict(lean=2, l_hip=5, l_knee=20, r_hip=25, r_knee=15, r_abd=25, l_shabd=30, r_shabd=25, twist=8)
# tackles
P["slide"]        = dict(lean=-30, drop=0.40, r_hip=75, r_knee=5, l_hip=20, l_knee=95, r_sh=-15, r_shabd=50, l_sh=-40, l_shabd=35, l_el=30)
P["tackle_stand"] = dict(lean=10, drop=0.08, l_hip=10, l_knee=35, r_hip=40, r_knee=10, r_abd=25, l_shabd=45, r_shabd=40)
# header (jumping)
P["header"]       = dict(lean=-8, rise=0.30, r_hip=20, r_knee=60, l_hip=5, l_knee=40, r_sh=30, r_shabd=70, l_sh=30, l_shabd=70, head=25)

BATCH1 = ["run_contactR", "run_downR", "run_passR", "run_contactL", "run_downL", "run_passL",
          "shot_plant", "shot_back", "shot_strike", "shot_follow", "pass_back", "pass_strike",
          "slide", "tackle_stand", "header"]
