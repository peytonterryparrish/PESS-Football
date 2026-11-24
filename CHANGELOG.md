# Changelog

## v0.5.0 (2026-11-24): ISS framing, dithered pitch, hard shadows

From the owner's reference shots of ISS Deluxe: the camera needed to be much closer, and the pitch needed the same dither, shadows and contrast.

### Changes
- **Camera much closer.** NEAR, the new default, shows about 14 m of pitch across at 1080p, and every sprite pixel is exactly 2×2 screen pixels, so players are crisp and big on screen (about 128 px tall at 1080p). Players stay at the half size relative to the pitch that v0.4 set. MEDIUM is 1.5:1 and FAR is 1:1 (about 28 m across). Saved camera choices from earlier versions are reset to NEAR.
- **Dithered ISS-style pitch.** A new pitch material (`M_PFPitch`, made by `Tools/ue/make_assets.py`) draws the grass in pixels the same size as the sprites' pixels. It uses clustered dark and light speckle over high-contrast mowing stripes, with white markings to real dimensions. The pattern is locked to the world, so it doesn't swim when the camera pans. Sunset and floodlit tints come from `Tools/pipeline/pitch.py`. It replaces v0.4's smooth pitch image, which has been removed.
- **Hard shadows.** Player and ball shadows are darker, hard-edged and cast toward the lower right, like ISS.
- Goal net strands are one art pixel wide, so they match the pixel scale.

### Known issues
- At this zoom the stands and the sand behind the goals (still the plates' upscaled art) look very blocky when they're on screen.
- Lying, diving and other horizontal keeper poses still use stand-ins.

## v0.4.0 (2026-11-24): half-size players, HD pitch and goals

From the owner's playtest of v0.3.0: the players were still too big, the cameramen and coaches were too big and stood in the stands, and the pitch looked low resolution.

### Changes
- **Players at half size.** Everything with height is drawn at half the stadium projection's height scale (`FPFMatch::HeightScale = 0.5`): players, officials, coaches, crew, shadows, the ball's flight and the goals. They stay consistent with each other; a header still meets the ball at head height. A 1.8 m player is now about 20 stadium pixels tall (about 50 screen pixels at 1080p on the default camera). Gameplay distances are unchanged.
- **Smooth sprites below 1:1.** The sprite material now takes 3×3 samples per screen pixel after the kit recolour, so half-size pixel art is smoothed instead of dropping rows.
- **New HD pitch.** The stadium plates' grass was 1280×720 art upscaled 3×. A new ground-space pitch is drawn over it, made by `Tools/pipeline/pitch.py` at 32 px per metre: ISS-style mowing stripes across the pitch (20 between the goal lines), fine grass grain and crisp anti-aliased markings to real dimensions. It is tinted per lighting stage (daylight, sunset, floodlit) from the plates' own grass.
- **Goals drawn in code.** Posts, crossbar, net mesh, rear frame and ground shadow are drawn in screen space at full resolution and at the same height scale. The posts and crossbar are depth-sorted with the players.
- **Crew and coaches off the stands.** The far touchline backs straight onto the stand, so nothing stands there now. The coaches stand in technical areas on the near-side track, and the cameramen stand behind each goal and on the near track. The far-side assistant referee runs just inside the far touchline.

### Known issues
- Lying, diving and other horizontal keeper poses still use stand-ins.
- The stands and the sand behind the goals are still the plates' upscaled pixel art.

## v0.3.0 (2026-11-24): art gaps closed, kit polish, feel test

### Changes
- **Every body complete.** The missing 15-pose batch for body O29 was regenerated (PixelLab static poses), so all 32 outfield bodies have every pose in all 8 directions.
- **No more green specks.** Blended template-green pixels at the shorts' edges used to keep their raw green in every kit. They are now part of the shorts and take the club's colour. All 45 atlases were rebuilt.
- **More build-up play.** AI players pass more in midfield and under pressure, instead of running at goal. AI matches average about 5.3 goals.

### Tooling
- **`-pffeeltest`:** a headless control test. It drives the human player with a scripted stick on an empty pitch and times top speed, reversal, facing and stopping, with and without the ball, for a slow and a fast player. Today's numbers:

  | | Slow player | Fast player |
  |---|---|---|
  | Top speed | 4.5 m/s in 0.28 s | 8.6 m/s in 0.23 s |
  | Reversing direction | moving back in one 120 Hz tick, facing back in 0.11 s | moving back in one tick, facing back in 0.07 s |
  | Stop | 0.15 s | 0.28 s |

- **`-pfsimtest`** now also reports passes, blocks, keeper dives and saves, and how each goal was scored.

### Known issues
- Lying, diving and other horizontal keeper poses still use stand-ins. They need a different generation method, planned next.
- Sprite pixels are drawn at true size, so at 1080p they are not an exact multiple of screen pixels.

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
