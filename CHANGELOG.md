# Changelog

## v0.2.0 (2026-11-24): true scale, arcade feel

From the owner's first playtest: the players were too big for the pitch, turning slid, everything felt draggy, dribbling feet ran like a rabbit's, and the camera was too far out. Also: no button worked in v0.1.0.

### Fixes
- **No input in v0.1.0.** The input settings were in the wrong config file, so the packaged game ran on the old input system and ignored every key and button. `Config/DefaultInput.ini` now carries them. The controller also forces Enhanced Input itself and reads keys and pad buttons directly as a backstop.

### Changes
- **Players at true size.** A player is drawn 1.82 m tall in the stadium's own projection, so players and pitch share one scale. Before, they were about 1.6 times too big.
- **Camera closer.** About 35 m of pitch across (was about 40 m), close to the owner's kick-off reference framing. Options now has **CAMERA: NEAR / MEDIUM / FAR**.
- **Arcade movement:**
  - The player goes where the stick points at once. No more drifting sideways through a turn.
  - Pad and keys move in 8 screen directions, matching the 8-direction sprites.
  - Top speed comes in 0.17 to 0.6 s depending on acceleration (was up to about 2 s).
  - A full about-turn takes 0.06 to 0.16 s depending on agility.
  - Agility now decides how much speed survives a sharp cut.
  - Players stop in about a quarter of a second.
- **Snappier actions.** Passes and shots make contact sooner. Players keep most of their speed while passing. The slowest animation rate went from 0.72x to 0.95x.
- **Cleaner running.** The run cycle runs on a smooth timer with a realistic stride length (longer strides when faster), so frames never skip and feet never blur. The sprint pose shows only while actually sprinting, so it no longer flickers.
- **Scripted dribbling.**
  - The ball is touched on the run cycle's foot-contact frames, rolls out, and comes back to the next contact.
  - Good close control touches it every step; heavy-touch players and sprinters touch it every other step and further ahead.
  - A sharp cut at speed can lose a poorly controlled ball.
  - Defenders can nick a heavy touch.
- **Match AI rebalanced for the new feel:**
  - Defenders tackle as soon as they are in reach (after their reaction time) and keep the ball when they win it.
  - Back-line players mark the nearest attacker goal-side in their own half.
  - Shots can be blocked.
  - Keepers close down a dribbler and smother at his feet.
  - AI attackers shoot from close in rather than walking the ball in, and change direction at a human rate.
  - Headless test: 5.5 goals and 37 shots per AI match, no stuck restarts.

### Bug fixes
- Sent-off players came back on: they could take kick-offs, receive passes, win headers and be switched to. They now stay off.
- The second half was kicked off by the wrong team after certain goals.
- A restart with no available taker could hang the match.
- Keepers holding the ball on their line could carry it over it and concede.
- Career: your cup ties were simulated instead of played when they fell on a league matchday. You now play both.
- Career: quitting a match mid-game kept the score at that moment. The rest of the match is now simulated.
- Career: several bids at once could take the transfer budget negative. Pending bids now count, deals re-check the money, and AI buyers pay for your players.
- Career: the fixtures list coloured the wrong results.
- Career: after a sacking, your old club's contracts and results were still treated as yours.
- Career: youth-match kids were matched by name, so a same-named adult could be added to your prospects.
- Career: the save file is checked for its version before loading, and a bad file no longer half-loads.

### Known issues
- AI v AI matches are a little high-scoring (about 5.5 goals per 5-minute match). Balance continues in v0.3.
- Sprite pixels are no longer an exact multiple of screen pixels at 1080p (players are drawn at true size), so a few pixel rows are one screen pixel thicker than others.
- One 15-pose batch for body O29 is still missing (it falls back to its neighbouring direction).
- No human pad playtest yet of the new feel.

## v0.1.0 (2026-11-24): first playable

The first build has every pillar in rough form. Expect rough edges; tuning comes from playing it.

### New
- **Project:** Unreal Engine 5.8 C++ project with Git LFS for art and Unreal binaries. The owner's nine art packs are in `ArtSource/Packs`, untouched.
- **Look:** the owner's 93 stadiums (daylight, evening, floodlit) as the pitch, with a close diagonal camera pulled out a touch from the SNES classics (about 40 m across). Crisp pixels everywhere: nearest sampling, no AA, no bloom, no motion blur, no tone mapping.
- **Sprite pipeline (PixelLab static poses only):**
  - 25 base characters: 16 outfield, 4 keepers, a referee, a linesman and 3 managers, from the owner's idles.
  - 8-direction rotations of each idle.
  - Up to 46 poses per character, each a single still drawn from a skeleton defined in our own code. No PixelLab animation.
  - Automatic QA: alpha, stray pixels, baked-in balls, doubled figures, size, palette clamp. Contact sheets for review.
  - A wave 2 adds 16 more bodies and 4 more keepers with all 8 directions generated natively.
- **Kits:** one pose set serves every club. Template-kit pixels are recoloured at run time into each club's home, away and keeper kits, including stripes, hoops and halves.
- **Match:** 11 v 11 with AI, keepers that position, dive, catch and parry, and headers.
  - Slide and standing tackles; fouls, free kicks, penalties, yellow and red cards.
  - Throw-ins, corners, goal kicks, kick-offs, half time and full time.
  - Posts and crossbar.
  - Referee and two assistants who follow play, whistle, show cards and raise the flag.
- **Talent you can see:** pace, acceleration, agility, ball control, passing, shooting and reactions map onto very wide, non-linear differences in speed, turning, touch, accuracy and reaction time. Animation speed scales with pace and agility. Run cycles advance with distance covered, so feet stay planted.
- **Controls:** pad first (pass, lob, through ball, hold-to-power shot, slide, switch, sprint), keyboard fallback. Control follows the ball to the receiver, and the defender switches automatically when the opposition breaks away.
- **HUD:** pixel-font scoreboard with crests, clock, nameplates, shot-power bar, minimap, event banners.
- **Career:**
  - Endless seasons across five English tiers (Premier Division to National League, 116 clubs; the owner's 93 plus generated non-league sides). Fictional names.
  - Two cups.
  - Board confidence, mid-season sackings, automatic sacking on relegation, reputation and job offers every summer.
  - Transfers with AI clubs bidding against you, player choice by wage and stature, transfer windows, listing and selling.
  - Youth academy intakes, worldwide regens, retirements and ageing.
  - Scouting network across 12 world regions with hidden stats; watch youth matches and sign the kids who stand out.
  - Play every match, manage it (8x "dots" sim that slows to full sprites for highlights), or take an instant result.
  - Local save file.
- **Menus:** title, main menu, friendly setup, pause, results, and the full career office (squad, player card, tactics with the owner's club shirts, tables, fixtures, transfers, academy, scouting, prospects, club and board, history, job offers). All pad-driven, on the owner's broadcast backgrounds.
- **Audio (PixelLab):** kicks, headers, bounces, net, post, catches, slides, three whistles, crowd roar, "ooh", groan, crowd ambience, menu blips, title and menu chiptune loops.
- **Tooling:** data build, atlas build and QA, PixelLab production runner (resumable, parallel, budget-capped), headless match test (`-pfsimtest=N`), headless career test (`-pfcareertest=N`), screenshot capture.

### Known issues
- Not yet tuned against a human holding a pad: shot power, pass assist and switching need a playtest.
- A few PixelLab poses still carry small artefacts; QA drops the worst, and the game falls back to a neighbouring direction or the idle.
- Managed matches pick the best XI automatically; no manual lineup or substitutions yet.
- No contract talks yet (user contracts auto-renew), no playoffs, no injuries shown in-match.
- Generated non-league clubs have no crest art (a coloured badge stands in).
