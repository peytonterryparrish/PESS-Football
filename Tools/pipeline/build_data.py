"""Builds GameData/ from the owner's packs (repeatable):
  GameData/Clubs/clubs.json   93 clubs: fictional name, short code, tier, 2005-06 division, kits, badge, stadium slug
  GameData/Badges/<club_id>.png  (Game_256 crests)
  GameData/UI/font_<name>.png/.json  bitmap fonts from the UI pack's glyph sprites
  GameData/Ball/ball_sheet.png  5 logo-free match-ball views (16 px)
  GameData/UI/*.png            HUD / menu pieces the game draws
Run:  python -I Tools/pipeline/build_data.py
"""
import csv, json, os, shutil, sys
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PK = os.path.join(ROOT, "ArtSource", "Packs")
GD = os.path.join(ROOT, "GameData")

def P(*a): return os.path.join(*a)
def mk(d): os.makedirs(d, exist_ok=True); return d

def clubs():
    man = list(csv.DictReader(open(P(PK, "PIX_Premier_Football_R18_Approved_93_Badges", "Manifest.csv"), encoding="utf-8")))
    kits = {k["slug"]: k for k in json.load(open(P(PK, "R08_93_Clubs_South_Idle_Only", "Club_Kit_Palettes.json"), encoding="utf-8"))}
    jerseys = {}
    for r in csv.DictReader(open(P(PK, "Untitled_Retro_Football_93_Club_Tactics_4K", "Tactics_Jerseys", "Club_Jerseys.csv"), encoding="utf-8")):
        jerseys[(r["Slug"], r["Variant"])] = r["PNG"]
    bd = mk(P(GD, "Badges")); jd = mk(P(GD, "Jerseys"))
    out = []
    for r in man:
        slug = r["source_slug"]; cid = r["club_id"]
        shutil.copyfile(P(PK, "PIX_Premier_Football_R18_Approved_93_Badges", r["game_png"]), P(bd, cid + ".png"))
        shutil.copyfile(P(PK, "PIX_Premier_Football_R18_Approved_93_Badges", r["native_png"]), P(bd, cid + "_80.png"))
        k = kits[slug]
        kd = {}
        for slot in ("home", "away", "keeper1", "keeper2"):
            kk = k[slot]
            kd[slot] = {x: kk[x] for x in ("shirt", "trim", "shorts", "socks", "pattern")}
            src = jerseys.get((slug, slot))
            if src:
                sp = P(PK, "Untitled_Retro_Football_93_Club_Tactics_4K", "Tactics_Jerseys", src)
                if os.path.exists(sp): shutil.copyfile(sp, P(jd, f"{cid}_{slot}.png"))
        div = r["division_2005_06"]
        out.append({
            "id": cid, "slug": slug, "name": r["fictional_club"], "short": r["fictional_short_code"],
            "tier": int(r["tier_rank"]), "tier_label": r["tier_label"], "division_2005_06": div,
            "home_hex": r["home_primary_hex"], "kits": kd,
            "badge": f"GameData/Badges/{cid}.png", "badge_small": f"GameData/Badges/{cid}_80.png",
            "jersey_home": f"GameData/Jerseys/{cid}_home.png", "jersey_away": f"GameData/Jerseys/{cid}_away.png",
            "stadium": slug,
        })
    mk(P(GD, "Clubs"))
    json.dump(out, open(P(GD, "Clubs", "clubs.json"), "w"), indent=1)
    print("clubs", len(out), {d: sum(1 for c in out if c["division_2005_06"] == d) for d in set(c["division_2005_06"] for c in out)})

def font(family, name):
    gdir = P(PK, "Untitled_Retro_Football_93_Club_Tactics_4K", "Sprites", "type_glyphs")
    glyphs = []
    for f in sorted(os.listdir(gdir)):
        if not f.startswith(family + "_u"): continue
        cp = int(f.rsplit("_u", 1)[1][:-4], 16)
        im = Image.open(P(gdir, f)).convert("RGBA")
        glyphs.append((cp, im))
    # space
    h = max(g[1].height for g in glyphs)
    ws = [g[1].width for g in glyphs]
    W = 1024; x = y = 0; rowh = h
    rects = {}
    for cp, im in glyphs:
        if x + im.width > W: x = 0; y += rowh + 1
        rects[cp] = (x, y, im.width, im.height); x += im.width + 1
    H = y + rowh + 1
    sheet = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    meta = {"line": h, "glyphs": {}}
    for cp, im in glyphs:
        x, y, w, hh = rects[cp]; sheet.paste(im, (x, y))
        bb = im.getbbox() or (0, 0, w, hh)
        # advance = inked width + a 1px gap; the sprites carry 3px of transparent padding each side
        meta["glyphs"][str(cp)] = [x, y, w, hh, bb[0], bb[2] - bb[0] + 1]
    a_bb = Image.open(P(gdir, family + "_u0041.png")).getbbox()
    meta["space"] = max(4, (a_bb[2] - a_bb[0]) // 2)
    meta["sheet"] = [W, H]
    d = mk(P(GD, "UI")); sheet.save(P(d, f"font_{name}.png"))
    json.dump(meta, open(P(d, f"font_{name}.json"), "w"))
    print("font", name, len(glyphs), "glyphs", (W, H))

def ball():
    src = P(PK, "PIX_2006_07_10_Logo_Free_Ball_Sprites", "Gameplay_16")
    names = ["06-01_Match_Front.png", "06-02_Match_Left_Turn.png", "06-04_Match_Angle_View.png", "06-03_Match_Right_Turn.png", "06-05_Match_Side_Band.png"]
    sheet = Image.new("RGBA", (16 * 5, 16), (0, 0, 0, 0))
    for i, n in enumerate(names): sheet.paste(Image.open(P(src, n)).convert("RGBA"), (i * 16, 0))
    d = mk(P(GD, "Ball")); sheet.save(P(d, "ball_sheet.png"))
    w = Image.new("RGBA", (16 * 5, 16), (0, 0, 0, 0))
    for i, n in enumerate(["06-06_Winter_Front.png", "06-07_Winter_Left_Turn.png", "06-09_Winter_Angle_View.png", "06-08_Winter_Right_Turn.png", "06-10_Winter_Side_Band.png"]): w.paste(Image.open(P(src, n)).convert("RGBA"), (i * 16, 0))
    w.save(P(d, "ball_sheet_winter.png"))

def ui():
    """Copy the UI/HUD sprites the game draws, by id, into GameData/UI/<id>.png (flat)."""
    d = mk(P(GD, "UI"))
    me = P(PK, "Retro_Football_Match_Engine_Revised_4K")
    n = 0
    for r in csv.DictReader(open(P(me, "Asset_Manifest.csv"), encoding="utf-8")):
        if r["group"] in ("01_Match_HUD", "02_Numbers_Names_Events", "03_Minimap"):
            shutil.copyfile(P(me, r["png"]), P(d, r["id"] + ".png")); n += 1
    ui_root = P(PK, "Untitled_Retro_Football_93_Club_Tactics_4K")
    for r in csv.DictReader(open(P(ui_root, "Sprite_Manifest.csv"), encoding="utf-8")):
        src = P(ui_root, r["asset"])
        if os.path.exists(src): shutil.copyfile(src, P(d, r["id"] + ".png")); n += 1
    # menu backgrounds (16:9 4K plates) are read in place from ArtSource; list them for the game
    bgs = []
    for man, root in ((P(PK, "Broadcast_Signal_300_Backgrounds_4K", "Background_Manifest.csv"), "Broadcast_Signal_300_Backgrounds_4K"),
                      (P(PK, "Broadcast_Signal_England_Arcade_50_4K", "Manifest.csv"), "Broadcast_Signal_England_Arcade_50_4K")):
        for r in csv.DictReader(open(man, encoding="utf-8")):
            bgs.append({"id": r["id"], "path": f"ArtSource/Packs/{root}/{r['file']}", "collection": r["collection"]})
    json.dump(bgs, open(P(d, "backgrounds.json"), "w"), indent=0)
    print("ui sprites", n, "backgrounds", len(bgs))

if __name__ == "__main__":
    clubs(); font("type_score_glyph", "score"); font("type_squad_glyph", "squad"); ball(); ui()
