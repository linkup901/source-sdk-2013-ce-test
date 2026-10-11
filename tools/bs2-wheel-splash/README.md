# BS2 buggy wheel water spray
Built from codex/bs2-combined at 5c1ccfd (October 6 Phase 2, focus blur and refined soft fog). This isolated review targets the original prop_vehicle_jeep used by the downhill campaign maps. The EP2 prop_vehicle_jalopy keeps its separate shipped WheelSplash system.

## Play
Close any Source game and run Play Buggy Splash.cmd. Press E beside the buggy, drive forward through the shallow crossing, then onto the dry bank. The play map has no automatic driving or exit. VMF: bs2_buggy_splash_play_261011.vmf.

## Behavior
Each wheel checks its actual physics ground contact. Submerged contacts suppress WheelDust. Moving submerged wheels emit small outward droplets and retain the stock surface ripples. Spray begins above 20 HU/sec, scales up to 600 HU/sec, and emits at the existing per-wheel ripple cadence (0.1–0.3 seconds). It uses shipped effects/splash2 and a native client effect callback, so there is no new PCF or particle manifest dependency. Losing ground contact clears stale water state.
The original buggy now retains its ordinary dry dust even when running with the episodic game flag. Other vehicles retain their existing dust policy.

## Use in campaign maps
Install the matching client.dll and server.dll from game/bin together after backing up the existing pair. These contain the combined Phase 2 + fog code plus this wheel change. No new FGD is needed. Existing prop_vehicle_jeep entities work without per-wheel entities or map outputs.
Water must be a compiled water-content brush volume with its top at the visible river surface. A decorative river model or shader alone supplies no water contents; put a matching water volume beneath it. This implementation targets shallow crossings; the engine's deep-water engine-disable behavior remains.

## Adjustment
bs2_wheel_splash_enable 1 (0 disables only the new spray)
bs2_wheel_splash_scale 1 (clamped 0–2; 0.6 for a restrained stream, 1.5 for larger drops)
cl_show_splashes 1 (stock client splash toggle)
With sv_cheats 1, bs2_wheel_splash_debug 1 prints wheel, speed and water-surface height; return it to 0 after diagnosis.

## Validation and scope
Windows combined DLL build 38143109963 passed. Both lab maps passed VBSP, VVIS and LDR VRAD without leaks; the lab reports a harmless missing GameData editor key and the dirt material's absent detail-object definition.
An actual driven buggy crossed the water in the engine. All four wheels emitted spray at about 621–637 HU/sec, surface z=17 (the brush surface is z=16 plus a 1-HU offset). Droplets were visible during the crossing. External point_viewcontrol capture does not reliably override this build's occupied vehicle view; the manual play map is the reliable visual review route.
No campaign VMF was changed and these splash DLLs have not replaced the production preview pair. The separate fog-only combined pair was installed in BS2-TiltShift-Preview on October 11 with a backup and COMBINED-FOG-INSTALL.json.

At default scale, the four wheels spawn roughly 240 droplets/sec at the mean ripple cadence, each lasting at most 0.55 seconds. This is a bounded particle-count estimate, not a measured campaign frame-time benchmark. Long downhill runs, multiplayer, slime and save/load visual continuity have not been profiled.
