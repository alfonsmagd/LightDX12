# Prompts de arte provisional

Estas imagenes se generaron con el modo integrado de ImageGen y son recursos provisionales para validar composicion, escala y parallax. No proceden del arte de ningun juego publicado.

## `western_level_background_v2.png`

```text
Use case: stylized-concept
Asset type: production background bitmap for a side-scrolling 2D arcade game
Primary request: create an original western railroad frontier level background suitable for horizontal scrolling
Input images: Image 1 is a mood, palette, and pixel-art density reference only; do not copy its characters, logo, UI, text, exact train, or composition
Scene/backdrop: dramatic desert canyon, distant mesas, stormy blue-gray clouds, an old frontier settlement and railroad elements in the middle distance, dusty walkable ground across the lower part
Style/medium: crisp high-detail 16-bit arcade pixel art with a Neo Geo-era feeling, coherent pixel clusters, strong silhouettes, production game background rather than concept-board presentation
Composition/framing: 16:9 landscape, strict side view, horizon in the upper third, layered depth for parallax, the lower 35 percent is a clear continuous gameplay lane with restrained detail so characters remain readable
Lighting/mood: late-afternoon warm amber light against cool storm clouds, adventurous and gritty
Color palette: burnt umber, ochre, charcoal, muted steel blue, dusty gold
Constraints: environment only; no people, horses, enemies, weapons, HUD, typography, borders, panels, logos, trademarks, or watermark; no checkerboard; fill the complete canvas; original design
Avoid: photorealism, painterly blur, isometric perspective, modern objects, excessive foreground clutter, fake UI, any readable text
```

## `western_ground_foreground_v2.png`

```text
Use case: stylized-concept
Asset type: seamless horizontally tileable foreground layer for a side-scrolling 2D arcade game
Primary request: an original dusty western trail foreground tile that scrolls under the characters
Input images: Image 1 is the already selected game background; match only its pixel density, palette, and lighting
Scene/backdrop: transparent canvas above, with compact dusty ground, a few embedded stones, sparse dry grass, and occasional short weathered plank fragments confined to the lower band
Style/medium: crisp high-detail 16-bit arcade pixel art, coherent hard pixel clusters, no soft painterly blur
Composition/framing: wide landscape tile; only the lower 38 percent contains ground art; upper 62 percent must be genuinely transparent; left and right edges must join seamlessly with no obvious focal object at either edge
Lighting/mood: warm late-afternoon amber light matching the reference
Color palette: ochre dirt, burnt umber shadows, muted dusty gold
Constraints: actual alpha transparency above the ground; environment only; horizontally seamless; no people, characters, horses, buildings, train, HUD, text, logos, borders, checkerboard, or watermark
Avoid: vertical walls, large props, high silhouettes behind character feet, gradients in the transparent region
```

## `western_midground_v3.png`

```text
Use case: production game asset
Asset type: horizontally tileable midground parallax layer for a side-scrolling 2D arcade game
Primary request: replace the flawed midground reference with a clean original western railroad strip containing only sparse telegraph poles, a low split-rail fence, thin railroad tracks, one small distant water tank, a few compact crates, and sparse dry scrub
Input images: Image 1 is the approved far background and defines pixel density, side-view perspective, warm/cool palette, and lighting. Image 2 is a flawed previous midground; retain only the general scale of poles and fences, not its pale mountains or checker pattern
Style/medium: crisp detailed 16-bit arcade pixel art, coherent hard pixel clusters, exact side view
Composition/framing: 16:9 wide landscape, props mostly within the lower 45 percent; telegraph poles may extend to the middle; strong horizontal rhythm; keep the gameplay lane open; left and right edges visually tileable
Background/alpha workflow: place the isolated artwork over one perfectly flat, absolutely uniform pure chroma-green RGB #00FF00 background covering every empty pixel. No transparency, no checkerboard, no gradients, shadows, texture, halos, noise, vignettes, or variations in the green background
Constraints: no mesas, mountains, cliffs, skyline, sky, clouds, large buildings, train, continuous opaque ground carpet, people, characters, horses, HUD, text, logos, borders, checkerboard, or watermark
Avoid: pale pink rock silhouettes, floating scenery, photorealism, painterly blur, perspective road, isometric view
```
