# Proper Football v0.8: status

Round of 2026-11-25. Covers the owner's notes on v0.7.0, what was fixed and how it was checked, the playtest, balance, the ISS Deluxe study, and the art recommendation.

## The owner's notes and what changed

| # | Owner's note | Cause found | Fix | Checked by |
|---|---|---|---|---|
| 1 | Passing doesn't feel good; it should be predetermined, assisted, snappy and land at the feet | A pressed with no team-mate in the stick's ±70° cone kicked the ball 14 m into space. Most of the auto-player's lost passes were these. Passes also rolled in slowly and could be misplaced. | A always finds a team-mate: the stick's cone, then a wider cone, then the nearest man. The pass is aimed where he'll be, travels at a steady 12 to 16.5 m/s, is steered onto his feet in flight and is killed dead there. Your side never misplaces one. The receiver checks his run and steps toward it. | Autopad short passes: 85% completed (80 of 94), 75 to the intended man. v0.7: 7 to 31% of all passes, 36% of short passes in this round's first build. The rest are interceptions in the lane. |
| 2 | Slide tackles don't work; no slide animation | The front and back slide frames read as a man standing with his arms out. Most slides were seen from those angles. The slide aimed only where the stick pointed. | Slides are drawn side-on: slide, sit on the turf with the leg out, up (0.73 s, about 2 m, ISS timings). With the ball within 5 m the slide locks onto it. The boots reach a little further, and a side-on slide that catches the man is a foul less often. | `-pfactshots` captures in `ArtWork/playtest_v08/act1*`. |
| 3 | Pixel bleed, missing pixels, grey spots | v0.7's own clean-up was the main cause. Its "mannequin ghost" rule erased grey and navy kit, and its baked-ball rule punched holes in white shirts and socks, then deleted the detached boots and hands. Some cells also touched their edge, so the sprite material's supersampling bled into neighbours. | The atlas pipeline is rebuilt (`build_club_atlas.py`): islands, tidy, a 2-texel margin, and tone matching for white kit. Sprites are drawn at whole-pixel positions. The broken "airplane" celebration frames are no longer used. | `Tools/pipeline/audit_sprites.py`, before and after in `ArtWork/audit_v08/`. Pinholes 29,905 → 687 (those left are in the owner's idles and pack dives, untouched). Edge-touching cells 1,264 → 0. Stray balls 344 → 76. |
| 4 | Goals look ruined | The pack's photo goal had been squashed to half height as if it were flat. The back of the net landed in the wrong place and read as a second, tilted crossbar. | Goals are drawn in code, pixel-art style, in the same projection as ball and players. True proportions; white posts with a grey shade; a see-through mesh net 2 m deep with back stanchions; a shadow; the net bellies out on a goal. `-pfgoalart` shows the old art. | `ArtWork/playtest_v08/goal/before_goal_cam.png` and `after_goal_cam_1.png` |
| 5 | Dribble legs move far faster than the body | The run cycle turned about 4.1 times a second at 6 m/s (each pose on screen ~40 ms). | Cadence comes from the ISS stride model: stride = 0.8 m + 0.44 s × speed, about 1.5 to 1.9 cycles a second. The dribble uses the same cycle, a touch slower. Speed of play is unchanged. | Footage numbers in `docs/ISS_STUDY.md` §2.1 |
| 6 | Runs look abysmal | Partly the cadence (above). Partly the art: each club's run frames were posed independently, so the cycle isn't consistent from club to club or direction to direction. | Cadence fixed. The art itself needs the base-body plan below. | |
| 7 | The keeper doesn't hold the ball properly | The held ball was drawn at a fixed 1 m height and 0.3 m in front. With half-size bodies that is above the head and off to the side. Dive catches left it floating where he had been. | Every ball-in-hands frame of all 279 sets has a hand point measured from its silhouette (`Tools/pipeline/hand_anchors.py`). The held ball is drawn there, in front of him, or behind him when he faces away: holding, catching, scooping, throwing, punting, flying, lying after a dive, getting up, and throw-ins. `-pfoldhold` shows the v0.7 behaviour. | `hold_*`, `land_*` captures |
| 8 | Dives should be proper, old-school ISS | The dive drew a flat sprite at a constant height for 0.35 s, then the get-up. | A quick lunge, a short flat flight on an arc (0.22 s, higher for a high dive), landing full length on the turf for about half a second, then up quickly. A caught ball stays in the gloves throughout. | `dive_*`, `land_*` captures |

## Playtest log (`-pfautopad`, captures)
1. **Before shots:** v0.7 code with `-pfautopad -pfshots`. In `ArtWork/playtest_v08/before/`.
2. **Bug-fix pass 1:** passing, slides, goals, cadence, keeper hands and dives. Short captures after each change (`r1/`, `goal/`).
3. **Playtest 1:** 16 minutes, 45 captures in `p1/`, stats in `playtest1_stats.log`.
   - The auto-player completed 36 to 48% of its passes, up from 7 to 31% in v0.7, but still low.
   - New per-pass logging (`PFPASSLOST`) showed most lost short passes had **no receiver**: the stick pointed at goal with nobody in the cone.
4. **Bug-fix pass 2:**
   - A always finds a team-mate (above).
   - The sprite atlases were rebuilt by the audit (above).
   - Hand anchors were re-measured on the new atlases.
   - Sprites are drawn at whole pixels.
   - The airplane celebration frame was dropped.
   - New `-pfactshots=N` captures of holds, landings and slides.
5. **Re-test:** short passes 85% completed (`pass_breakdown.log`). Holds, landings and slides were checked in close-up (`act1_zoom.png`).

## Balance
Headless AI matches (`-pfsimtest`; the runs are deterministic, logs in `ArtWork/playtest_v08/simtest_*.log`):

| Build | Goals per match | Shots | Pass completion | Note |
|---|---|---|---|---|
| v0.7 (CHANGELOG) | 2.1 | 16 | 65% | |
| First v0.8 build | 0.05 | 0.4 | 74% (224 passes) | The always-find-a-man fallback applied to the AI too, so it passed forever and never shot. Now only your presses use it. |
| + pad-only fallback | 1.0 | 14.9 | 63% | |
| + v0.7 dive travel time | 1.05 | 15.0 | 63% | Faster dive body travel was not the cause; the 0.35 s travel time was kept anyway, with the 0.22 s airborne look. |
| A/B: v0.7 slide rules for everyone | 2.0 | 15.5 | | **Cause found:** the generous new slide let AI defenders win the ball too often. |
| **Final:** generous slides for your player only | **1.73** | **15.9** | **63%** | 60 matches. 3.9 dives and 2.1 dive saves per match. |

The cadence change was also A/B-tested (1.30 goals with the old cadence), so it isn't what moved the scoring.

## ISS Deluxe study
`docs/ISS_STUDY.md` covers:
- frame-by-frame measurements from the owner's video (PAL, 50 Hz);
- research on how the game was made and received, with sources;
- the numeric targets used above.

**What we take and what we don't.** We learn the design: timings, frame counts, proportions, assists, and the palette-swap approach. We don't take the art: nothing is traced, extracted or reused, and the look isn't cloned frame for frame. The owner asked for "as identical as possible". The honest answer is to match the feel and the craft (pace, cadence, pass assist, keeper behaviour, goal proportions) with the owner's own art and PixelLab output. That is what this round does.

## The art question: too much art? Simpler player designs?
**Short answer:**
- Yes, there is too much independently generated art.
- No, the player designs don't need to be simpler or cheaper looking.
- What they need is fewer bodies, each with one consistent, hand-checked animation set, coloured into any kit by code.

**The hypothesis holds, in part.** Weird runs and dribbles came from two things:
1. **The cadence bug (code).** Legs cycled about 2.3× faster than ISS. That is fixed now, at no art cost.
2. **Pose drift (art).** v0.7 generated 279 sets (93 clubs × home, away, keeper). Every pose of every direction is a separate PixelLab still, posed from a guide skeleton the generator only loosely follows.
   - The same run frame differs from club to club. Some "passing" frames have both knees up.
   - 66 airplane and 25 fall cells are broken, and a few slides had a ball baked in.
   - No amount of code timing makes a cycle smooth when its frames don't belong together.

**How 16-bit sports games did it** (`docs/ISS_STUDY.md` §6): one or two base bodies, every pose drawn once with fixed palette slots for skin, hair, shirt, shorts and socks, and kits as palette swaps. That's about 180 to 200 outfield frames and 60 keeper frames in total. Proper Football already has the modern version of this: v0.6's `kit_keyed` atlases, recoloured into any kit (stripes, hoops, halves) by the sprite material. The engine still supports it.

**Recommendation:**
- **Base bodies:** 6 outfield bodies (build × skin tone, with hair variants) and 2 keeper bodies.
- **Animation sets:** each body gets one full set in 5 directions (3 mirrored):
  - run 8, sprint 6, dribble 6, walk 4, idle 2;
  - pass 3, shot 4, header 3, slide 4, tackle 2, fall and get-up 3, throw-in 2, celebrations 4;
  - keepers: ready, shuffle, low dive 3, high dive 3, catch and hold 2, scoop, throw 2, punt 2.
- **Generation:** generated as one consistent set per body, then audited frame by frame against the owner's south idle (dither, texture, shading) like v0.7.
- **Club kits:** colour by code, as in v0.6.
- **What it keeps:** the owner's idle art stays the source of every body.
- **What it gives up:** the per-club sets that hold each club's exact hand-placed kit details beyond colours and pattern, such as a crest pixel.

**Cost** (PixelLab, from v0.7's logs: about $0.15 to $0.20 per 15-pose job):
- 8 bodies × 5 directions × about 4 jobs = about 160 jobs, about $30 for one pass.
- With about three takes per batch to pick the best frames, about **$90 to $120**.
- A **pilot** of 1 outfield and 1 keeper body (about $25 to $35) proves it first.
- **PixelLab balance:** $556.57 at the start of this round, with nothing spent this round.
- The owner's OK is needed before any spend over $50, so this is **not started**.

**Can we manage with what he gave us?** Yes.
- Every body comes from the owner's own south idles, his R08 keeper dives stay (they passed the audit), and his stadiums and fans are untouched.
- The animations he doesn't like are the generated in-betweens, not his art.

## Not done or known issues
- **Run art drift:** the art half of note 6 waits on the base-body decision above.
- **Grey patches:** some remain in white kits where PixelLab shaded them darker than the idle. Tone matching reduces them (see the audit summary).
- **Weak sets:** `rochdale_home` and `derby-county_home` have the most broken cells and should be regenerated, or replaced by base bodies.
- **Stadium near side:** still empty, as delivered (unchanged from v0.7).
