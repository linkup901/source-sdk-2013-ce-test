# BS2 tilt-shift selective focus

Implementation target: `linkup901/source-sdk-2013-ce-test`, maintained singleplayer `sdk2013ce` build (with Episodic support), base commit `de3c38ed8f14749472675837d809bab71322752b`.

This is a screen-space miniature effect. It does not move the camera, supply an isometric movement system, or simulate depth-aware optical focus. A fixed elevated view makes it look most convincing. The same screen band can blur different parts of a tall object; this is intentional for this inexpensive-to-integrate version.

## Rendering path

`CViewRender::RenderView` calls `BS2_DrawTiltShift` once on the main scene after the engine's tone mapping/screen-space effects and before HUD/VGUI. It skips cubemap generation and Hammer's editor view. Reflections and camera textures do not receive the extra pass. The engine's existing `screenspace_general` material shader loads our compiled `bs2_tiltshift_ps20b.vcs`.

Two framebuffer snapshots avoid sampling a render target while writing to it. Pass one blurs horizontally; pass two blurs vertically and applies optional saturation. A smoothstep mask gives exactly zero blur inside the sharp band and continuous blur outside it. The shader uses 65 Gaussian samples per pass to avoid the visible sampling grid in the rejected 17-tap prototype. Radius is capped at 32 screen pixels so adjacent samples never exceed one source texel. It uses sRGB texture decoding and output encoding. No depth texture, new shader DLL, server entity or FGD is required.

130 samples per affected pixel makes this a quality-first prototype: **GPU cost is not measured yet**. Radius is measured in actual screen pixels, not a fixed percentage; higher-resolution output may need radius adjustment. Do not increase the cap without changing sampling or adding a proper prefiltered blur pyramid.

## Build

Use this CE checkout, not the current Valve/master multiplayer tree. Generate with `/sdk2013ce`, as the repository's own build script does: the older `/episodic` target omits CE's modern-compiler fixes. The CE project scripts target the VS2022 v143 toolset, despite VPC's `/2013` project-format argument. The build script compiles individual projects in dependency order because the legacy solution generator can omit build configuration mappings.

Install Visual Studio 2022 or Build Tools with C++ v143 x86/x64 tools, C++ MFC, and a Windows 10/11 SDK, as required by CE. Keep Source SDK Base 2013 Singleplayer installed in Steam.

Run from the repository root in PowerShell:

```powershell
.\tools\tiltshift\build_client.ps1
.\tools\tiltshift\build_shader.ps1 -SdkBin 'C:\SteamLibrary\steamapps\common\Source SDK Base 2013 Singleplayer\bin'
```

The compiled pixel shader is also included, so rebuilding it is needed only after changing the FXC. The shader-only build does not need Visual Studio or Perl. It creates the same single-combination worklist format as CE's `fxc_prep.pl`, then calls Valve's installed shader compiler. The client build produces matching DLLs in `sp/game/mod_sdk2013ce/bin`, verifies both exist, and stages them in `sp/game/mod_episodic/bin` alongside the effect assets. Do not mix these binaries with retail HL2's engine.

## Install a separate test mod

After the build succeeds:

```powershell
.\tools\tiltshift\install_preview.ps1 `
  -SdkRoot 'C:\SteamLibrary\steamapps\common\Source SDK Base 2013 Singleplayer' `
  -ModDir 'C:\Users\linkup\Desktop\BS2-TiltShift-Preview'
```

The destination must not already exist. This copies the built files and writes a gameinfo that uses its own `bin`, Steam app 243730, and the SDK installation's EP2/Episodic/HL2 assets. It leaves the installed game and existing particle files unchanged. The SDK content directories must contain your EP2 assets and maps; `part3dot` has previously been installed under SDK/ep2/maps.

With Steam running, launch:

```powershell
& 'C:\SteamLibrary\steamapps\common\Source SDK Base 2013 Singleplayer\hl2.exe' `
  -game 'C:\Users\linkup\Desktop\BS2-TiltShift-Preview' -novid -console +map part3dot
```

The isolated preview's autoexec enables the preset. To install into your own **matching CE mod** manually, deploy `bin/client.dll`, `bin/server.dll`, `materials/effects/bs2_tiltshift.vmt`, `shaders/fxc/bs2_tiltshift_ps20b.vcs` and both `cfg/bs2_tiltshift_*.cfg` from `sp/game/mod_episodic`. Then run `exec bs2_tiltshift_on` in the console. An FGD alone cannot install the shader or render hook.

## View and tune

```text
exec bs2_tiltshift_on
bs2_tiltshift_enable 0
bs2_tiltshift_enable 1
```

Compare enabled and disabled from the same camera position. For an elevated inspection without implementing the new camera yet, use `sv_cheats 1`, `noclip`, fly above an outdoor area, then look down. Keep the intended subject inside the sharp band.

| Console setting | Default | Meaning |
| --- | --- | --- |
| `bs2_tiltshift_center` | 0.55 | Focus center, top=0, bottom=1 |
| `bs2_tiltshift_width` | 0.24 | Full width of completely sharp band, in screen heights |
| `bs2_tiltshift_falloff` | 0.25 | Soft transition outside each edge of that band |
| `bs2_tiltshift_angle` | 0 | Focus-band angle, degrees; positive slopes down to right |
| `bs2_tiltshift_radius` | 24 | Blur support in screen pixels, maximum 32 |
| `bs2_tiltshift_strength` | 1 | Blur multiplier, 0–1 |
| `bs2_tiltshift_saturation` | 1.05 | 1 preserves color; 1.05 is subtle enhancement |
| `bs2_tiltshift_debug` | 0 | 1 shows mask: black sharp, white maximum blur |

For a wider gameplay-safe area, try width `0.32`. For subtler blur try strength `0.65`. For a tilted band try angle `5`. To make strength zero a true image identity also set saturation to `1`. `enable 0` bypasses the whole effect regardless of other settings.

ConVars are not archived: put a chosen preset in a cfg and `exec` it from your mod's autoexec if desired. Controls are global until changed, including across map loads. The effect does not automatically follow the player's on-screen position; keep the camera's subject at the configured band center.

## Hammer: existing entities are enough

Use your current HL2/EP2 FGD. Add a `point_clientcommand` named `tiltshift_commands`. Add a `logic_auto` with these outputs (Target = `tiltshift_commands`, Input = `Command`, delay 0):

| Output | Parameter |
| --- | --- |
| OnMapSpawn | `bs2_tiltshift_enable 1` |
| OnLoadGame | `bs2_tiltshift_enable 1` |

Additional outputs can set `bs2_tiltshift_center 0.55`, width, radius, etc. A trigger/relay can send `bs2_tiltshift_enable 0` for scenes that should stay fully sharp. Configure each map's initial state explicitly to avoid inheriting the previous map's settings. Map-driven settings are global, not per-player, which is suitable for this singleplayer target. The flags on the controls allow the engine to accept these server-triggered client commands.

Hammer's 3D editor viewport will not show the effect; compile/run the map in the built mod.

## Checks and limitations

See `VALIDATION.md` for actual completed validation. Offline previews are labeled as such; they are not screenshots of the effect running in Source. Remaining runtime checks include HDR/LDR, HUD sharpness, video-mode changes, save/load, map changes, and GPU frame time. A black/purple image suggests missing VMT/VCS or an incompatible shader. `Unknown command bs2_tiltshift_enable` means you loaded a client DLL without the hook. `Can't load library client` is a mod runtime/binary setup problem, not the FXC.

## Visual references and preview

The independent reviewer used Ben Thomas's [Cityshrinker photographs](https://benthomas.co/cityshrinker) (Brohattan, Baseball, Arc). Those copyrighted photographs are not bundled. The demonstration input [Aerial view of downtown Seattle](https://commons.wikimedia.org/wiki/File:Aerial_view_of_downtown_Seattle.jpg), by Dcoetzee (2009), was released into the public domain. The station input is the user's own `trainstationseq.jpg`.

`preview.py` evaluates the shader's weights and mask with bilinear sampling and sRGB conversions on the CPU, including intermediate 8-bit quantization. It requires Python, numpy and Pillow. It tests kernel normalization, disabled-blur identity, flat-color preservation and sharp-band preservation. It is useful for appearance review, not a substitute for compiling C++ or running the engine.

