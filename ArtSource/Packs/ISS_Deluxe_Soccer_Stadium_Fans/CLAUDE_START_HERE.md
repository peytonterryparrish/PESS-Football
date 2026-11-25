# ISS Deluxe Soccer | Sprite import

20 fan designs with one seated and one standing south-facing idle each. The original transparent PNGs are unchanged. Previews are for review only.

| Folder | Subject | Orientation |
|---|---|---|
| `02_ISS_Deluxe_Soccer` | English football stadium spectators | South/front, seated or standing idle |

## Unreal import

1. Import the PNGs under the relevant project’s own content root. If the project uses Paper 2D, enable the Paper 2D plugin. In the Content Browser use **Sprite Actions > Apply Paper 2D Import Settings** and **Sprite Actions > Create Sprite**. Check that filtering is **Nearest** at native/integer scale to retain block edges and dither. For smaller or distant crowds on a moving camera, compare with correctly generated mips at representative zooms on device before deciding on the shipping settings.
2. Preserve the original canvas and use the base pixel dimensions in `sprite_index.csv`. The visible alpha bounds are in left/top/right-exclusive/bottom-exclusive pixel coordinates, with origin at the top-left. Use the per-set notes or manifests for authoritative pivots, where provided. Alpha bounds are a QA aid, not an instruction to auto-trim.
3. **Standing people:** align to the feet; darken or light by section at placement rather than modifying the source file. Stadium crowds can be batched as instances or use small texture pages in production. **Seated people:** line up hips with the bench seat. Keep the seat and stadium crowd sprites on distinct render layers. The seated and standing stages are separate idle sprites, not transition frames.
5. Test exact game-camera scale before duplicating across a stadium or city block. Validate alpha on both bright and dark backdrops, especially cars' tires and dark suits, and verify draw order against railings, pitch barriers, curbs and foreground props. No engine import or in-game rendering has been performed as part of this pack.

Official Unreal documentation: https://dev.epicgames.com/documentation/unreal-engine/how-to-import-and-use-paper-2d-sprites-in-unreal-engine
