# Changelog

## v0.9.3 (2026-11-27): aim where you point

- **Shots go where the stick points:** the ball is aimed where the stick's line meets the goal line (near post, far post, anywhere between), kept just inside the posts; no stick aims at the far corner. The pad's random shot error is under half what it was, scaled by the shooter's rating. Aim test: 9 shots from three spots all land within about a metre of the target (75-rated shooter).
- **Passes go where the stick points:** the target is picked by the angle on screen, as the controller sees it, inside about 37 degrees, before anything else.
- **True 8-way movement:** the controlled player faces and runs along the chosen direction at once (no curving through a turn); reversing still costs speed.
- **Keepers dive:** only a ball straight at him (under 0.45 m away) is caught standing; anything wider is a dive, and the passive body block only fires on a ball that hits him. Save test: 18 corner shots, he dived at all 18 (caught 4).
- Balance: 20-match sim 2.20 goals/match (dives 3.4 per match, standing saves 1.5), 0 stuck.

## v0.9.2 (2026-11-27): realism, ratings and called runs

**Movement and animation**
- Realistic 12-frame run, sprint and dribble cycles for all five outfield bodies (PixelLab, $1.51 + B01's two takes), gait-data angles instead of v0.9's exaggerated set. `-pfarcade` brings the arcade cycles back.
- Players 40% smaller against the pitch (v0.9.1), game speed 1.05 (was 0.85).
- Idle players stand calmly: they settle on their spot and stay there, the breathing frame shows briefly every few seconds, and the drawn view only changes once the facing is clearly in the next direction.

**Play**
- Keepers time their dives: only at real shots, they shuffle across while the ball travels and leave the ground just in time to arrive with it.
- **LB with the ball** sends the best-placed team-mate on a forward run; **Y** plays the through ball into his path. Through balls go further (10 m + ahead of the run) and die into the space.
- A much wider gap between bad and great players (speed 3.0–9.9 m/s, touch, passing, shooting, reactions, tackling), a ±12 spread inside each squad, and poor passers misplace some assisted passes.
- Ball physics closer to real turf: gentler rolling deceleration, livelier bounce.

**Ratings on screen:** each player's rating over his head (red under 55, amber to 69, green 70+), on the name bars, and both teams' ratings beside the clock.

**Camera:** WIDE is the default (owner's pick).

**Stadium fixes**
- The camera no longer scrolls over the bare concrete below the near touchline.
- The high-detail grass only paints where the stadium art is grass (no more green over the track, boards and stand bases).
- Sunset no longer turns the pitch mustard.
- Known: some stadium art has dark green specks on the track at night; left for the 3D stadium work.

**Tests:** 30-minute recorded autopad session (6 matches, 20 clips), no errors; 20-match sim 1.95 goals/match, 0 stuck.

## v0.9.0 (2026-11-26): the feel pass

The owner rated v0.8.1 "2 or 3 out of 10". This round goes after the reasons, measured against the ISS Deluxe study.

**Scale and camera**
- Players are drawn at true size against the pitch (they were half size), and the camera pulls back to match. Sprites are as big on screen as before, but you see about 28 m of pitch instead of 14 m. The pitch is half as many screens long, and speeds match ISS for a player's height. `-pfscale=0.5` is the v0.8 framing.
- Six camera zooms in Options (CLOSEST, CLOSER, STANDARD, WIDE, WIDER, WIDEST). Comparison shots: `ArtWork/camera_v09/compare_A.png` and `compare_B.png`. `-pfcam=0..5` picks one; `-pfcamsweep` captures all six.

**Play**
- **Locked passes.** Once you pass, the receiver ignores the stick and runs onto the ball himself until he has it (lofted balls: to where they come down). A pass or shot pressed just before it arrives is still played first time.
- **Taking a man on.** You dribble at 93% of running speed (was 80%). Defenders jockey instead of sprinting into you. A tackle from behind, or on a man shielding the ball, usually fails, and a missed tackle leaves the defender beaten for a moment. A loose touch can no longer be nicked almost at once (it was 5% per 120 Hz step).
- **Movement weight.** An about-turn costs speed and takes about 0.1 s through the in-between directions (it was a one-frame flip).
- **Shooting.** 23–38 m/s at full power (was 17–33), a firm tap, quicker charge, and a freeze-frame and camera kick when you strike it.
- **Passing.** ~22 m/s (ISS), up from 12–16.5.

**Animation (PixelLab, all seven bodies)**
- Slide tackle: drop, a flat slide with the leg out (held 0.38 s), hand down, crouch up, with turf and dust thrown up behind.
- Shot: big back-swing, high follow-through. Tackle: lunge and poke.
- Run, sprint and dribble: harder lean, high back heel, bigger arm drive; the dribble is bent over the ball.
- Keeper dive: crouch, thrown take-off, reach, then the owner's flat R08 flight and landing, with turf where he lands; no get-up, he snaps to ready (ISS).

**Crowd**
- The owner's 20 supporters got a 3-frame cheer and a 3-frame "no!" (PixelLab). On a goal the scorers' fans cheer and the others put their hands on their heads; a save or the woodwork makes the shooting side's fans clutch their heads.

**Bug fixes**
- Kit clash: striped kits against reversed stripes (Northcalder v Saltmarsh) no longer clash; 1.2% of pairings that clashed with both away kits get a plain change kit.
- Fans were drawn twice too big once players went true size.
- Dust uses its own random numbers, so `-pfsimtest` results stay comparable.
- The playtest bot carries the ball in when free (it passed every 2.2 s and almost never shot).

**Tests:** 20-match sim 1.55 goals/match, 0 stuck; pad side 84% passes completed in an autopad match; feel test reverse 0.11 s, passes answer every press.

**Cost:** about $16 on PixelLab across the round (fans $3.95, animations ~$8.5 of our jobs; the rest of the balance drop was the medieval project running at the same time).

## v0.8.1 (2026-11-25): base bodies are the default

The art plan from v0.8's status report, now in the game. Every player is drawn in one of seven hand-checked base bodies, and the sprite material colours in his club's kit (stripes, hoops and halves included). Each body has one consistent animation set, so runs, sprints and dribbles cycle properly in every direction.

- **Seven bodies.** Five outfield bodies with different builds, skin tones and hair, and two keepers. All come from the owner's own south idles and the v0.6 template bodies.
  - Each outfield body has 273 frames in 5 drawn directions (3 mirrored): run 8, sprint 6, dribble 6, walk 4, slide 4, tackles, falls, passes, shots, volley, headers, chest, celebrations and throw-in.
  - The keepers have their set position, shuffles, catches, scoop, punch, throw, punt, dive starts, run and walk, plus the owner's six audited flat dives from the R08 pack.
- **Each player keeps his look.** His template body maps to the nearest base body by skin tone and hair, so a squad looks the same match to match.
- **Generated the v0.8 way:** one PixelLab job per animation, frames in playing order, natural gait angles, three takes, with the cleanest take picked per direction and animation. Every direction was audited by eye.
- **Better clean-up** (`Tools/pipeline/build_base_atlas.py`):
  - Grey and purple leftovers inside a kit area become that kit.
  - Pinned takes (`ArtWork/base_pilot/picks.json`) replace one with a ball stuck to the boot.
  - A sash that one body's back views grew, which isn't in its front design, is painted out.
- **Held back: B06** (ginger mop). Its hair is a different colour in the front view than in the others, and its standing turns have a hair wisp the moving frames don't have. Those players use B02, the other golden-tan body.
- **`-pfclubsets`** brings back v0.7's per-club kit art. Those 279 sets stay in the build for now.
- **Cost:** $27.40 of PixelLab credit for all bodies, pilot included.

## v0.8.0 (2026-11-25): assisted passing, real slide tackles, proper goals, ISS run cadence, the ball in the keeper's hands

From the owner's notes on v0.7.0. Timings come from a frame-by-frame study of the owner's ISS Deluxe reference video (`docs/ISS_STUDY.md`). We match the feel and craft with Proper Football's own art; nothing from that game is copied.

### Passing (owner: "doesn't feel good at all")
- **A always finds a team-mate.** If nobody stands in the stick's direction it widens the search, then takes the nearest man, instead of kicking the ball into empty grass (most of v0.7's lost passes).
- **Assisted, predetermined passes.** A short pass (A) picks the team-mate, aims where he'll be when the ball arrives, and travels at a steady, snappy speed (12 to 16.5 m/s) with no slow roll-in. It is steered onto his feet in flight and killed dead there.
  - A pass from your side is never misplaced.
  - AI passers keep about a third of their usual error.
  - The ball turns loose the moment it is touched, deflected or lifted, or if the receiver runs away from it.
- **The receiver checks his run** and steps toward the ball instead of chasing its path. The one you control does this when your stick is idle.
- Through balls (Y), crosses and lofted passes (X) are unchanged: they still go into space.

### Slide tackles (owner: "don't work, no animation")
- **They work.** Your slides: with the ball within 5 m, a slide locks onto where it will be, and the boots reach a little further. A side-on slide that catches the man after the ball is a foul less often. From behind it is still almost always a foul. AI slides keep v0.7's contact rules (the generous version halved the goals in AI matches).
- **They show.** Slides are drawn side-on, as the front and back views read as a man standing with his arms out. They go slide, then sitting on the turf with the leg out, then up: 0.73 s in all, about 2 m, as in the ISS study.

### Goals (owner: "ruined, terrible")
- **New goals drawn in code, pixel-art style,** in the same projection as the ball and players, so posts, bar and net always agree with the physics:
  - True proportions: 7.32 m wide, posts 2.44 m at the players' half height.
  - White posts with a grey shade, and a see-through mesh net 2 m deep with grey back stanchions.
  - The frame casts a shadow, and the net bellies out when a goal goes in.
- The v0.7 photo goal had been squashed to half height as if flat, which threw the back of the net into the wrong place. `-pfgoalart` still draws it, for comparison.

### Running and dribbling (owner: "legs move far too much", "runs look abysmal")
- **Cadence from the ISS study.** Speed comes from longer strides, not faster legs:
  - 1.5 to 1.9 steps-pairs per second (was about 4); the dribble uses the same cycle, a touch slower.
  - The speed of play is unchanged.
  - `-pfcada=` / `-pfcadb=` tune it.

### Goalkeepers (owner: "the ball isn't held properly", "dives need to be proper")
- **The ball is always in his hands.** Every ball-in-hands frame of every keeper (and every throw-in) has a measured hand point (`Tools/pipeline/hand_anchors.py`, 279 sets). The held ball is drawn there, in front of him, or behind him when he faces away. Before, it hovered at a fixed height that didn't match the half-size bodies.
- **ISS-style dives.** A quick lunge, then a short flat flight on an arc (0.22 s, higher for a high dive). He lands full length on the turf for about half a second, then he's up. A caught ball stays in his gloves throughout.

### Sprites (owner: "pixel bleed, missing pixels, grey spots")
- **Most of the damage was our own clean-up.**
  - v0.7's "mannequin ghost" rule erased real grey and navy kit.
  - Its baked-ball rule punched holes in white shirts and socks, then deleted the boots and hands it had cut off.
- **Both rules are now careful** (`Tools/pipeline/build_club_atlas.py`):
  - Detached hands and feet are kept; specks, balls, grey crumbs and stray fragments are dropped.
  - 1–2 px holes are filled, alpha is hard, and every cell keeps a 2-pixel margin so the sprite material can't bleed into its neighbour.
  - White and light kit is tone-matched to the owner's south idle before the palette snap, so it no longer turns grey.
  - The owner's south idles are untouched (byte-identical).
- **Sprites are drawn at whole-pixel positions.**
- **All 279 sets rebuilt.** Audit (`Tools/pipeline/audit_sprites.py`, `ArtWork/audit_v08/`):
  - Pinholes 29,905 → 687 (those left are in the owner's own idles and pack dives).
  - Cells touching their edge 1,264 → 0.
  - Stray balls 344 → 76 (mostly raised gloves).
  - Grey patches cut back to the level of genuine grey kit.
- **The airplane celebration frames** (66 broken cells) are no longer used.

### Testing
- **Playtest** (`-pfautopad`, 16 minutes, 45 screenshots, then re-tests; log in `docs/STATUS_v0.8.md`): the auto-player's short passes went from 36% to **85% completed**, 75 of 94 to the intended man.
- **Balance** (60 AI matches): 1.7 goals, 15.9 shots and 63% of passes completed per match (v0.7: 2.1, 16 and 65%).
- **`-pfactshots=N`:** screenshots of keepers holding the ball, keepers landing after a dive, and slides.
- **`-pfoldhold` / `-pfgoalart`:** draw the v0.7 held ball or goal, for comparison.
- **New counters:** `PFPASSLOST` logs every lost pass of yours, and `PFPLAY` lines now count short passes.
- `Tools/pipeline/audit_sprites.py` audits every atlas cell, `hand_anchors.py` measures the hand points, and `contact.py` tiles screenshots.

### Known issues
- **Run art:** individual run frames still vary from club to club, because each club's set was generated separately. Fixing that needs the base-body plan in `docs/STATUS_v0.8.md`, which needs the owner's OK to spend.
- **Weak sets:** `rochdale_home` and `derby-county_home` have the most broken frames.
- **Stadium near side:** still empty, as delivered.

## v0.7.0 (2026-11-25): every club in its own kit, the Laws of the Game, your stadiums, fans and goals

From the owner's notes on v0.6.0 and on the v0.7 work in progress.

### Players
- **Every club plays in its own kit.** Each club gets one player in its home kit, one in its away kit and its keeper, generated from the owner's south idles. The poses are new and exaggerated, ISS-style: high knees, big arm pumps, huge backswings and follow-throughs (`Tools/pixellab/poses_x.py`, `produce_clubs.py`). Clubs without a set yet use the shared bodies.
- **Same art in every direction.** Every frame uses only the colours of the owner's south idle for that player. The idle's speckled grain is added back until each frame's texture measures the same (`Tools/pipeline/build_club_atlas.py`).
- **Bugs fixed:**
  - Grey ghost legs left over from the pose generator ("players standing in their legs").
  - Footballs drawn into frames.
  - Big hairstyles turned into a block floating above the head; those players are no longer used as animated bodies.
- **Keeper dives fly flat.** They use the six dive and smother frames that passed a strict audit of the owner's R08 action pack, in each club's keeper kit. The pack's outfield actions failed the audit (scrambled legs) and are not used.

### Rules (docs/RULES.md)
- **Keepers:**
  - A keeper handles the ball only inside his own area.
  - Nobody may challenge a keeper holding the ball. Opponents run out of the area, the one you control included.
  - Holding it for more than 8 seconds gives the other side a corner.
  - The back-pass rule applies.
- **Offside:** given when the ball is played, flagged by the assistant on that half.
- **Restarts:** opponents stand 9.15 m from free kicks and corners and 2 m from throw-ins, and defenders form walls. Goal kicks are kicked off the ground.
- **Cards:** a second yellow is a red, and denying a clear goal-scoring chance is a straight red.
- **Goals that don't count:** straight from a throw-in, or straight from an indirect free kick.

### Controls (owner's layout)
- **Buttons:** A short pass, B shoot (hold for power), X cross, Y through ball, LB + B chip.
- **Right-stick skill moves:** flick left for a feint, flick right for a step-over that knocks the ball right. Nearby defenders may commit the wrong way.
- **Aftertouch curl:** push the stick across a shot, cross or chip just after striking it to bend it.
- **Your keeper:** you control him when he has the ball. In his hands he can carry it inside his area, and throws, rolls or kicks it out of his hands.
- **Throw-ins:** rings mark your three nearest team-mates. Point at one, then press A.
- **Off-screen arrows:** while your side has the ball, arrows at the screen edge point to team-mates you can't see.

### Ball and passing
- **The ball stops.** It now has real rolling friction, so a loose ball never rolls for long. In the playtest the longest roll was 4.7 s.
- **Passes arrive.** They're weighted to reach the receiver still rolling, aimed just ahead of him, and he runs onto the ball's path. The receiver you control does this when your stick is idle. A pass can't be cut out in its first 2.2 m. AI pass completion went from 28% to 65%.
- **Goals stay in.** The ball stays in the net, and the net bulges back and springs into shape.

### Stadium
- **Your stadium images,** shown exactly as provided. Only the pitch is redrawn.
- **Your supporters** sit in every ground's own seats, at the players' scale, in the home club's colours, and stand up for goals. They never stand on the track or by the advertising boards.
- **Your goals:** the pack's goal art, at half height to match the players. The posts stand about 1.3 player heights, as in real life.
- **Grass with depth and light:**
  - The far stand's shadow, long and cool at sunset.
  - Brighter pools toward the floodlights.
  - Worn goalmouths and centre spot, and grass blades that catch the light differently on each stripe.
  - The centre and penalty spots are now real size.

### Balance (headless AI matches)
- 2.1 goals and 16 shots per match, with 65% of passes completed (v0.6: 4.0 goals and 26 shots).

### Testing
- **`-pfautopad`:** an auto-player plays your side through the real controls, match after match, logging pass completion, roll times and keeper crowding. A 30-minute run with 90 screenshots found the low pass completion, the slow exit from the keeper's area and the oversized spots, all fixed above.
- **`-pfhome=` / `-pfaway=` / `-pfstage=`:** pick the clubs and the kick-off time.
- **`-pfcamx=` / `-pfcamy=`:** hold the camera on a point.
- **`-pfdiveshots=N`:** screenshot keeper dives.

### Known issues
- The near side of the stadium images is empty, with stray wall panels, as delivered.
- Throw-ins are rare in AI play: the AI seldom puts the ball out.
- Second-choice keeper kits use the shared keeper bodies.

## v0.6.0 (2026-11-24): passing fixed, ball at the feet, ISS Deluxe pace and pitch

From the owner's playtest of v0.5.0: passing was unresponsive, the dribble ran too far from the feet, turns needed to be quicker, the game must be single player and offline only, and the look and flow should follow ISS Deluxe as closely as possible, studied frame by frame from real footage.

### Fixes
- **Passing was unresponsive.** The match runs at a fixed 120 Hz, and a button press only counted on the frame it happened. Above 120 fps (full screen) many frames run no match step, so most presses were thrown away. Presses are now carried to the next step and buffered for 0.22 s. A press during a turn or trap still counts. The ball leaves the boot on the press, and the clip just plays out. A pass pressed as a teammate's ball arrives plays a first-time pass instead of switching player. The feel test measures it: 20 of 20 single-frame presses at 300 fps passed, with the ball away within 0.01 s.
- **The dribble ran too far ahead.** It was tuned in metres for full-size players, then players were drawn at half height. The ball is now touched every step and stays 0.24 to 0.40 m ahead (0.26 to 0.44 of a player's drawn height), the same range as in the footage. It swings round with the body at once on a turn.
- **Turns.** The controlled player faces the new direction at once and keeps most of his speed through a cut (72 to 97%, by agility). AI players keep their v0.5 turning so match balance is unchanged.
- **Single player, offline only.** There was no online or multiplayer code; friendlies were already HOME, AWAY or WATCH with one human. The engine's online subsystem plugins are now disabled in the project too.

### ISS Deluxe look and flow (see docs/ISS_STUDY.md)
- **Pace.** Measured from the footage, ISS players run about 5 to 6 of their own heights per second and passes travel about 12. With the half-size players, ours ran about 6.8 and 18. A global `GameSpeed` of 0.85 brings them to about 5.8 and 15. Halves keep their real-time length.
- **Run cycle.** Stride length is scaled to the half-size bodies, so a two-step cycle takes about 0.3 s at a jog, as in the footage (0.27 s).
- **Pitch.** Narrow diagonal mowing bands (0.5 m, about 0.4 of a player's height) in the grass colours sampled from the footage, a bright yellow-green, over the same dithered speckle.

### Balance
- AI matches: 4.0 goals and 26 shots per match (v0.5: 5.3 and 35).

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
