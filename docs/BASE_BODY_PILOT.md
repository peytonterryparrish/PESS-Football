# Base-body art pilot (v0.8, 2026-11-25)

The owner approved a pilot of the art plan in `docs/STATUS_v0.8.md`: one outfield body and one keeper body, each with one consistent, hand-checked animation set, coloured into any club kit by code.

## Result
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
| **Total** | **about $9.80** |

- **Price per job.** About $0.016 per job, roughly ten times cheaper than the v0.7 estimate.
- **The full plan.** 6 outfield and 2 keeper bodies would cost about **$40 to $60**, not $90 to $120.
- **PixelLab balance.** $531.99 now.
- **Spending elsewhere on the account.** The balance was $542.89 when the pilot started, against $556.57 at the start of v0.8. About $13.70 went somewhere other than this pilot.

## What went wrong and was fixed
- **First keeper rejected.** The template conversion only knew Portsmouth's home kit, so the keeper's reference stayed yellow. PixelLab then repainted the poses from the text prompt, with a big afro that isn't in the owner's idle. That fails the idle-match rule.
  - The take is archived in `ArtWork/raw_base/_rejected_BK1_v1/`.
  - `kit.segment_generic` fixed the conversion, and the redo describes the idle's short hair.
- **Baked-in balls and grey limbs.** Removed by the builder rules above. Checked on contact sheets in `ArtWork/base_pilot/sheet_*`.

## Open
- **One face for everyone.** Every outfielder wears the same face and hair. The full plan adds 5 more outfield bodies (build × skin tone, from the owner's idles) and a second keeper.
- **Not the default yet.** `-pfbase` is a test switch. Making base bodies the default, and dropping the 279 per-club sets, is the owner's call. The clubs would lose only kit details beyond colours and pattern.
- **Small leftovers.** A few grey specks remain on the fall frame, and some celebration hands are greyish.
