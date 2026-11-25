# ISS Deluxe Soccer Football | terrace supporters

20 crowd characters, each in two matching, static, south-facing poses: **20 standing and 20 seated**. The North/South convention follows the existing R08 Match Engine South Idle players and staff. This pack extends the original code-native R08 palette and pixel treatment using the approved original R08 faces, plus newly drawn match-day supporter outfits and pose geometry. They are fictional, with no club trademarks.

## Import

- `Sprites_256/`: 40 individual RGBA PNGs, transparent background, no floor, shadows, or stadium seats. This is the main delivery.
- `Native_64/`: matching, 64×64 source-resolution versions of the same 40 sprites (not additional characters). The 256px files are intentional nearest-neighbor 4× renders, not independently sharpened or invented details.
- `Manifest.json` / `Manifest.csv`: indexed art paths, paired fan IDs, per-pose pivots and alpha bounds. Coordinates are top-left origin. The floor anchor is (128,252) on the 256px files and (32,63) on the 64px files. The seated seat mount is near the hips, at 256px (128,188 or 192) depending on character, for systems that align to a bench or seat socket. Use one pivot system per crowd placement node; if anchoring at the floor, position/align stadium furniture independently. Seat and foot layers are not baked into the art.
- `Seated_QA_Preview.png`: seat-height alignment demonstration on a pale backdrop. The guides are preview-only.
- `Preview.png`: contact sheet; standing on the left, seated on the right for each character.
- `source/`: editable pixel renderer and R08 inputs for regenerating this art. Use Python 3 with Pillow and NumPy. Fonts are only used for the contact sheet. Run `python source/make_fans.py` from this folder.

### Paper2D

For nearest edges, apply Unreal's Paper2D texture settings and verify the filter is Nearest; avoid smoothing or lossy compression around alpha edges. Set pivot from the manifest if aligning separate seating objects. When importing 256px sprites, use four times the pixels-per-unit value used for native 64px R08 sprites to preserve the same authored world size. Both are static renders, not flipbooks; pair them by `fan_id` if triggering stand/sit swaps.
