# BS2 combined runtime setup

The combined branch contains Black Hunter, Crabby, the cinematic HUD, infection
effects, focus blur, soft fog, four-layer terrain, river rendering and buggy
water splashes. The required NPC models/materials, eight vein stages, breath PCF
and its sprite sheet, D-DIN fonts and OFL license, and campaign credits are now
included in the source tree. `tools/bs2/asset-manifest.json` records their hashes.
The model derivatives use Valve content and require the licensed HL2/EP2 assets
mounted by a matching Source SDK Base 2013 Singleplayer CE mod.

## Build and install

Use Visual Studio 2022 C++ v143 x86/x64 tools, MFC and a Windows SDK. From the
repository root, with the C++ developer environment available:

```powershell
./tools/tiltshift/build_client.ps1
./tools/terrain4/build-dll.ps1
./tools/bs2/package.ps1
```

The included GPU programs can be used unchanged; rebuild them and update the
compilation hash record when editing the terrain/river FXC sources. Packaging
verifies their hashes and the imported assets, and requires all three DLLs.
The `SP_Windows_TiltShift` workflow builds the game and shader DLLs together and
publishes **bs2-combined-install**. Prefer that artifact for a complete install;
the separately named fog/splash DLL artifacts are incremental developer builds.

Extract the install artifact, close the game, and run:

```powershell
./Install-BS2.ps1 -ModPath 'C:/path/to/your/CE-mod'
```

The installer verifies every packaged hash before writing, backs up replaced
files under `installation-backups`, merges particle registrations, preserves
existing credits and skill configuration, and records `BS2-INSTALL.json`.
It installs the matching client/server and terrain/river shader DLLs together.
It does not replace gameinfo, campaign maps, key bindings or autoexec. It does
replace `game_shader_dx9.dll`: a mod with additional custom shaders must build
those in the combined shader project before using this installer.

For a new preview, build first and use `tools/tiltshift/install_preview.ps1`
with a new directory and your Source SDK Base 2013 Singleplayer path. It now
copies the combined assets and Hammer definitions, not only tilt-shift files.
The new-mod particle manifest retains stock EP2 registrations.

## Hammer and gameplay

Load the CE/EP2 base FGD followed by `bs2_phase2.fgd`; the latter now includes
fog, Crabby and Black Hunter as well as the cinematic entities. Remove duplicate
standalone Crabby/Black Hunter/fog FGD entries if they were previously loaded.
Set the Hammer game/material path to the installed mod so it sees the models.

Credits default to the included root `credits.txt`; an existing campaign's file
is preserved. If changing the entity's file key, use a path relative to the mod.
The Crabby model uses the supplied `crabby.ani` and stock mounted headcrab
animations; Black Hunter uses the mounted EP2 Hunter animations.

Compile four-way terrain with `tools/terrain4/Compile-TerrainMap.ps1` or the
documented compile overlay, not directly against the custom runtime material.
See `docs/BS2-Terrain4.md`. River rendering needs an actual water-content volume
under the surface for swimming and buggy splashes; decorative geometry alone
does not provide those contents. Existing campaign maps are not modified.

## Validation and performance

`verify-assets.ps1` checks required asset hashes, FGD includes and breath
registration. `test-install.ps1` checks installation, preservation, backups,
repeat installation, corrupted payload rejection and traversal rejection.
These checks run before the Windows DLL builds in the main workflow.

New fog entities default to 32 slices. Existing maps keep their explicit layer
settings. The renderer evaluates 1,089 unique grid points per slice instead of
6,144 repeated vertices, and reads scattering parameters once per draw. Geometry
and draw counts are unchanged at the same layer setting. This reduces repeated
CPU work; no new frame-time improvement is claimed without a fresh benchmark.
The historical performance review describes the previous renderer. Start with
one visible bank and benchmark the actual campaign before adding overlapping fog.

Physics callback validation now matches pointers against live entities and
caches serialised entity handles; it does not dereference arbitrary game data
or cache the readability of freed addresses. Full ridge-driving, save/load,
map-change, fog appearance and performance checks remain necessary in-game.
