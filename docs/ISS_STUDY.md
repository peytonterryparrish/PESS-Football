# ISS Deluxe footage study (v0.6.0)

The owner asked for Proper Football's look, feel and flow to follow International Superstar Soccer Deluxe (SNES, 1995). I studied gameplay footage frame by frame, measured it, and matched the numbers. The art stays Proper Football's own: nothing was copied from the game.

## Method
- **Source:** "International Superstar Soccer Deluxe | Full Game Longplay | No Commentary | SNES" on YouTube (`https://www.youtube.com/watch?v=vd_ipSTKbGc`), watched in the desktop app's browser pane. Nothing was downloaded.
- **Frame rate:** the stream runs at 60 fps, which I measured with `requestVideoFrameCallback`: every gap is 16.7 ms.
- **Frame capture:** a script in the page paused the video, seeked to exact timestamps one frame (1/60 s) apart, and drew each frame into a canvas.
  - It cropped the 4:3 SNES picture to 256×224.
  - It laid the frames out as labelled contact sheets, each frame stamped with its index and timestamp.
- **Measurements:** made from the same frames, in the page.
  - **Camera movement:** the best-matching grass shift between consecutive frames.
  - **Player position:** the centroid of shirt-coloured pixels.
  - **Grass colours:** a histogram of the grass pixels.
- **Study sheets:** saved outside the repository, in `Saved/ISS_Study/` (they are frames of a commercial game, so they aren't committed):
  - `01_overview_every_5s.jpg`
  - `02_scan_every_1s.png`
  - `03_header_frame_by_frame.jpg`
  - `04_dribble_frame_by_frame_6x.jpg`
  - `05_dribble_48_frames.jpg`
  - `06_dribble_run_cycle_zoom.png`
  - `07_full_frame_goal.png`
  - `08_proper_football_v060.png`: our game after the changes, for comparison.

## Measurements
| What | ISS Deluxe (footage) | Proper Football before | v0.6.0 |
|---|---|---|---|
| Player height on screen | ~28 of 224 lines (12.5%) | 128 of 1080 px (11.9%) | unchanged (already matched) |
| Dribble: ball ahead of the body | 0.3 to 0.7 player heights; a touch every ~8 frames (once per stride); never leaves the boots | up to 2.3 m ahead of a 0.9 m-tall drawn player (v0.5) | 0.24 to 0.40 m (0.26 to 0.44 heights); a touch every step |
| Dribbling speed | camera 54 px in 23 frames: ~141 px/s, **4.9 heights/s** | ~6.8 heights/s for a typical player | about 5.8 heights/s (`GameSpeed` 0.85) |
| Running speed | camera steady at 150 to 180 px/s: **5 to 6 heights/s** | ~6.8 heights/s | about 5.8 heights/s |
| Pass speed | camera peaks of 250 to 380 px/s: **~12 heights/s** (the camera trails the ball) | ~18 heights/s | ~15 heights/s |
| Run cycle (two steps) | each pose held ~4 frames; one cycle ≈ 16 frames = **0.27 s** | 0.58 s at a jog (stride length was in full-size metres) | ~0.3 s (stride scaled to the half-size bodies) |
| Grass stripes | narrow diagonal bands about 0.3 to 0.5 of a player's height wide | 5.25 m bands (5.6 heights) | 0.5 m bands (~0.4 heights) |
| Grass colours | dominant (160,216,80) and (140,180,58), with lighter and darker speckle | (88,162,48) and (70,138,38) | the sampled ISS palette |
| Frame pacing | about 1 frame in 6 is repeated (a 50 Hz original captured at 60 fps) | — | — |

## Not matched (on purpose, or left for later)
- ISS's goals are drawn about 2.5 player heights tall, roughly twice real proportion. Ours stay in proportion with the players, so ball heights and headers stay consistent.
- ISS has its own player art, HUD and radar. Proper Football keeps its own.
