# Base-body art pilot (v0.8, 2026-11-25)

The owner approved a pilot of the art plan in `docs/STATUS_v0.8.md`: one outfield body and one keeper body, each with one consistent, hand-checked animation set, coloured into any club kit by code.

## v0.8.1: the full set, now the default
The owner approved the full set after the pilot, then asked for base bodies to be the default.

| Body | Source | Look | Status |
|---|---|---|---|
| B01 | O01 (the owner's Portsmouth F001 idle) | fair, dark brown crew cut, tall | in |
| B02 | O11 | golden tan, close buzz, stocky | in |
| B03 | O06 | warm brown, bald, broad | in |
| B04 | O12 | fair, dark blonde crop, tall | in |
| B05 | O14 | deep brown, short dreadlocks, compact | in, back-view sash painted out |
| B06 | O09 | golden tan, ginger mop | **held back** (`ArtWork/base_pilot/held_back/`) |
| BK1 | Chelsea G014 keeper | warm brown, short black hair | in |
| BK2 | Newcastle G003 keeper | deep brown, crew cut | in |

- **Which body a player gets.** His template body (O01 to O16, K01 to K04) maps to the nearest base body by skin tone and hair (`PFMatch.cpp`). Unknown ids, and an outfield body in goal, are hashed from the player's name.
- **`-pfclubsets`** brings back v0.7's per-club art.

**Audit findings and fixes** (contact sheets `ArtWork/base_pilot/sheet_*`)
- **B03:** grey and purple specks in kit areas, from the guide and from O06's own grey blot on the back of the shorts.
  - New rule `kit_fill`: a non-warm pixel with 3 or more of its 8 neighbours in one kit region becomes that region.
  - The "grey" left at the knees is the sock band. It is trim at the darkest shade, so a white trim reads grey; that is the kit design.
- **B03 east kicks:** the auto-picked take had balls touching the boot and the head. Pinned to take 0 (`picks.json`), whose balls stand clear and are cut.
- **B05:** O14's v0.6 back turns grew a pink and yellow sash that the front design doesn't have. `no_sash` paints trim and bright leftovers above the waistband into the shirt, in back views only.
- **B06:** held back.
  - Its hair is dark brown in the front view and ginger in the other four.
  - Its standing turns carry a flame-like hair wisp the moving frames lack.
  - One dive keeps a ball.
  - This is rotation drift from v0.6, so re-picking takes can't fix it. Its players use B02.

## Pilot result
- **B01 (outfield).** Built on the owner's Portsmouth F001 idle, in the template kit; its eight turns were reused from v0.6.
  - 273 frames in 5 drawn directions, with the other 3 mirrored.
  - Cycles: run 8, sprint 6, dribble 6, walk 4.
  - The rest: slide (drop, slide, sit, up), tackle, block, fall and get-up, pass 2, shot 4, volley, header 3, chest, turn, shield, celebrations, throw-in.
- **BK1 (keeper).** Built on the owner's Chelsea G014 keeper idle, recoloured into the template kit.
  - 174 frames: set position, shuffles, catches, hold, scoop, punch, throw, punt, dive starts and stretches, run 6, walk 4.
  - Also the owner's six audited flat dives from the R08 pack, recoloured into the template kit.
- **In the game.** `-pfbase` puts every player in these bodies, in his club's kit (stripes included) by the sprite material. Screenshots are in `ArtWork/base_pilot/ingame*`.

**What changed in how the art is made**
- Every animation is generated as one PixelLab job, with its frames in playing order (`Tools/pixellab/poses_base.py`, `produce_base.py`).
  - v0.7 mixed unrelated poses in one job and posed every direction separately.
  - The cycles now belong together: the side-view run reads as one stride, contact → load → push → flight.
- **Natural gait angles, not v0.7's exaggerated poses.** The game plays the 8-frame run and the 6-frame sprint and dribble when a set has them (`PoseFor`).
- **Three takes of every job.** The builder picks the cleanest take per direction and animation (`Tools/pipeline/build_base_atlas.py`, overridable in `ArtWork/base_pilot/picks.json`). One take is always used for a whole cycle.
- **Clean-up for base bodies.** The template kit has no white or grey, so the builder can safely:
  - cut out baked-in balls (round light blobs at the feet, overhead, at the end of a lying figure, or held to the chest);
  - cut out the pose guide's grey limbs;
  - drop loose fragments.
- **`kit.segment_generic`** puts any club's kit into the template, not just Portsmouth's home kit.

## Cost
| Item | Spend |
|---|---|
| Test run, 5 jobs | $0.08 |
| Both bodies, 3 takes | $6.17 |
| Keeper redo, first try rejected (see below) | $3.55 |
| **Pilot total** | **about $9.80** |
| Full set: B02 to B06 and BK2, 3 takes (495 jobs) | $17.60 |
| **All bodies** | **about $27.40** |

- **Price per job.** About $0.016 per job, roughly ten times cheaper than the v0.7 estimate.
- **The full plan.** 6 outfield and 2 keeper bodies would cost about **$40 to $60**, not $90 to $120.
- **PixelLab balance.** $482.39 after the full set.
- **Other spending.** Between the pilot ($531.99) and the full run ($499.99), $32 went elsewhere on the account. The medieval RTS project's PixelLab work in a parallel session is the likely cause.
- **Spending elsewhere on the account.** The balance was $542.89 when the pilot started, against $556.57 at the start of v0.8. About $13.70 went somewhere other than this pilot.

## What went wrong and was fixed
- **First keeper rejected.** The template conversion only knew Portsmouth's home kit, so the keeper's reference stayed yellow. PixelLab then repainted the poses from the text prompt, with a big afro that isn't in the owner's idle. That fails the idle-match rule.
  - The take is archived in `ArtWork/raw_base/_rejected_BK1_v1/`.
  - `kit.segment_generic` fixed the conversion, and the redo describes the idle's short hair.
- **Baked-in balls and grey limbs.** Removed by the builder rules above. Checked on contact sheets in `ArtWork/base_pilot/sheet_*`.

## Open
- **B06 or a replacement.** It needs fresh turns from a body whose turns hold its hair. About $3.50.
- **The per-club sets** (279) are still packaged. Dropping them would shrink the build; that's the owner's call.
- **Small leftovers.** A few grey specks remain on the fall frame, and some celebration hands are greyish.
