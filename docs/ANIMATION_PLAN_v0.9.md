# Animation plan (v0.9 proposal, 2026-11-26)

The owner's verdict on v0.8.1 animations: not impressive, should be "more dramatic, more arcade, and more realism at the same time". Slide tackles look really bad. Keepers need proper dives.

## What is wrong now (contact sheets `ArtWork/base_pilot/sheet_*`)
- **Slide tackle.** The four poses are: a step, a man sitting upright with one leg poking out, a sit, and a kneel. There is no flat, committed, leg-first slide, so it reads as a stumble.
- **Kicks and shots.** The back-swing and follow-through are small (natural gait-data angles). At 64 px a small swing doesn't read. A shot looks like a pass.
- **Keeper dives.** There is one short low lunge, then lying down and a kneel-up. There is no long, flat, airborne save across the goal.
- **Run.** The cycle is correct but polite. ISS reads as committed because of a forward lean, a high rear heel and pumping arms.

## The fix: exaggerated key poses, held, fewer in-betweens
This follows the ISS study, lessons 1, 2 and 5: few strong silhouettes, held long.

| Animation | New poses per direction | Key change |
|---|---|---|
| Slide tackle | 4: drop, **flat slide** (body near horizontal, lead leg straight out, arm back on the grass), hand-down, crouch-up | Body 20° off the grass instead of sitting upright; held for 0.33 s while sliding |
| Standing tackle | 2: lunge, poke (leg across the ball) | New; there is none today |
| Shot | 4: plant, **big back-swing** (heel to the backside), strike, **high follow-through** (leg past the hip, arms out) | About twice the current swing |
| Pass | 3: plant, swing, side-foot follow | Clearer side-foot |
| Run / sprint | 8 / 6 | 15–20° lean, higher rear heel, bigger arm drive |
| Keeper: long dive | 3: push-off, **full flat flight** (body horizontal, both arms stretched past the head), land flat | New; for shots beyond 1.5 m. Flight 0.25–0.33 s |
| Keeper: low dive / smother | 3: lunge, diagonal, flat | Present but weak; redo |
| Keeper: tip over, punch, catch high | 2 each | Clearer silhouettes |
| Celebrations | 4 | Knee slide, arms out "aeroplane", jump |

The code side for the long dive (a flight time and distance by shot distance) and the standing tackle is small and goes in alongside the art.

## How it is made
- **A new pose library** (`poses_arcade.py`). The same stick figure, with exaggerated angles; v0.7's exaggerated set is the starting point for kicks.
- **One PixelLab job per animation**, as in v0.8, with three takes each. I'll hand-pick from contact sheets against the idle (the dither and shading rule).
- **Pilot first, on B01 (outfield) and BK1 (keeper) only.** Slide, tackle, shot, run and the keeper dives, shown to you in-game before the other bodies are done.

## Cost (at the v0.8 rate of about $0.016 per job)
| Step | Jobs | Cost |
|---|---|---|
| Pilot: B01 + BK1, the new sets, 3 takes | ~150 | **~$2.50** |
| The other 5 outfield bodies + BK2 | ~450 | ~$7.50 |
| B06 replacement body (optional) | ~200 | ~$3.50 |
| **All** | | **~$10–14** |

That is well under the $50 ask-first line. The PixelLab balance is $482.39.
