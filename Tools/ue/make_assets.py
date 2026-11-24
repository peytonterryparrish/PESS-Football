"""Creates the few Unreal assets the game needs (everything else loads from GameData at run time):
  /Game/PF/M_PFSprite  - UI-domain sprite material: nearest-sampled atlas cell + kit recolouring of key colours
  /Game/Maps/Main      - empty map (the HUD canvas draws the whole game)
Run headless: Tools/ue/run_py.sh Tools/ue/make_assets.py make_assets
"""
import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

HLSL = r"""
float4 c = Texture2DSample(Tex, TexSampler, UV);
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
        float2 lp = fmod(UV * AtlasCell.xy, AtlasCell.z);
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
return float4(o, c.a);
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

def make_map():
    path = "/Game/Maps/Main"
    if EAL.does_asset_exist(path):
        unreal.log("[PF] map exists"); return
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level(path)
    les.save_current_level()
    unreal.log("[PF] map ok")

make_material()
make_map()
