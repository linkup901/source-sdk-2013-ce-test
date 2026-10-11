# BS2 four-way terrain review

Built on `codex/bs2-combined` commit 5c1ccfd, including the October 6 Phase 2 code and soft fog. Isolated shader review branch: `codex/bs2-terrain4-review-20261011`. No campaign VMF or installed mod DLL has been replaced.

## Materials chosen from the schematics and current VMFs

The downhill trail and forest-transition schematic sheets show a 320 HU gravel centre, flush shoulders, forest loam, muddy low spots/ruts, and exposed rock around outcrops. The current downhill VMFs already use `bs2/dhv3/blend_tmloam_rock` and gravel/forest pairs. This prototype uses their existing texture assets:

- Gravel: `bs2/terrain_mines_261011/downhill_gravel`
- Loam: `bs2/terrain_mines_261011/forest_loam`
- Mud: `bs2/terrain_mines_261011/trail_mud`
- Rock: `bs2/minekit/mountain_rock`

The source-texture manifest records the exact input hashes. Primary application sites are road-to-forest shoulders, mud ruts/drainage low spots, and rocky road apexes. A four-way material is unnecessary on uniform rock walls or timber structures.

## Hammer++ workflow

Hammer displacement Paint Alpha supplies one scalar blend channel, which normally controls two textures. AI does not change that editor limitation. This material uses an independently authored **RGBA weight mask**, with four channels: R gravel, G loam, B mud, A rock. Alpha is a weight, not transparency. The shader normalizes all four weights, so a mixture remains stable. All-zero pixels should be avoided; they become an equal mixture through a small numerical floor.

1. Apply `bs2/terrain4/trail_fourway` to a brush top or displacement. It is an opaque terrain material, not a model material.
2. Make a uniquely named VMT/mask pair for each terrain region. `$maskbounds` is `[minimumX minimumY width height]` in world units. Example patch: `[-512 -512 1024 1024]`.
3. The mask is projected onto world XY, independently of the repeating surface UV. Adjacent displacements using the same VMT share the same mask and therefore blend continuously. Use identical texture alignment/scale across their boundaries. Sew geometry separately.
4. The four textures still tile according to Hammer face UV. `$layerscales` provides four independent tiling multipliers. `$blendcontrast` changes the weight transition sharpness; 1 retains the authored mask.
5. Paint the mask in an image editor that preserves RGBA, or generate it from surveyed terrain/road geometry. Rebuild its **linear, clamp-to-edge, mipmapped RGBA8888** VTF. Do not gamma-correct, JPEG-compress, or discard alpha from the mask.
6. Refresh `%tooltexture` from the weighted color blend. It provides a material swatch, **not a spatially accurate world-mask preview on the runtime face**, because Hammer repeats it with the face UV. For accurate placement, open `bs2_terrain4_hammer_preview_261011.vmf`: this separate copy uses a stock preview material and one mask-sized UV tile. It shows the whole blend layout in Hammer++. Keep that preview UV separate from the runtime texture tiling. Recompile geometry/lightmaps after material changes, then inspect the runtime result.

The example generator demonstrates an S-shaped centre, loam shoulders, a muddy rut, and rock along the outer edges. It is a test pattern, not a mask already fitted to the entire campaign. For adoption, survey the latest saved VMF, generate a mask covering its region, and review a separate VMF copy before replacing authored materials.

## Build and preview

`build-example.py` creates the mask, material, matching Hammer preview, source hashes and a small real displacement test map. `build-vcs.ps1` compiles the two GPU programs with the installed SDK's shader compiler. The shader DLL was built by GitHub Actions, run 38141457243. `Play Terrain Review.cmd` opens the isolated example for walking; `Launch-Terrain.ps1 -Capture` records an overview and exits.

The shader needs the matching `game_shader_dx9.dll`, `bs2_terrain4_vs30.vcs`, `bs2_terrain4_ps30.vcs`, VMT, mask and all eight source color/normal textures. The isolated DLL is built from the combined repository's shader project and this addition; it does **not** yet consolidate the separate river/glass shader DLL projects. Do not overwrite a production custom shader DLL with this test DLL.

## Limits and validation

Four base-color samples, four normal samples, one mask sample and three directional lightmap samples per pixel. This is more expensive than a two-texture transition; avoid applying it everywhere without measuring. Normals are blended before directional lightmap shading. The material is opaque, uses world-space masks, has SM3 support and a single-texture fallback on older hardware. LDR geometry, visibility, lighting compilation and an in-game render have been checked.

This prototype has range-fog support. Underwater height fog, flashlight passes, HDR correctness, steep/vertical rock projection, large campaign performance and four-material physical footstep classification need additional integration checks. There is no parallax, automatic puddle reflection or slope material selection in this prototype. Its single `$surfaceprop` is gravel across the patch. A visually muddy channel is not a water volume and cannot alone trigger wheel splashes.

## Fog DLL answer

**Yes, for `client.dll` and `server.dll` from combined soft-fog run 38140696667, built at 5c1ccfd.** That commit descends from the October 6 Phase 2/HUD/font/vein changes. The build succeeded. The pair is available in `../soft-fog-review-20261011/combined-dlls/`. Earlier standalone fog-preview DLLs are not the combined build. Preserve existing Phase 2 assets, back up the installed DLL pair, and replace both together; the code confirmation does not validate unrelated asset replacements.
