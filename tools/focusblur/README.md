# BS2 full-screen focus blur

`env_bs_focus_blur` blurs the live scene like a camera losing focus. It works
with a stationary camera, preserves colour, and leaves the HUD sharp. Blur is
uniform across the scene, including the weapon model; it approximates defocus
with a Gaussian filter rather than using depth-dependent focus or bokeh.

## Build and install

The source files are registered in the CE client/server VPC projects and the
main render path. The compiled pixel shader is included in the repository.
Build both DLLs with the existing `tools/tiltshift/build_client.ps1`. Pushing
this branch also triggers `SP_windows.yml`; its
`bs2_tiltshift_episodic_windows` artifact now includes the focus-blur material,
shader, Hammer FGD files and these instructions alongside the matching DLLs.

From `sp/game/mod_episodic`, deploy these files into a separate test mod first:

- `bin/client.dll` and `bin/server.dll` (both from the same build)
- `materials/effects/bs_focus_blur.vmt`
- `shaders/fxc/bs_focus_blur_ps20b.vcs`
- `bs2_phase2.fgd` and its `fgd` folder for Hammer

Keep the other files required by your existing BS2 effects. Do not replace your
mod's gameinfo. Load `bs2_phase2.fgd` alongside the existing game FGD in Hammer;
it now includes `fgd/bs_focus_blur.fgd`. If you already load the aggregator,
refresh it rather than also loading the blur FGD separately.

To rebuild the shader after changing the FXC:

```powershell
.\tools\focusblur\build_shader.ps1 -SdkBin 'C:\SteamLibrary\steamapps\common\Source SDK Base 2013 Singleplayer\bin'
```

## Hammer controls

Place one `env_bs_focus_blur`, name it `focus_blur`, and initially use radius
`32`. Smaller radii give a gentler effect. Radius is measured in screen pixels.
Set `initialblur` to `0` for a sharp start or `1` for a blurred start.

| Input | Parameter | Behaviour |
|---|---|---|
| BlurIn | Duration in seconds, e.g. 3 | Current blur becomes sharp and stays sharp |
| BlurOut | Duration in seconds, e.g. 3 | Current blur becomes fully blurred and stays blurred |
| SetBlur | Amount from 0 to 1 | Set immediately: 0 sharp, 1 fully blurred |

The duration goes in the output parameter, not its delay. Transitions ease at
both ends, and reversals begin from the current amount. Zero or negative
durations change immediately. Intermediate SetBlur values are supported.

For a blurred opening becoming sharp, set `initialblur = 1`, then send
`logic_auto.OnMapSpawn -> focus_blur.BlurIn`, parameter `3`, delay `0`.

## Combine with a black fade

Use standard `env_fade` entities with colour `0 0 0`, alpha `255`, hold time
`0`, and Modulate unchecked. Opacity and focus have independent timings.

For waking up, name a fade `fade_from_black`, set duration `1.5` and check
Fade From. Set the blur controller's `initialblur = 1`. Send these outputs
from one `logic_auto`:

| Event | Target | Input | Parameter | Delay |
|---|---|---|---|---|
| OnMapSpawn | fade_from_black | Fade | empty | 0 |
| OnMapSpawn | focus_blur | BlurIn | 3 | 0 |

The scene emerges from black while blurred, then becomes sharp. To reveal the
blurred scene first and focus afterwards, give BlurIn a delay of `1.5`.

For a blackout, create `fade_to_black`, duration `2`, Fade From unchecked,
Stay Out checked. Trigger these outputs from one `logic_relay`:

| Event | Target | Input | Parameter | Delay |
|---|---|---|---|---|
| OnTrigger | focus_blur | BlurOut | 2 | 0 |
| OnTrigger | fade_to_black | Fade | empty | 0 |

To show focus loss before darkening, delay Fade by `1` second. To recover,
trigger `fade_from_black.Fade` and `focus_blur.BlurIn` together.

## State and rendering

The server saves and replicates both transition endpoints, start time,
duration and radius. The start time uses FIELD_TIME for save/load time rebasing.
Each new map starts from that map's own controller settings. Use one controller
per map; if several exist, the largest current radius wins. This is a
singleplayer effect affecting the whole view.

The renderer runs after existing screen effects and tilt-shift, before the HUD,
and skips cubemap building and Hammer views. Uniform screen fades coexist with
the blur; in this SDK rendering order, the sampled scene already contains the
fade. Fully sharp frames skip the two blur passes. The shader requires pixel
shader 2_b and uses two 65-tap passes with a maximum radius of 32 pixels.
Profile GPU cost at your target resolutions before release.

## Validation status

The Source SDK shader compiler produced the included VCS. Gaussian weights
sum to 0.9999999996. Transition reference checks cover both directions,
held endpoints, instant changes and reversal continuity. Source integration
and generated CE project inclusion are checked locally.

This PC has no detected C++ build toolchain. The GitHub Actions build after
push is the first client/server compilation for this effect. In-game checks
remain outstanding: appearance, performance, reversal, save/load during each
transition, map changes, fade overlap, HUD, HDR/LDR and resolution changes.
The prepared implementation does not modify any campaign maps.
