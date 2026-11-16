Black buggy speed effects, 16 November 2026.
Copy materials/effects/bs2_buggy_speed* to the mod's materials/effects folder alongside the built client.dll AND server.dll.
Defaults: bs2_buggy_speedfx 1, bs2_buggy_speedlines 0.30, bs2_buggy_speedwarp 0.012.
Thin peripheral lines fade in over 0.25 s during actual server-confirmed boost, out over 0.45 s. The center stays clear.
Warp starts at 30 mph, reaches full strength at 58 mph; smooth radial stretch, no camera shake. HUD is drawn afterward.
Only models/bs2_black_buggy/blackbuggy*.mdl while the local player drives; clears on exit, death, other view entities, and vehicle changes.
Diagnostic: sv_cheats 1; bs2_buggy_speedfx_debug 1 logs mph, actual boost and envelopes. Disable with 0.
Verified in-engine in an isolated overlay at 31, 59 and 60 mph, boost on/off, stopped, and exit. No campaign VMFs changed.
