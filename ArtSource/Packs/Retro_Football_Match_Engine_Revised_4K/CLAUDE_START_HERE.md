# Match-engine art add-on: 992 modular assets

This ZIP is the match-engine add-on for the already delivered retro football master. It does not replace the approved team-art assets, UI menus, Youth Academy or stadiums. It contains art, sample compositions and import information, not an integrated engine. Do not add a title, player cards, pack screens or collectible packs.

## Asset directory and counted scope

| Directory | Count | Contents |
|---|---:|---|
| `Sprites/01_Match_HUD` | 260 | Scoreboard plates/shells, clock and added-time wells, left/right player name bars, fitness, shot power, pause and set-piece controls |
| `Sprites/02_Numbers_Names_Events` | 300 | Two new 00–99 styles, extended name glyphs, foul/goal/goal kick/corner/yellow/red and other flags, blank flag chrome |
| `Sprites/03_Minimap` | 160 | 24 bases + 24 line overlays, 48 live-marker options, 40 formation diagrams and 24 event target overlays |
| `Sprites/04_Goals` | 72 | 9 remaining era-inspired builds × South/North × shadow/rear net/front frame/composite |
| `Sprites/05_Match_Props` | 60 | 10 original logo-free English top-flight era-inspired balls × 3 static views; 30 corner flags |
| `Sprites/06_Officials` | 60 | 30 centre refs and 30 assistant refs holding checkered flags |
| `Sprites/07_Sideline_Crew` | 80 | 20 head coaches, 20 assistant coaches, 20 bibbed tracksuit warmup players, 10 ball boys and 10 camera operators |

Each of the 992 counted files is an RGBA sprite at its authored dimensions, not an entire UHD canvas. `Atlases_4K/` contains 3840×2160 sheets for batching; resolutions and rects are in `Asset_Manifest.json` and `.csv`. Atlas sheets, examples, developer notes, and provenance do not count toward the 992. Regeneration inputs in `Source/` are not delivery sprites.

## Match HUD, names and minimap

- Layer a blank scoreboard casing, home/away abbreviation/crests, dynamic score, clock and stoppage time. Use `me_clock_led_pair_XX` and `me_score_led_pair_XX` with the existing colon and separators or use individually rendered text. Do not bake scores into new layouts.
- Left and right corner bars have separate artwork. Render dynamic player names on top, using the R08 master font or a localized font, and the new extended glyphs for accents. `me_roster_extended_*` uses NFC, 16 px advance and y=26 baseline (21×37 sprite canvas); see individual asset notes. Do not assume that a name is a fixed sprite. Clamp or truncate long names and test language fallbacks.
- Dynamic meters: use empty casing, clip the fill width with a UI mask, add safe-zone markers independently. The pre-filled steps are examples or discrete options, not a required meter quantization.
- Rules messages use a readable icon plus text: goal, foul, yellow card, red card, corner kick, goal kick and more individual assets. Use a timed overlay keyed from your rules events. These are referee cards, not player cards.
- Minimap is top-down. Match the base with the linework of the same geometry and palette. Register live icons from normalized pitch positions, not the broadcast-camera crop. For horizontal/compact mapping, pitch marking bounds are x=10..width−11 and y=12..height−13; the vertical layout is the horizontal native map rotated 90° counterclockwise. Confirm whether home attack direction should flip at halftime. Only use static formation and set-piece overlays in the contexts appropriate to those examples; they are not a replacement for live markers.

## Projected goals and on-field placement

- `Data/Goals_Placement.json` is the authoritative snapped **4K stadium-world atlas** placement for this pack. Use the exact per-file absolute (x,y,width,height) at 1:1 stadium-atlas scale. This placement is **different** from an atlas packing rectangle in the asset manifest. Do not scale one by the other.
- Approved V3 camera: x = −52.5 m is South and x = +52.5 m is North. Keep the 716.8×403.2 source crop and pan it over the approved 3840×2160 stadium atlas. Do not change the angle or zoom. An approximately 1.8 m player is 40.56 projected atlas pixels tall, measured on opaque crown-to-boot length before scaling to screen.
- Use one of the nine remaining goal designs. For each end compose shadow, rear net, actor/ball depth order, then front frame. Do not place the composite on top of those layers. The composite is a convenience alternate for previews. The 7.32 m × 2.44 m opening is shared across the game options; differences are timber/metal, bracing, net depth/mesh and era styling, not historical reconstruction claims.
- Officials and sideline crew are all 64×64 **South Idle** only. Use the R08 field placement convention; source notes use foot anchor (32,63), equivalent to (32,64) when anchoring on the exclusive lower edge, not a head or camera-prop anchor. Camera operators and ball boys carry props in the same cutout. These are static poses only.
- Ball sprites have centered pivot, three independently sampled views per motif, no rolling cycle, and no manufacturer logos. Head coaches use suits or smart polos with slacks and polished shoes. Assistants and other sprites are unchanged. Corner flags use solid red or yellow cloth only. Corner flag pivot is (20,42) in the 42×46 native canvas, with cloth/wind variants not timed animation. Goal, referee and prop choices are not representations of specific people or licensed products.

## Unreal handoff and checks

Use nearest/point sampling to preserve the dither and hard alpha. Atlases contain 2× sprite rectangles and small edge padding. Individual sprites are preferred for goal placement, short glyphs and static world props. Read any margin values in **native** sprite pixels. Preserve a grounded shadow layer for on-pitch props and keep HUD art separate from world art. Independently verify translucency, edge bleed and arbitrary display scales in your engine.

Start with one stadium at kickoff, then both goal ends and a corner. Compare facing, keeper depth order, ball scale and flag anchor. Trigger six rules notifications, confirm keyboard/controller labels, exercise fitness and power from 0 to 100, and check on-pitch versus sideline scale before adding additional movement. Nothing in this ZIP adds or restores non-south animation for the existing player library.

## Revision notes
Goal 08 has been removed from both ends. Retained goals keep their original IDs; do not renumber 09 or 10. Remap old corner flag IDs using Data/Corner_Flag_Replacements.csv if needed. Existing coach IDs are unchanged.
