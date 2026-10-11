# Soft fog performance review — 11 October 2026

## Verdict
The prototype is visually useful but too expensive to cover an entire downhill route with overlapping enabled volumes. Start with ONE visible volume at 32 slices. Keep campaign adoption experimental until driving tests in the actual downhill maps confirm the frame budget. The example's regular play preset remains 48 slices; 32 was tested in the separate fogbench32 copy, not silently adopted into the example or campaign.

## Measurements
Recorded fixed-camera demos in the separate forest-road example, approximately 600 recorded frames each. Source timedemo reports roughly 497–498 measured frames after its startup exclusion. Three playback runs per case, median reported average frame time. LDR, windowed, VSync disabled and frame limit removed. The 32-slice copy differs in the fog layers key. This is a rendering microbenchmark, not a driving benchmark or 1% low measurement.

Machine: Intel Core i5-12600K. Installed GPUs include RTX 3070 Ti and Intel UHD 770; the active rendering adapter was not independently confirmed. Background desktop applications remained open.

| Case | 1280×720 median frame time | 1920×1080 median frame time | Added cost at 1080p |
|---|---:|---:|---:|
| Volume disabled | 0.49 ms | 0.27 ms | — |
| 48 slices, scattering off | 8.69 ms | 8.91 ms | 8.64 ms |
| 48 slices, scattering on | 9.44 ms | 9.65 ms | 9.38 ms |
| 32 slices, scattering on | not tested | 6.11 ms | 5.84 ms |

Subtracting baseline is an approximate effect cost. The extremely fast baseline is this small uncapped scene and must not be used as expected campaign FPS. Resolution changes produced little fog-time difference; CPU geometry generation appears dominant, but this is an inference, not a CPU/GPU profiler result. Results are warm repeated averages, not frame-time percentiles.

## Budget for downhill maps
A 60 FPS target allows 16.67 ms per frame. One 48-slice scattering volume consumes about 56% of that budget; one 32-slice volume about 35%. On this machine, a 32-slice bank leaves roughly 10.8 ms for the rest of the frame, assuming similar costs. Test in the full campaign with vehicles, NPCs, particles, reflections and projected lights active before relying on that estimate.

Use 32 slices for the first campaign trial, localized in hollows beside the road. Plan for one volume visible at once. Several banks may be placed along a route, but enable/disable them only while out of sight; current inputs switch instantly. Avoid overlapping visible banks. Two 48-slice banks would plausibly exceed a 60 FPS budget by themselves: this is extrapolation, not measured. Reducing density or light intensity does not substantially reduce geometry work. Reducing size primarily reduces visible coverage/overdraw; the mesh still uses the same tessellation count when drawn. Disabled volumes skip DrawModel, but their entities still exist and are always transmitted.

32-slice rendering quality was not separately approved from new comparison pictures; choose 48 only where the quality difference justifies the measured cost. No moving buggy-headlight implementation or shadowed scattering is included.

## Source review: where the cost comes from
At grid 32, one volume emits 98,304 triangles and 294,912 vertices per frame at 48 slices, in 48 mesh draws. At 32 slices it emits 65,536 triangles and 196,608 vertices in 32 draws. Density, edge fade, noise, ray direction and scattering are evaluated on the CPU for every emitted vertex, including repeated vertices shared between adjacent triangles. Light position/radius/anisotropy ConVars are also fetched repeatedly inside the vertex loop.

Highest-value follow-up: evaluate each unique grid point once per slice, then reuse it for triangle vertices (1,089 unique points versus 6,144 emitted vertices per slice). Cache light parameters once per draw and add a quality/LOD control. These changes should be measured before claiming savings; they are not implemented by this review.

## Validation and excluded attempts
The first timerefresh test batched draws without normal presentation, produced impossible FPS and exhausted the temporary mesh buffer. Its results are invalid and excluded. A first demo used an overlong map name that Source truncated; those failed results are also excluded. The final demos use short fogbench/fogbench32 map names and completed all playback runs. Logs append across launches; performance-results.json uses only the final three valid measurements of each case.

An additional native-window capture attempt timed out, so independent visual verification of the demo playback window was not completed. Recorded camera settings match the previously inspected in-game comparison view. Full-game driving, multiple-volume stress, low-end hardware and 1% low testing remain outstanding.

Raw results: performance-results.json, perf-baseline/fog/scatter-*.log, perf-scatter32-1920x1080.log. Invalid test logs are preserved separately. Benchmark-Fog.ps1 replays the saved baseline/fog demos at both resolutions. Normal preview capture and frame-limit settings were restored afterward. No campaign VMF or shared mod binary was changed by this review.

