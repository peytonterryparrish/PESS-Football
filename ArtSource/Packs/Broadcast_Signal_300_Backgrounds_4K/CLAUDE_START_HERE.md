# Broadcast Signal: 300 UHD background plates

One approved art direction: **07 Broadcast Signal**. Period CRT scanlines,
fine pixel dither, purple/blue with red and ice accents. English football
motifs include netting, terrace roofs, floodlights, supporter textiles, and
logo-free footballs. No game titles, club crests, numbers or menu art are
baked into the background plates. This revision removes the kicking-player
silhouette and its trail entirely, while keeping the existing asset IDs and
filenames. No foreground figures are baked into the plates.

- Backgrounds_4K: 300 uniquely composed variants, divided into six collections of
  50. Each collection has five distinct layouts, each varied across ten
  framing/light/detail passes. Files are 3840×2160 sRGB PNGs. The
  pixel art is drawn on a 1280×720 logical grid and expanded 3× without smoothing.
- Background_Manifest.csv / .json: exact path, ID, layout, palette, dimensions,
  and measured center luminance. Use IDs for bindings.
- Previews: single contact sheet of all 50 plates per collection, plus overview
  sheets with one example from each layout. Labels are only on preview sheets.
- Source: original editable procedural compositions, built using Python,
  Pillow and NumPy. Generated from new game-graphics geometry and textures;
  no approved preview pixels are baked into the output.

## Use in Unreal
Treat these as decorative 16:9 menu backgrounds, not stadium gameplay atlases.
Keep the existing interactive UI on separate layers above them. Protect the
center approximately 55%–60% of the canvas with the built-in dark field.
Position large headings, lists and buttons inside the UI safe area; use
the collection labeled Quiet Menu Plates if you need the least visual activity.
Keep aspect ratio; crop surplus at edges where needed. Use point sampling when
you want hard pixel clusters, and test the scanline cadence at target output
sizes. Avoid stretching across ultrawide layouts or centering new decorative
art under text. No assets have been imported into Unreal by this delivery.
