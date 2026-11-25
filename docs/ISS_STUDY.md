# ISS Deluxe study (v0.8)

The owner wants Proper Football to have the feel and craft of International Superstar Soccer Deluxe (SNES, 1995). I studied his reference video frame by frame and researched how the game was made. This file records what I measured and what Proper Football should take from it.

**Hard rule.** We learn design only: timings, frame counts, proportions, assist logic, palette approach. Nothing from the game is copied, traced, extracted or committed. Its art, frames, logos and music stay out of the repository. The study frames are local working material, never shipped. They live in `Saved/ISS_Study/v08/`, which is git-ignored, and this file doesn't show any of them.

Every finding below carries one of three tags:
- **[M] Measured:** from the video, in this study.
- **[R] Reported:** a published source says it. The URL is given.
- **[I] Inferred:** my deduction from [M] and [R].

---

## 1. Method

- **Source video.** The owner's file `videoplayback (7).mp4`: 14:15 long, 640×360, 30 fps. It covers the title screens and menus, then a full Argentina v Brazil match (3:18 to 13:18 of the video), with five goals, replays, a corner, a throw-in, a goal kick and half-time.
- **Which version.** The title screen reads "©1995 1996 Konami" and "Developed by Konami Computer Entertainment Osaka" [M]. The 1996 date points to the European release [I].
- **50 Hz source.** The motion cadence confirms the game ran at 50 Hz [M]:
  - During steady camera pans, the frame-to-frame difference runs in a repeating high, high, low pattern. That is what 50 game frames squeezed into 30 video frames produces (steps of 2, 2 and 1).
  - The v0.6 capture, at 60 fps, showed the matching 1-in-6 repeat.
  - So one video frame is about 1.67 game ticks. All times below are in seconds, measured at PAL speed. The NTSC original may run up to about 20% faster [I].
- **Picture geometry.** The video stretches the 256×224 SNES picture to 16:9.
  - Every frame was rescaled to 512×448: 2× native pixels, with the correct aspect ratio and nearest-neighbour sampling.
  - The ball and the HUD font come out square and round, which confirms the rescale.
  - All pixel figures below are native SNES pixels.
- **Tools.**
  - **Frame extraction:** ffmpeg 9.
  - **Contact sheets:** Python and Pillow. One frame every 3 s for the overview, then every frame (30 fps) on the key moments, zoomed 2.5× to 6× and labelled with frame number and time.
  - **Camera motion:** phase correlation of the grass between consecutive frames.
  - **Player position:** colour-blob tracking of the shorts.
  - **Cycle period:** pose self-difference against lag, taken from a crop of the legs that follows the player. The best-matching lag is the cycle.
- **What I could not see.** The video shows no controller, so pass targeting is reported or inferred, never measured.
  - No high flying save happens in this match; only low and smothering dives do.
  - Colour counts per sprite can't be measured through video compression.
- **Study files.** All of them are in `Saved/ISS_Study/v08/` (about 540 MB):
  - Overview sheets: `ov_00` to `ov_07`.
  - Per-frame sheets: `A_*` (kick-off and pass), `K_*` (run, dribble, slide, keeper, goal), `S_*` and `G_*` (keeper dives), `C_*` (catch and hold).
  - The goal measured on a native-pixel grid: `goal_grid.png`.

## 2. Measurements

**Scale [M]:**
- A player is about **33 px** from hair to soles (±2 px; seven frames measured), 15% of the 224-line screen.
- **"h" means one player height.** Proper Football's player is 1.82 m tall, so 1 h = 1.82 m at body scale.
- The pitch is drawn smaller than the players. The centre circle (18.3 m across) is 216 px wide, which is 11.8 px per metre along the pitch.
- A 1.82 m player at that scale would be 21.5 px. So ISS draws players **about 1.55× larger than the pitch**, a standard 16-bit cheat for readable sprites.

### 2.1 Running and dribbling [M]
| What | ISS Deluxe |
|---|---|
| Directions drawn | back, front, side, and both three-quarter views seen; left/right are almost certainly mirrored (SNES sprites flip in hardware) [I] |
| Run cycle period (two steps) | **0.53 s** (16 video frames, ~27 ticks) at 152 px/s; **0.60 s** (18 frames) at 88 px/s |
| Cadence | **1.88 cycles/s at 4.6 h/s**; **1.67 cycles/s at 2.7 h/s**. Cadence rises only ~13% while speed rises 70%: **speed comes from stride length** |
| Stride (two steps) | 2.45 h at top speed; 1.6 h at a jog |
| Distinct poses per cycle | about **6 to 8** (stride, contact/passing, push-off, for each leg) |
| Hold per pose | about **2 to 3 video frames**, so 3 to 5 game ticks (0.07 to 0.09 s) |
| Top observed running speed | **152 px/s = 4.6 h/s** (12.9 m/s at pitch scale, 8.4 m/s at body scale) |
| Jog (off the ball) | 88 px/s = 2.7 h/s |
| Dribbling | Same cycle (0.53 s) at 148 px/s: running with the ball barely slows a player when he is running straight. The kick-off dribble, with turns, was ~93 px/s. |
| Ball while dribbling | Stays at the front foot, never out of stride (matches the v0.6 finding of 0.3 to 0.7 h ahead). |
| Look of the run | Long, athletic strides with a forward lean. Both feet are off the ground in the wide-stride pose, with a high rear-heel kick and arms pumping. |

### 2.2 Short pass [M]
Measured on the kick-off move, a 4.8 h pass to a standing team-mate.

| What | ISS Deluxe |
|---|---|
| Wind-up | Plant foot, 2 frames; strike pose with arms out, 2 frames; ball leaves on the next frame. **About 4 frames = 0.13 s** from plant to release. Follow-through 1 to 2 frames. |
| Ball speed | **8 px per game tick = 400 px/s ≈ 12 h/s**, nearly constant over the pass. The camera stepped 32, 32, 16 (2×) per video frame, the 2-2-1 tick pattern. |
| Flight | Low. The ball's shadow sits about one ball-width below it. |
| Distance and time | ~135 px (≈4 to 5 h) in **11 frames = 0.37 s** |
| Arrival | Exactly at the receiver's feet. He did not move to meet it; the ball **stopped dead at his feet on arrival** (trapped in 1 frame). He stood in a receive pose for ~8 frames (0.27 s), then played the next pass. |
| Long ground ball (into space) | The camera tracked it from ~260 px/s down to ~50 px/s in about 1.6 s, a deceleration of ~130 px/s² (≈4 h/s²). It is camera-derived, so it reads low early while the camera catches up. |
| Lofted ball | A small **"×" marker appears on the grass where a high ball will land** and stays there until the ball comes down. |

### 2.3 Slide tackle [M]
One clean side-on slide, from 8:46.8 in the video.

| Phase | Frames (30 fps) | Time |
|---|---|---|
| Drop into the slide | 1 to 2 | 0.05 s |
| Slide, one held pose: leading leg out, trailing leg tucked, one arm back, body almost flat | **10** | **0.33 s** |
| Down, hand on the grass (3 frames), then a rising crouch (6 frames) | 9 | 0.30 s |
| **Total from launch to upright** | **~22** | **~0.73 s** |

- **Distance:** about **34 px ≈ 1.0 h**. The slider keeps roughly his running speed (~105 px/s) through the slide, then stops abruptly.
- **Poses:** about four: drop, slide, hand-down, crouch.
- **The miss:** the dribbler rode it and kept the ball.

### 2.4 Goalkeeper [M]
I measured three low or smothering dives (8:48.4, 10:47.1 and 10:47.7) and one low catch at the near post (4:19.5 and 3:30.0).

| What | ISS Deluxe |
|---|---|
| Take-off | 1 crouched lunge frame |
| Flight | **2 to 4 frames (0.07 to 0.13 s)**: one held pose, body diagonal (~45°), arms up and forward. These are short, low dives, not flat flights. |
| Landing | Lands **flat**. The lying sprite is ~35 px long and 7 to 11 px tall: about **1.05 h long, fully horizontal**, arms spread. |
| Time on the ground | **12 to 15 frames (0.40 to 0.50 s)**, 14 frames when holding a caught ball |
| Get-up | **None.** He snaps from the lying pose straight to the standing ready pose in a single frame. The game never waits on a keeper standing up. |
| Distance covered | Roughly one body length (≈1 h) for these short dives. There is no long lateral dive in the footage. |
| Parry | At 10:47 the shot struck the diving keeper and rebounded up and out, loose. That is a deflection, not a catch. |
| Catch | Caught low at the near post while diving, then held on the ground for 14 frames before standing. |
| Hold | Standing, **ball raised in one hand at about head height, arm forward**. He walks slowly for ~3.5 s, then **drop-kicks (punts)** it long. Catch to punt: **~3.6 s**. |
| Behind the net | When the keeper is inside the goal, the **net mesh is drawn over him**, so you see him through the netting. |
| Ground pick-up | Not seen clearly in this video. |

### 2.5 Goals and nets [M]
Measured on a native-pixel grid at 8:50.

| What | ISS Deluxe |
|---|---|
| Near post height | **~59 px ≈ 1.8 h** (true: 2.44 m = 1.34 h). **Height is exaggerated about 1.35× against the body.** |
| Mouth (near post to far post) | ~97 px across and ~90 px up the slanted goal line, about 132 px along it, **≈ 4 h** (true: 7.32 m = 4.0 h). **Width stays true to the body.** |
| Depth | The back frame is offset ~50 to 55 px (≈1.6 h) behind the goal line, drawn obliquely like the pitch lines. |
| Posts and bar | ~3 px thick, cream-white with a grey shaded side. Post, bar and back frame form one rigid box. |
| Net | A regular diamond mesh of 1 px light lines with ~4 px cells, covering the side, roof and back. It is see-through: grass and players show through it, and it draws in front of anything inside the goal. |
| Ball in the net | The ball rolls to the back and **stays inside the net**, visible through the mesh, while the 3D "GOAL" letters animate in (about 2 s). **No visible net bulge** in this footage. |
| Shadow | No distinct goal shadow on the grass. The stand and wall shadow is a broad grey band along the bottom touchline. |

### 2.6 Sprites [M] unless marked
- **Height:** ~33 px. Head about 6 px (**about 1 : 5.5**), shoulders ~11 px wide. Proportions are realistic, not cartoon, with long legs.
- **Outline:** no uniform black outline. Each material's edge is its own darkest shade (selective outlining), for example the yellow shirt edged in ochre and the dark shorts in near-black. Figures read by value contrast against the green.
- **Shadow:** a dark-green drop shadow under every player, offset down and to the left. The ball has one too, and its separation from the ball shows height.
- **Kits:**
  - Both outfield teams use **the same body poses**; only kit colours differ (Brazil yellow and blue, Argentina white and black).
  - Each keeper has his own kit (red; yellow and black).
  - Argentina's stripes show on the front and back views; side views simplify them to a plain shirt with one dark mark.
- **Hair and heads:** these vary by player (long blond, dark short), so heads or hair are per-player variants or palette entries [I]. Reviewers single out ponytails, a headband, red hair and beards [R, gamingrespawn below].
- **Ball:** about 7 px, about 0.2 h, roughly 1.75× real size against the body.
- **Colours per sprite:** the SNES hard limit is 15 colours plus transparent per sprite palette [R, nesdev]. I could not count ISS's exact palette entries through video compression.
- **Selection ring:** an ellipse (~30×10 px) under each team's active player. Its colour cycles red, orange and yellow.
- **Camera:** follows the ball tightly, even at pass speed (400 px/s). The ball carrier sits off-centre with space ahead of him.
- **HUD and clock:**
  - The HUD has the score and clock at the top, a radar mini-pitch at the bottom centre, and the names and positions of both active players.
  - The clock counted down 3:00 per half in real time in this match.

---

## 3. Research: how ISS Deluxe was made and received

**Who made it**
- Konami developed it. Release dates: Japan 22 Sep 1995 (*Jikkyou World Soccer 2: Fighting Eleven*), North America Nov 1995, Europe 25 Jan 1996. Factor 5 did the Mega Drive port [R](https://en.wikipedia.org/wiki/International_Superstar_Soccer_Deluxe).
- The ISS series was made mainly by Konami's Osaka branch (KCEO) [R](https://en.wikipedia.org/wiki/International_Superstar_Soccer), matching the title screen [M].
- The 1994 ISS was directed by Yasuo Okuda [R](https://en.wikipedia.org/wiki/International_Superstar_Soccer_(video_game)). No staff credits for Deluxe were found.
- Winning Eleven / Pro Evolution came from the separate Tokyo studio, KCET [R](https://en.wikipedia.org/wiki/Pro_Evolution_Soccer).

**Content**
- There are 36 national teams plus 6 hidden ones, with 20 players per squad [R](https://gopherproxy.meulie.net/gopher.endangeredsoft.org/0/gamefaqs-archive/snes/international-superstar-soccer-deluxe/Team-Data-Guide-by--Spanettone.txt).
- Each team has two kits, each defined only by **shirt, shorts and sock colours** (same source).
- Player names are made up because the game was unlicensed (Allejo, Beranco, Galfano).
- An edit mode changes uniform colours and player attributes [R](https://gopherproxy.meulie.net/gopher.endangeredsoft.org/0/gamefaqs-archive/snes/international-superstar-soccer-deluxe/FAQ-by--IFerrel.txt).
- A kit defined only by colours, and editable only by colour, is how palette-swapped kits work [I].

**Controls and assists**
- With the ball: B + direction passes, A + direction lofts, X + direction shoots, Y runs [R](https://gopherproxy.meulie.net/gopher.endangeredsoft.org/0/gamefaqs-archive/snes/international-superstar-soccer-deluxe/FAQ_Strategy-Guide-by--NMutis.txt).
- Without the ball: B is a standing tackle, X + direction a slide, X+A a body charge, R switches player, and L + a face button changes tactic (same source).
- **Shots and lobs curve toward the direction held after the kick**, which is aftertouch (same source). Holding X longer changes shot power and curve [R, IFerrel].
- No dedicated through-ball button and **no documentation of how the pass picks its target** were found. The footage fits a pass aimed at the team-mate nearest the stick direction, delivered to his feet [I].
- There are 16 formations and 8 strategies (per a search snippet of MobyGames, which blocked the fetch).

**Keepers**
- The keeper is automatic by default, with a semi-auto mode where L+R takes control [R, IFerrel].
- He dives, punches high balls and corners, sometimes fumbles ("Keeper Fumbles!") and gets drawn off his line by lobs [R, NMutis].
- Reviewers call the keepers "insanely good" [R](https://sega-16.com/?p=5901) and manual keeper control hard [R, Wikipedia].

**Reception**
- CVG gave 91%, GamePro 4/5 ("easy to learn" controls), Next Generation 3/5. The Mega Drive version got Joypad 92% (praising speed and animation), NeXt Level 90% and Hobby Consolas 91% [R, Wikipedia].
- Sega-16 wrote that it "plays so much more fluidly" than FIFA '95 and found the controls easier to pick up. It called the sprites "much bigger and better animated" [R](https://sega-16.com/?p=5901).
- Players praised how individual the players looked compared with FIFA's identical figures [R](https://gamingrespawn.com/?p=19569).

**Hardware [R]** ([snes.nesdev.org/wiki/Sprites](https://snes.nesdev.org/wiki/Sprites), [snes.nesdev.org/wiki/Palettes](https://snes.nesdev.org/wiki/Palettes))
- The SNES has 128 sprites, sized 8/16/32/64 px squares plus 16×32 and 32×64, with two sizes active at once.
- Each scanline holds at most 32 sprites or 34 eight-pixel slivers.
- Sprite pixels are 4 bits per pixel: each sprite uses **one of 8 sprite palettes of 15 colours plus transparent** (CGRAM entries 128 to 255).

**Not found:** published frame or animation counts, sprite sheets, any interview about animation, and confirmation that the PAL version was speed-corrected. I did not repeat claims from an unsourced page that says "motion-captured".

---

## 4. Design lessons

1. **Cadence is nearly constant; stride does the work.** ISS changes the cycle period only from 0.60 s to 0.53 s across a 70% speed range. A run looks committed and readable because the legs never flicker faster than about 8 poses per 0.5 s.
2. **Few poses, held long.** Each run pose stays up for 3 to 5 game ticks. Six to eight strong silhouettes beat many in-betweens at this size.
3. **Passes are fast, flat and certain.** A short pass covers 4 to 5 heights in under 0.4 s at constant speed and lands exactly at the feet. The receiver kills it dead with no bounce or fumble. The player feels in control because the ball goes where it should, quickly.
4. **The ball dies quickly when nobody is there.** Long ground balls lose about 4 h/s every second, so loose balls stay contestable instead of rolling out of play.
5. **The slide is short, committed and punishing.** It is one held pose for a third of a second, then about 0.3 s helpless on the ground. Missing costs you a defender for 0.7 s.
6. **Keepers never waste time getting up.** A low dive is a quick lunge, flat for about half a second, then an instant snap to the ready stance. A held ball is shown clearly: raised in one hand at head height for a few seconds, then punted.
7. **Goals are big where it matters.** The height is exaggerated (~1.8 h) so the frame reads and headers look possible, while the mouth keeps true width (~4 h).
   - The net is a see-through mesh drawn in front of whatever is inside the goal.
   - A scored ball stays visible in the net for the celebration.
8. **Readable over realistic.** Players are drawn 1.55× the pitch scale and the ball about 1.75× its body-relative size. Shadows show ball height, and a landing "×" marks high balls.
9. **The camera serves the ball.** It locks to the ball even at pass speed and leaves space ahead of the carrier.

---

## 5. What Proper Football takes from this (v0.8)

Proper Football draws players at true scale against the pitch (1.82 m; see DESIGN.md). Targets are given in player heights (h = 1.82 m) and in metres at that scale. Animation times are at 50 Hz-equivalent ISS speed. Our 60 fps simulation should use these seconds, not ISS's frame counts.

### 5.1 Run and dribble cadence
| Target | Value |
|---|---|
| Stride per cycle (two steps), `L(v)` | **L = 0.80 m + 0.44 s × v**. The current code has 1.25 m + 0.33 s × v. |
| Cadence, v / L | 3.9 m/s → **1.55**/s; 5 m/s → **1.67**/s; 6 m/s → **1.74**/s; 7 m/s → **1.80**/s; 8.6 m/s → **1.88**/s |
| Cycle period | 0.64 s at a jog down to 0.53 s at top speed |
| Poses per cycle | 6 (minimum) to 8; each shown 0.066 to 0.09 s. Never skip a pose; never blend. |
| Sprint | Same rule. Extend stride, not cadence. Cap at ~1.95 cycles/s. |
| Dribble | Same cycle as the run at the same speed. Straight-line dribble speed ≥ 0.9× run speed (ISS 0.97×). Touch on foot-contact frames; ball 0.3 to 0.7 h ahead (v0.6 numbers hold). |

### 5.2 Pass assist
| Target | Value |
|---|---|
| Target selection | Team-mate nearest the stick direction inside a ±30° cone, weighted to the nearer one; if nobody is in the cone, pass into space along the stick [I; ISS's own rule is undocumented]. |
| Wind-up | Plant to contact **0.12 to 0.14 s**, then a 0.05 s follow-through |
| Short-pass speed | **~12 h/s ≈ 22 m/s** at release, nearly constant over a short pass. Arrival for 8 to 10 m: **0.35 to 0.45 s**. |
| Arrival | Aim at the receiver's feet: at the predicted feet position, or the current one if he is standing. The ball reaches him on the ground. |
| Receiving | The ball stops dead at the feet within 1 frame (no bounce-off for an assisted pass). Receive pose 0.25 s, cancellable by the next input. |
| Ball friction (unowned, on the ground) | **~4 h/s² ≈ 7 m/s²** deceleration, so a ball into space rolls ~1.5 s and stops |
| Lofted balls | Show a landing "×" (our own art) on the grass until the ball lands. The shadow separates from the ball with height. |

### 5.3 Slide tackle
| Target | Value |
|---|---|
| Frames (poses) | 4: drop (0.05 s), slide (one held pose), hand-down, crouch |
| Slide | **0.33 s**, carrying the run speed and then stopping hard |
| Distance | **1.0 to 1.3 h ≈ 1.8 to 2.4 m** |
| On the ground | 0.10 s hand-down + 0.20 s crouch = **0.30 s**, no control |
| Launch to upright | **~0.73 s** |

### 5.4 Keeper dive
| Target | Value |
|---|---|
| Low or smother dive | 1 lunge pose (0.04 s), **flight 0.07 to 0.13 s** (one held diagonal pose), land **flat** (the lying pose is 1.05 h long, fully horizontal) |
| Down time | **0.40 to 0.50 s** lying, then **snap straight to the ready pose** (no get-up clip) |
| Distance (low dive) | ~1 h (1.8 m) |
| Long lateral save dive | Not in the footage. Suggested: flight 0.25 to 0.33 s over 1.5 to 2.0 h, body flat, then the same 0.45 s down and snap. **Not measured.** |
| Catch | While lying, the ball sits at the chest under the hands. Standing: **ball raised in one hand at head height, arm forward**. |
| Hold before release | AI holds ~3 s, walking slowly. Punt by default; throw when a team-mate is free. |
| Parry | A shot the keeper can't hold rebounds off him up and out, loose |
| Drawing | When the keeper is inside the goal, draw the net mesh over him |

### 5.5 Goals
| Target | Value |
|---|---|
| Height | ISS draws ~1.8 h. Proper Football is now at true height (1.34 h, the owner's v0.7 decision), which is fine. If the owner wants the ISS look, go no higher than **1.6 h**, because headers and the in-play crossbar must stay consistent. |
| Mouth width | **4.0 h** (true; ISS keeps this) |
| Depth | **~1.5 h** back to the back frame, drawn in the pitch's own oblique projection |
| Posts | Two tones (light front, shaded side), about 1/11 h thick |
| Net | A regular see-through mesh, cells about 1/8 h, covering the side, roof and back. Drawn in front of anything inside the goal. |
| Scored ball | Stays visible inside the net for ≥ 2 s. A net bulge is our own addition; ISS shows none. |

### 5.6 Frames per animation (budget)
| Animation | Poses per direction |
|---|---|
| Run | 6 to 8 |
| Sprint | 6 |
| Pass | 3 |
| Shot | 4 |
| Slide | 4 |
| Receive | 1 to 2 |
| Keeper low dive | 3 (lunge, flight, lying) |
| Keeper catch-hold | 2 |
| Keeper punt | 2 |

Draw five directions and mirror the other three. Proper Football's v0.7 pose library already covers these counts.

---

## 6. Art approach: base bodies and palette swaps

How 16-bit sports games did kits:
- **One body set, many kits.**
  - The SNES gives each sprite one 16-entry palette (15 visible colours plus transparent), and there are 8 such palettes on screen [R, nesdev].
  - A sports game therefore draws every pose once, using fixed palette **indices** for each material: skin (2 to 3 shades), hair (1 to 2), shirt (2 to 3), shorts (2), socks (1 to 2), boots and outline (1 to 2).
  - A kit is just a different set of colours in the shirt, shorts and socks entries [I].
- **This is what ISS shows.**
  - Teams are defined only by shirt, shorts and sock colours, two kits each, and the edit mode changes only colours [R, Spanettone and IFerrel guides].
  - Brazil and Argentina use identical poses [M].
  - Keepers have their own palette [M].
  - Hair and face vary per player: ponytails, a headband, beards, red and blond hair [R, gamingrespawn; M].
  - Patterned shirts like Argentina's stripes appear only where the pose shows them clearly [M].
- **Budget [I], for ISS-like coverage:**
  - Base bodies: about **1 outfield body and 1 keeper body**, plus a handful of head or hair variants. Build differences come from a few alternate bodies at most.
  - Directions: 5 drawn and 3 mirrored.
  - Outfield poses per direction: run 6 to 8, sprint, pass 3, shot 4, header 2 to 3, slide 4, tackle 2, fall and lie 2, throw-in 2, celebration 2 to 4. That is about 35 to 40 poses, or roughly **180 to 200 drawn frames**.
  - Keeper poses: ready, low dive (3), high dive (3), catch and hold (2), punt or throw (2): about **12 poses, ~60 frames**.
  - With 8 sprite palettes on screen, two teams take 2 to 4 of them (outfield and keeper per team), leaving the rest for the referee, ball and effects.
- **Proper Football has done both.**
  - v0.6 kept 16 outfield bodies and 4 keepers, recoloured through a template kit with key colours (region + shade) into any club kit at run time. That handles the stripes and hoops a 15-colour SNES palette couldn't. The engine still supports it (`kit_keyed` atlases).
  - v0.7 generated 279 separate sets instead, one per club kit and keeper. Each frame was posed independently, so the same pose drifts from club to club and from one direction to the next. Some slides came out mangled or with a ball baked in.
  - v0.8's recommendation is to go back to the v0.6 approach with fewer, better bodies (see `docs/STATUS_v0.8.md`).
  - The ISS lesson: **spend the art budget on a few strong, well-timed poses and on face and hair variety, not on more in-between frames.**

---

## 7. Earlier measurements (v0.6)
The v0.6 study used a different YouTube longplay, at 60 fps.

| What | v0.6 finding |
|---|---|
| Player height | ~28 of 224 lines. v0.8 measures ~33 px with a cleaner method; v0.6 likely missed the hair and boots. |
| Dribble | Ball 0.3 to 0.7 h ahead; a touch every ~8 frames at 60 fps |
| Dribbling speed | ~4.9 h/s |
| Running speed | 5 to 6 h/s, from the camera. v0.8 measured the player directly: 4.6 h/s top, 2.7 h/s jog. |
| Pass speed | ~12 h/s, from the camera. v0.8 confirms it: 400 px/s ≈ 12 h/s. |
| Run cycle | One pose held ~4 frames at 60 fps. v0.6 read a two-step cycle as ~0.27 s; v0.8 finds 0.27 s is **one step** and the full cycle is 0.53 s. |
| Grass | Narrow bands 0.3 to 0.5 h wide; dominant colours (160,216,80) and (140,180,58) |
| Frame pacing | 1 in 6 frames repeated at 60 fps, which means a 50 Hz source |
