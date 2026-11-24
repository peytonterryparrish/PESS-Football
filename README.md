# Proper Football

An arcade English football game in Unreal Engine 5.8, inspired by the feel of the SNES-era classics: a close, diagonal camera, chunky pixel art, fast play, simple menus. Offline only.

- **Play** any match yourself with a controller, or **manage** it and watch the highlights.
- **Endless career** across five English tiers. Start at a random League Two club, survive, climb, get sacked, move on.
- **Youth academy and scouting.** You never see a youth player's stats while scouting: you watch them play. Talent is obvious on the pitch.

![Match](docs/images/match.png)
![Career office](docs/images/career_office.png)

## Running it

Requirements: Unreal Engine 5.8 (source build at `D:\Epic Games\UE_5.8`), Visual Studio 2022, Python 3 with Pillow and NumPy, Git LFS.

```
bash Tools/ue/build.sh                  # build the game module
bash Tools/ue/run_game.sh               # play (windowed 1920x1080)
bash Tools/ue/run_game.sh -pfmatch      # straight into a friendly
bash Tools/ue/run_game.sh -pfcareertest=10   # headless: simulate 10 career seasons and log them
```

Controls (pad first, keyboard fallback):

| | Pad | Keyboard |
|---|---|---|
| Move | Left stick / d-pad | WASD / arrows |
| Pass / select | A | J / Enter |
| Lob / back | B | L / Backspace |
| Shoot (hold for power) / slide tackle | X | K |
| Through ball | Y | I |
| Switch player | LB | Q |
| Sprint | RT | Shift |
| Pause | Start | Esc |

## Where things are

| Path | What |
|---|---|
| `Source/ProperFootball/` | C++: match engine (`PFMatch`), sprites (`PFSprites`), world data (`PFWorld`), career model and sim (`PFCareerData`), screens (`PFFlow`, `PFCareer`) |
| `ArtSource/Packs/` | The owner's art packs, untouched (stadiums, idles, badges, UI, backgrounds, balls) |
| `Tools/pixellab/` | PixelLab client, pose library, production runner |
| `Tools/pipeline/` | Data build, atlas build + QA, contact sheets |
| `Tools/ue/` | Build, run, and editor-asset scripts |
| `GameData/` | Generated game data read at run time (sprite atlases, clubs, fonts, badges) |
| `ArtWork/` | Raw PixelLab output, QA reports, contact sheets |
| `docs/DESIGN.md` | The source of truth for design and pipeline decisions |

See `CHANGELOG.md` for versions.
