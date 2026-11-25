"""Creates the few Unreal assets the game needs (everything else loads from GameData at run time):
  /Game/PF/M_PFSprite  - UI-domain sprite material: nearest-sampled atlas cell + kit recolouring of key colours
  /Game/PF/M_PFPitch   - UI-domain pitch: world-locked dithered grass, mowing stripes and markings (v0.5)
  /Game/Maps/Main      - empty map (the HUD canvas draws the whole game)
Run headless: Tools/ue/run_py.sh Tools/ue/make_assets.py make_assets
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

HLSL = r"""
// 3x3 supersampling over the screen pixel's footprint: sprites are drawn below 1:1 since v0.4 (half-size players),
// so each sample is point-sampled and recoloured, then the results are averaged (alpha-weighted, in linear light)
float2 dx = ddx(UV), dy = ddy(UV);
float4 acc = 0;
[unroll] for (int i = 0; i < 3; i++)
[unroll] for (int j = 0; j < 3; j++)
{
    float2 uv = UV + dx * ((i - 1.0) / 3.0) + dy * ((j - 1.0) / 3.0);
    float4 c = Texture2DSampleGrad(Tex, TexSampler, uv, dx, dy);
    float3 s = c.rgb;
    float3 o = s;
    if (KitOn > 0.5)
    {
        float3 code = round(s * 255.0);
        if (abs(code.b - 1.0) < 0.5 && code.r >= 30.0)
        {
            float region = round(code.r / 40.0);
            float g = code.g;
            float m = g < 96.0 ? 0.55 : (g < 160.0 ? 0.78 : (g < 224.0 ? 1.0 : 1.22));
            float2 lp = fmod(uv * AtlasCell.xy, AtlasCell.z);
            float3 base = ShirtA;
            if (region < 1.5)
            {
                if (Pattern > 0.5 && Pattern < 1.5 && fmod(floor(lp.x / 2.0), 2.0) > 0.5) base = ShirtB;
                if (Pattern > 1.5 && Pattern < 2.5 && fmod(floor(lp.y / 3.0), 2.0) > 0.5) base = ShirtB;
                if (Pattern > 2.5 && lp.x > AtlasCell.z * 0.5) base = ShirtB;
            }
            else if (region < 2.5) base = Shorts;
            else if (region < 3.5) base = Socks;
            else base = ShirtB;
            s = saturate(base * m);
        }
        // kit-keyed atlases are stored as raw sRGB codes: decode to linear here
        o = s <= 0.04045 ? s / 12.92 : pow((s + 0.055) / 1.055, 2.4);
    }
    acc += float4(o * c.a, c.a);
}
acc /= 9.0;
return float4(acc.a > 0.0001 ? acc.rgb / acc.a : float3(0, 0, 0), acc.a);
"""

def make_material():
    path = "/Game/PF/M_PFSprite"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset("M_PFSprite", "/Game/PF", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("opacity_mask_clip_value", 0.5)

    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -900, -200)
    tex.set_editor_property("parameter_name", "Atlas")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)

    def vparam(name, y, val=(1, 1, 1, 1)):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", unreal.LinearColor(*val))
        return e
    def sparam(name, y, val=0.0):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", val)
        return e

    ins = {
        "Tex": tex, "UV": uv,
        "ShirtA": vparam("ShirtA", 100), "ShirtB": vparam("ShirtB", 250), "Shorts": vparam("Shorts", 400), "Socks": vparam("Socks", 550),
        "KitOn": sparam("KitOn", 700), "Pattern": sparam("Pattern", 800), "AtlasCell": vparam("AtlasCell", 900, (1920, 1152, 96, 0)),
    }
    cust = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cust.set_editor_property("code", HLSL)
    cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    cust.set_editor_property("description", "PFSprite")
    cin = []
    for n in ins:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        cin.append(ci)
    cust.set_editor_property("inputs", cin)
    for n, e in ins.items():
        out = ""
        if n in ("ShirtA", "ShirtB", "Shorts", "Socks"):
            # RGB only
            mask = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -650, e.get_editor_property("material_expression_editor_y"))
            mask.set_editor_property("r", True); mask.set_editor_property("g", True); mask.set_editor_property("b", True); mask.set_editor_property("a", False)
            MEL.connect_material_expressions(e, "", mask, "")
            MEL.connect_material_expressions(mask, "", cust, n)
        else:
            MEL.connect_material_expressions(e, out, cust, n)
    rgb = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, -50)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True); rgb.set_editor_property("a", False)
    a = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, 100)
    a.set_editor_property("r", False); a.set_editor_property("g", False); a.set_editor_property("b", False); a.set_editor_property("a", True)
    MEL.connect_material_expressions(cust, "", rgb, "")
    MEL.connect_material_expressions(cust, "", a, "")
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_asset(path)
    unreal.log("[PF] material ok")

PITCH_HLSL = r"""
// ISS-style pitch, drawn as one full-screen tile. Every 'grass pixel' is a Pix x Pix block of screen pixels locked to
// the world (Cam = the camera's offset in screen px), so the dither never swims as the camera pans.
// P1 = (screen w, screen h, Pix); G0 = ground at screen (0,0); GA / GB = ground metres per screen px along x / y.
float pix = max(P1.z, 1.0);
float2 sp = UV * P1.xy;
float2 q = floor((sp + Cam.xy) / pix);
float2 c = q * pix + pix * 0.5 - Cam.xy;
float2 g = G0.xy + c.x * GA.xy + c.y * GB.xy;
if (abs(g.x) > 56.75 || g.y < -34.3 || g.y > 34.0) return float4(0, 0, 0, 0);
// clustered speckle: three octaves of a hashed block value
float v = 0;
[unroll] for (int k = 0; k < 3; k++)
{
    float s = k == 0 ? 1.0 : (k == 1 ? 2.0 : 4.0);
    float w = k == 0 ? 0.5 : (k == 1 ? 0.3 : 0.2);
    int2 qi = int2(floor(q / s)) + int2(40000 + k * 977, 40000 + k * 331);
    uint h = uint(qi.x) * 1664525u + uint(qi.y) * 1013904223u;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    v += w * float(h & 65535u) / 65535.0;
}
// mowing stripes: narrow bands across the pitch, about 0.4 of a player's drawn height wide (as ISS Deluxe)
bool bandA = frac((g.x + 52.5) / 1.0) < 0.5;
// palette sampled from ISS Deluxe footage (dominant grass colours: 160,216,80 and 140,180,58)
float3 base  = bandA ? float3(160, 216, 78) : float3(140, 182, 58);
float3 dark  = bandA ? float3(140, 186, 62) : float3(120, 160, 46);
float3 light = bandA ? float3(176, 230, 96) : float3(156, 202, 72);
float3 col = v < 0.33 ? dark : (v > 0.69 ? light : base);
// markings (the pitch is symmetric in both axes): distances in metres to each line centre
float ax = abs(g.x), ay = abs(g.y);
float L = 52.44, Wd = 33.94;
float d = 1e9;
#define HSEG(Y0, A, B) d = min(d, length(float2(max(max((A) - ax, ax - (B)), 0.0), ay - (Y0))))
#define VSEG(X0, A, B) d = min(d, length(float2(ax - (X0), max(max((A) - ay, ay - (B)), 0.0))))
HSEG(Wd, 0.0, L);            // touchlines
VSEG(L, 0.0, Wd);            // goal lines
VSEG(0.0, 0.0, Wd);          // halfway line
VSEG(36.06, 0.0, 20.16); HSEG(20.16, 36.06, L);   // penalty area
VSEG(47.06, 0.0, 9.16);  HSEG(9.16, 47.06, L);    // goal area
d = min(d, abs(length(g) - 9.15));                // centre circle
if (ax < 36.0) d = min(d, abs(length(float2(ax - 41.5, ay)) - 9.15));   // penalty arcs
if (ax <= 52.5 && ay <= 34.0) d = min(d, abs(length(float2(ax - 52.5, ay - 34.0)) - 0.94));   // corner arcs
d = min(d, max(length(g) - 0.14, 0.0));           // spots
d = min(d, max(length(float2(ax - 41.5, ay)) - 0.14, 0.0));
if (d < 0.06) col = v < 0.2 ? float3(204, 210, 198) : float3(240, 242, 234);
float3 sc = saturate(col / 255.0 * Tint.rgb);
float3 o = sc <= 0.04045 ? sc / 12.92 : pow((sc + 0.055) / 1.055, 2.4);
return float4(o, 1.0);
"""

STANDS_HLSL = r"""
// v0.7 stands: the stadium pack leaves the left-end stand as an empty grey slab (it slopes slightly DOWN in the
// projection, so it reads as a flat plane) and the near side as a void with stray wall panels. Both are painted here
// on the ground plane, world-locked like the pitch: advertising boards, a front wall, then rows of seats in the home
// club's colour filled with supporters. Sizes are halved like the players (seat 0.28 m, row 0.45 m) so fans sit in
// proportion with the half-height players.
float pix = max(P1.z, 1.0);
float2 sp = UV * P1.xy;
float2 q = floor((sp + Cam.xy) / pix);
float2 c = q * pix + pix * 0.5 - Cam.xy;
float2 g = G0.xy + c.x * GA.xy + c.y * GB.xy;
float dl = -60.6 - g.x;          // depth into the left-end stand
float dn = -36.6 - g.y;          // depth into the near-side stand
bool inL = dl > 0.0 && g.y < 33.6;
bool inN = dn > 0.0 && g.x < 62.0;
if (!inL && !inN) return float4(0, 0, 0, 0);
bool nearS = inN && (!inL || dn > dl);
float r = nearS ? dn : dl;
float a = nearS ? g.x : -g.y;
#define HASH(X, Y, OUT) { uint hh = uint(int(X) + 70000) * 1664525u + uint(int(Y) + 70000) * 1013904223u; hh ^= hh >> 16; hh *= 0x7feb352du; hh ^= hh >> 15; hh *= 0x846ca68bu; hh ^= hh >> 16; OUT = float(hh & 65535u) / 65535.0; }
float3 col;
if (r < 0.9)
{
    // advertising boards, 3 m panels: a bright ground and a row of light 'lettering' blocks
    float bi = floor((a + 300.0) / 3.0);
    float hb; HASH(bi, 17, hb);
    float3 pal[6] = { float3(196, 32, 38), float3(24, 70, 160), float3(250, 200, 30), float3(20, 120, 70), float3(235, 235, 235), float3(30, 30, 36) };
    col = pal[min(5, int(hb * 6.0))];
    float fr = r / 0.9;
    float lu = floor((a + 300.0) / 0.22);
    float hl; HASH(lu, bi, hl);
    if (fr > 0.3 && fr < 0.7 && hl > 0.42 && frac((a + 300.0) / 3.0) > 0.08 && frac((a + 300.0) / 3.0) < 0.92)
        col = hb > 0.66 && hb < 0.84 ? float3(30, 30, 36) : float3(250, 250, 240);
    if (frac((a + 300.0) / 3.0) < 0.02) col *= 0.55;
}
else if (r < 1.25)
{
    col = float3(58, 60, 66);                       // the front wall / walkway
    if (r > 1.17) col = float3(120, 122, 126);
}
else
{
    float rr = r - 1.25;
    float row = floor(rr / 0.45);
    float f = frac(rr / 0.45);
    float sa = (a + 500.0) / 0.28;
    float seat = floor(sa);
    float u = frac(sa);
    float shade = lerp(1.0, 0.78, saturate(rr / 22.0)) * (fmod(row, 2.0) < 1.0 ? 1.0 : 0.94);
    if (fmod(seat, 24.0) < 2.0)
    {
        col = f < 0.25 ? float3(96, 98, 104) : float3(150, 152, 156);   // stairway
    }
    else if (f < 0.2)
    {
        col = float3(72, 74, 80);                    // riser
    }
    else
    {
        float v = (f - 0.2) / 0.8;
        float ho; HASH(seat, row, ho);
        float3 seatc = Seat.rgb * 255.0 * 0.8;
        col = v < 0.35 ? seatc * 0.7 : seatc;
        if (ho < 0.8)
        {
            float hs; HASH(seat + 911, row, hs);
            float hk; HASH(seat, row + 733, hk);
            float3 skins[4] = { float3(236, 190, 152), float3(204, 146, 104), float3(146, 90, 58), float3(92, 58, 40) };
            float3 hair[3] = { float3(42, 32, 26), float3(96, 62, 36), float3(206, 172, 96) };
            float3 shirt = hs < 0.62 ? Seat.rgb * 255.0 : hs < 0.74 ? float3(240, 240, 236) : hs < 0.84 ? float3(34, 34, 44) : hs < 0.9 ? Seat2.rgb * 255.0 : float3(150 + 100 * hk, 60 + 150 * hs, 80 * hk + 40);
            float2 hc = float2(0.5 + (hk - 0.5) * 0.2, 0.34);
            float2 dd = float2((u - hc.x) * 0.28, (v - hc.y) * 0.36);
            if (length(dd) < 0.085) col = (v < hc.y - 0.05) ? hair[min(2, int(hk * 3.0))] : skins[min(3, int(hs * 4.0))];
            else if (v > 0.58 && abs(u - 0.5) < 0.46) col = shirt;
        }
    }
    col *= shade;
}
float3 sc = saturate(col / 255.0 * Tint.rgb);
float3 o = sc <= 0.04045 ? sc / 12.92 : pow((sc + 0.055) / 1.055, 2.4);
return float4(o, 1.0);
"""

def make_ui_material(name, hlsl, params):
    path = "/Game/PF/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset(name, "/Game/PF", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    ins = {"UV": uv}
    y = 100
    for n in params:
        e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y); y += 150
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        ins[n] = e
    cust = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cust.set_editor_property("code", hlsl)
    cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    cust.set_editor_property("description", name)
    cin = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); cin.append(ci)
    cust.set_editor_property("inputs", cin)
    for n, e in ins.items():
        if n == "UV":
            MEL.connect_material_expressions(e, "", cust, n)
        else:
            mask = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -650, e.get_editor_property("material_expression_editor_y"))
            mask.set_editor_property("r", True); mask.set_editor_property("g", True); mask.set_editor_property("b", True); mask.set_editor_property("a", False)
            MEL.connect_material_expressions(e, "", mask, "")
            MEL.connect_material_expressions(mask, "", cust, n)
    rgb = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, -50)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True); rgb.set_editor_property("a", False)
    a = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, 100)
    a.set_editor_property("r", False); a.set_editor_property("g", False); a.set_editor_property("b", False); a.set_editor_property("a", True)
    MEL.connect_material_expressions(cust, "", rgb, "")
    MEL.connect_material_expressions(cust, "", a, "")
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_asset(path)
    unreal.log("[PF] %s ok" % name)

def make_pitch_material():
    path = "/Game/PF/M_PFPitch"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = AT.create_asset("M_PFPitch", "/Game/PF", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = MEL.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    ins = {"UV": uv}
    y = 100
    for n in ("P1", "G0", "GA", "GB", "Cam", "Tint"):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, y); y += 150
        e.set_editor_property("parameter_name", n)
        e.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        ins[n] = e
    cust = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -400, 0)
    cust.set_editor_property("code", PITCH_HLSL)
    cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    cust.set_editor_property("description", "PFPitch")
    cin = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); cin.append(ci)
    cust.set_editor_property("inputs", cin)
    for n, e in ins.items():
        if n == "UV":
            MEL.connect_material_expressions(e, "", cust, n)
        else:
            mask = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -650, e.get_editor_property("material_expression_editor_y"))
            mask.set_editor_property("r", True); mask.set_editor_property("g", True); mask.set_editor_property("b", True); mask.set_editor_property("a", False)
            MEL.connect_material_expressions(e, "", mask, "")
            MEL.connect_material_expressions(mask, "", cust, n)
    rgb = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, -50)
    rgb.set_editor_property("r", True); rgb.set_editor_property("g", True); rgb.set_editor_property("b", True); rgb.set_editor_property("a", False)
    a = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -150, 100)
    a.set_editor_property("r", False); a.set_editor_property("g", False); a.set_editor_property("b", False); a.set_editor_property("a", True)
    MEL.connect_material_expressions(cust, "", rgb, "")
    MEL.connect_material_expressions(cust, "", a, "")
    MEL.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_asset(path)
    unreal.log("[PF] pitch material ok")

def make_map():
    path = "/Game/Maps/Main"
    if EAL.does_asset_exist(path):
        unreal.log("[PF] map exists"); return
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level(path)
    les.save_current_level()
    unreal.log("[PF] map ok")

make_material()
make_pitch_material()
make_ui_material("M_PFStands", STANDS_HLSL, ("P1", "G0", "GA", "GB", "Cam", "Tint", "Seat", "Seat2"))
make_map()
