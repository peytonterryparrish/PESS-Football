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
| Short pass / select (defending: switch player) | A | J / Enter |
| Shoot, hold for power (defending: slide tackle) / back | B | L / Backspace |
| Cross (lofted pass outside the attacking third; defending: tackle) | X | K |
| Through ball (defending: tackle) | Y | I |
| Chip shot | LB + B | Q + L |
| Switch player | LB | Q |
| Skill moves: feint left / step-over and knock right | Right stick flick left / right | Z / C |
| Aftertouch curl (just after a shot, cross or chip) | Left stick across the ball's flight | WASD |
| Sprint | RT | Shift |
| Pause | Start | Esc / P |

Your keeper is yours when he has the ball. In his hands he can carry it anywhere in his area: A throws it to the team-mate you point at, X throws it long, Y rolls it out, and B kicks it out of his hands. At throw-ins, rings mark your three nearest team-mates. Point at one and press A.

Options has music, effects, half length and camera distance (near / medium / far).

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
