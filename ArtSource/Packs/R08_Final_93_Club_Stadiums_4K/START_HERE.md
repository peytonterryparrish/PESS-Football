# R08 / PIX Premier Football: 93 final home stadiums

There are 279 finished 3840 × 2160 PNG stadium backgrounds: 93 home clubs × daylight, evening sunset, and floodlit night. This is the approved five-tier stadium system, not 93 separately modeled grounds. The home primary color is baked into the seats, including both end stands, so do not apply a runtime seat tint to these PNGs. Tier 5 includes the approved 72-row south stand.

## Integrate
1. Find your club by its existing R08 source slug in Manifest.csv or Manifest.json. Use the corresponding stage image in Clubs/<slug>/. Names and IDs are unchanged.
2. Each PNG is a finished world atlas, not a 16:9 gameplay screenshot. Use the unchanged Projection.json and Gameplay_Scale.json. The in-game crop remains 716.8 × 403.2 atlas pixels at 16:9. Pan it with the ball. Use nearest sampling to preserve pixel edges.
3. The baked plate includes the pitch, stands, seats, barriers and boards. Do not place the earlier V3/V8/V9 overlays on top of it; they are already included. It includes no fans, players, ball, HUD or goal meshes, so these can be composed correctly in gameplay.
4. The original goal assets for both ends and all three lighting stages are in Goals/. Follow Goal_Layout.json for position and depth. Use Close_6x for match zoom. Draw the shadows and rear nets behind the appropriate players and ball and place the front frames at their original depth. Use the included preview only to check the look, not as a texture.
5. Filenames use Daylight, Sunset and Floodlit_Night. Manifest hashes identify the exact exported PNGs.

Previews show a sample of clubs and lighting conditions at the locked gameplay zoom. Quality_Report.json confirms full roster coverage and seat-color and pitch-pixel checks. No engine files are altered by this delivery.
