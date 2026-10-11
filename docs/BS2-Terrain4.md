# BS2 four-way terrain

`BS2_Terrain4` blends the existing downhill gravel, forest loam, trail mud and mountain rock color and normal textures on a brush or displacement. An RGBA mask places the four surfaces in world XY, so a road shoulder, muddy rut and rock outcrop can share one terrain patch. This is opaque ground, not water; visual mud does not trigger buggy splashes.

## Install and try it

Download `bs2-terrain4-install` from the **BS2 Four-Way Terrain Review** workflow on `codex/bs2-combined`. Extract it and run:

```powershell
./Install-Terrain4.ps1 -ModPath 'C:/path/to/BS2-TiltShift-Preview'
```

The installer backs up replaced files, verifies SHA-256 hashes and writes `TERRAIN4-INSTALL.json`. It installs the combined **terrain + river** shader DLL, four GPU programs and the terrain assets. It leaves client/server DLLs and campaign maps alone. The glass design source is separate and unbuilt; this release does not install that design. Retain any unrelated custom shader DLL rather than replacing it blindly.

Compile `examples/bs2_terrain4_play_261011.vmf` with your normal SDK/CompilePal setup, using the installed mod as the game directory. Copy the BSP to the mod's `maps` folder and run:

```text
map bs2_terrain4_play_261011
```

The local delivery also includes a compiled example. The Hammer preview VMF is an editor aid and uses a stock shader; use the **play** VMF for runtime tests.

## Use in Hammer++

No new FGD or entity is needed. Apply `bs2/terrain4/trail_fourway` to a displacement or brush top. Hammer's displacement Paint Alpha still provides only one channel; it cannot paint all four weights. Author the external RGBA mask instead.

1. Copy the VMT and `trail_weights.png` under unique names for your map region. Preserve RGBA: **R = gravel, G = loam, B = mud, A = rock**. Alpha is a material weight, not transparency. Do not flatten it. Pure layers are `(255,0,0,0)`, `(0,255,0,0)`, `(0,0,255,0)`, `(0,0,0,255)`. Mixtures interpolate; black falls back to gravel.
2. Convert your edited PNG to a linear, clamp-to-edge, mipmapped VTF. Python requires Pillow and numpy:

```text
python make-mask.py my_trail.png C:/path/to/mod/materials/bs2/terrain4/my_trail.vtf
```

3. In the new VMT set `$blendmask` to `bs2/terrain4/my_trail` and `$maskbounds` to `[minimumX minimumY width height]` in Hammer world units. The example covers `[-512 -512 1024 1024]`. Image top maps toward minimum Y; image right toward positive X. Width and height must be positive. Mask pixel dimensions must be powers of two.
4. Give adjacent displacements in the same region the same VMT and sew their geometry. Keep face texture alignment consistent. World-mask placement is independent of face UV; changing UV affects detail tiling, not the positions of the material regions.
5. `$layerscales "[1 1 1 1]"` multiplies each layer's tiling. `$blendcontrast` defaults to 1; values above 1 sharpen transitions. `$normalstrength` scales normal detail. `$debugweights "1"` temporarily displays the weights in-game.

The swatch uses `%tooltexture`; it is not a live four-layer Hammer render. `bs2_terrain4_hammer_preview_261011.vmf` demonstrates a baked full-mask preview with separate UV alignment. Inspect the actual shader in-game after compiling. Adopt campaign materials in a review VMF first; the supplied example mask is not fitted to your campaign.

## Lighting, cost and limits

Four normals are blended before directional lightmap shading. The shader supports an additive flashlight pass, shadow depth filtering, LDR/HDR lightmap scaling, range fog and underwater height fog. The same DLL includes the existing `BS2_River` shader. Terrain needs SM3 and falls back to `LightmappedGeneric` on older hardware.

The regular pass uses four color, four normal, one mask and three directional lightmap samples per pixel. Use it for downhill road/forest shoulders, ruts and rock edges; uniform ground can retain cheaper stock materials. World-XY projection is intended for ground, not vertical cliffs. There is no parallax or automatic slope selection.

One VMT has one physical `$surfaceprop` (the preset uses gravel). The visual channels do not independently select footstep, friction or tire-dust types. Use separate physical regions if gameplay requires those differences. This workflow does not change Hammer's painting tools.

Sources, reusable assets, examples, GPU programs and packaging tools are in the repository. The workflow builds the Windows DLL rather than committing it as an opaque source replacement.
