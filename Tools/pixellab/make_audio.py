"""Generates the game's sound effects and music loops with PixelLab's sound tool (WAV).
  python -I Tools/pixellab/make_audio.py      -> ArtWork/audio/<name>.wav (skips existing)
Then Tools/ue/import_audio.py brings them into /Game/PF/Audio."""
import json, os, sys, time, urllib.request
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import plclient

SFX = {
    "kick_pass":    ("a crisp leather football being side-footed on grass, short soft thud", 0.6),
    "kick_shot":    ("a powerful kick of a leather football, loud punchy thump", 0.7),
    "kick_header":  ("a football bouncing off a player's head, dull thud", 0.5),
    "ball_bounce":  ("a leather football bouncing once on a grass pitch", 0.5),
    "net":          ("a football hitting the back of a goal net, swishing rope net ripple", 1.0),
    "post":         ("a football striking a metal goalpost, loud clang ring", 1.0),
    "catch":        ("goalkeeper catching a football with gloves, firm leather slap", 0.5),
    "slide":        ("a footballer sliding across wet grass, quick scuff and swish", 0.8),
    "whistle_short":("a referee's whistle, one short sharp blast", 0.6),
    "whistle_long": ("a referee's whistle, one long blast for full time", 1.6),
    "whistle_three":("a referee whistling three times, two short blasts and one long for the end of a football match", 2.4),
    "crowd_goal":   ("a big English football crowd erupting into a huge roar after a goal, cheering and chanting", 5.0),
    "crowd_ooh":    ("a football crowd going oooh after a near miss", 2.0),
    "crowd_groan":  ("a football crowd groaning in disappointment", 1.8),
    "crowd_loop":   ("ambient English football stadium crowd on a Saturday afternoon, steady murmur, distant chants and claps, no announcer", 20.0),
    "ui_move":      ("a short retro 16-bit menu cursor blip", 0.5),
    "ui_select":    ("a bright retro 16-bit menu confirm chime", 0.6),
    "ui_back":      ("a low retro 16-bit menu back blip", 0.5),
    "music_title":  ("an upbeat 16-bit SNES chiptune football game title theme, driving bass, bright lead melody, triumphant, seamless loop", 30.0),
    "music_menu":   ("a relaxed 16-bit SNES chiptune menu music loop for a football management game, bouncy bass, light melody, seamless loop", 30.0),
}

def make(name, prompt, secs, out):
    r = plclient._req("POST", "/sound-effects", {"prompt": prompt, "seconds": secs, "name": "PF " + name})
    sid = r.get("id") or r.get("sound_effect_id") or (r.get("sound_effect") or {}).get("id")
    jid = r.get("background_job_id")
    for _ in range(90):
        time.sleep(3)
        j = plclient._req("GET", f"/sound-effects/{sid}") if sid else plclient._req("GET", f"/background-jobs/{jid}")
        url = j.get("audio_url") or (j.get("last_response") or {}).get("audio_url")
        if url:
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (game-asset-pipeline)"})
            with urllib.request.urlopen(req, timeout=120) as resp, open(out, "wb") as f: f.write(resp.read())
            return True
        if j.get("status") == "failed": print(name, "failed", json.dumps(j)[:300]); return False
    return False

if __name__ == "__main__":
    d = os.path.join(ROOT, "ArtWork", "audio"); os.makedirs(d, exist_ok=True)
    for n, (p, s) in SFX.items():
        out = os.path.join(d, n + ".wav")
        if os.path.exists(out): continue
        try: print(n, make(n, p, s, out), flush=True)
        except Exception as e: print(n, "ERROR", str(e)[:300], flush=True)
    print("balance", plclient.balance()["credits"])
