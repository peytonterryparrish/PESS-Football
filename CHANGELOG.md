# Changelog

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
