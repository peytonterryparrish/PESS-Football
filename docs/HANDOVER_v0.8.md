# Hand-over after v0.8 (2026-11-25)

**State:** branch `v080`, packaged as `C:\PFBuild\Builds\ProperFootball_v0.8.0_Windows(.zip)`. It is merged and released only after the owner's OK. Full detail: `docs/STATUS_v0.8.md`. ISS numbers: `docs/ISS_STUDY.md` §5.

**Where things live**
- **Passing:**
  - `BestPassTarget` / `BestPassTargetCone`: the fallback to "always a team-mate" applies to the pad's presses only.
  - Guided pass: `Kick()` default case (`Ball.Guide`, `Ball.GuideV`), the homing in `BallPhysics`, the dead trap in `Possession`.
- **Slides:**
  - `MovePlayers` (`bPadSlide`: the generous reach is the pad's only).
  - Side-on drawing in `Draw` (`DrawDir`), the clip in `PFSprites.cpp`.
- **Goals:** the `GoalBack` and `GoalFrame` lambdas in `Draw`. `-pfgoalart` shows the old art.
- **Cadence:** `MovePlayers`, ISS stride model. `-pfcada=` scales it, `-pfcadb=` sets the stride growth.
- **Keeper:**
  - Hand anchors: `GameData/Sprites/Clubs/*_hands.json` from `Tools/pipeline/hand_anchors.py`. Re-run it after any atlas rebuild.
  - Drawn by `HeldBall` in `Draw`.
  - Dive: `DiveMoveT` 0.35 s (balance) and `DiveFlightT` 0.22 s (look).
- **Sprites:**
  - `Tools/pipeline/build_club_atlas.py`: `islands`, `tidy`, `tone_match`.
  - Audit with `Tools/pipeline/audit_sprites.py`; reports go to `ArtWork/audit_v08/`.
- **Tests:**
  - `-pfautopad` (PFPLAY lines now count short passes; PFPASSLOST logs each pass you lose).
  - `-pfactshots=N` (holds, landings, slides), `-pfoldhold`, `-pfsimtest=N` (deterministic).

**Next**
1. **Base bodies:** the owner's decision on the base-body art plan (STATUS §art). A pilot of 1 outfield and 1 keeper body costs about $25 to $35; the full plan about $90 to $120. Ask before spending over $50. The PixelLab balance was $556.57 at the start of v0.8, and nothing was spent this round.
2. **Weak sets:** regenerate `rochdale_home`, `derby-county_home`, and the `fall_ground` cells, or let base bodies replace them.
3. **Goal rate:** AI matches score 1.73 goals per match (v0.7: 2.1). Nudge it if the owner wants more.
