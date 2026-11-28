# v0.10 sprite audit (N01 and NK1)

The audit covers every frame the game can look up:
- **N01:** 2920 keys.
- **NK1:** 1193 keys.
- **Included:** all 33 approved 12-direction animations and the older 8-direction fallback poses.

## The brown streak
1. **N01 a060 run, walk and sprint** (a300 is the mirror): a stalk with a cap above the head. It was cut from 25 frames.
2. **NK1 a150 gk_ready** (a210 is the mirror): a blade and stick above the head.
   - The cycle was replaced by the turn image held still.
   - The opening frames of catch, high, punch and scoop now hold their first clean frame.

## Fixed
- **Kit colours:** raw red socks and magenta keeper sleeves now take the club colours. Raw-kit flags fell from 1700 to 84 in N01 and from 743 to 4 in NK1.
- **Leg gaps:** knees were being cut out as if they were balls.
- **Loose debris:** strays fell from 195 to 16 in N01 and from 6 to 3 in NK1.
- **Hair lines** above the head.
- **Heads:** hook and ring shapes repaired in chest a150 and celebrate_05/07 a120.
- **Balls:** a ball above the head in chest north, and baked balls in 11 frames.
- **Ground:** a squiggle under the kneeling keeper.

## To regenerate
| Body | Animation | Direction | Frames | Why |
|---|---|---|---|---|
| NK1 | gk_ready | a150 | all | Blade and stick artefact; currently a held still |
| NK1 | gk_catch / high / punch / scoop | a150 | f00–f04 / f00–f02 / f00–f02 / f00–f01 | Start from the broken ready pose; opening frames are held |
| NK1 | gk_throw | a060 | all | Throwing arm drawn as a grey hockey-stick shape |
| N01 | celebrate_05 | a060 | f01–f12 | Head becomes a hollow ring, then disappears |
| N01 | celebrate_05 | a120 | f03–f12 | Heads repaired from f02; green specks remain on the back |
| N01 | celebrate_04 | north | f05–f12 | Hands hold a black block above the head |
| N01 | shot | north | f06–f10 | Turns to face the camera in the back view |
| N01 | shot | a120 | f05–f08 | Body shrinks and the head separates; the kicking foot floats apart |
| N01 | chest | a150 | f02–f05 | Head repaired, but a clean take would be better |
| N01 | dribble | south | f01–f03 | Ball removed, but the planted foot has no boot |
| N01 | throwin | all directions | f01–f08 | Brown ball baked into the hands, so two balls can show |
| N01 (old pose) | stumble | north-east (and its mirror) | 1 | Ball by the hand; only a fallback |

## Suggestions
1. Make the build fail when the audit finds raw kit pixels.
2. Prompt PixelLab for "no ball, empty hands", and let `anim_hands` place the ball.
3. Check gk_ready and idle in every direction before generating moves that start from them.
4. Review sheets in the Newcastle kit: black kit shows raw red and specks at once, while Liverpool red hides them.
5. Back views (a120, a150, north) are the weakest; generate them from the `rot12_*` turn images.
6. Decide whether keeper sleeves follow the kit trim colour or stay a fixed colour, and prompt for that.

## Tools
- **Audit:** `Tools/pipeline/audit_anim.py`.
- **Fixes:** `Tools/pipeline/fix_raw12.py`. It always restarts from `ArtWork/audit_v100/raw_backup/`. Don't re-run it after regenerating a listed animation without first refreshing that animation's backup.
