# Validation record — 2026-09-20

## Completed

- Inspected the actual CE singleplayer code at `de3c38ed8f14749472675837d809bab71322752b`; no assumption that it matches current Valve/master.
- Compiled `bs2_tiltshift_ps20b.fxc` using the locally installed Source SDK Base 2013 Singleplayer `shadercompile.exe`. Fresh VCS output, version 6, one static/dynamic combination. This is an actual compiled shader, not a renamed HLSL file.
- Generated the Episodic solution with CE's `vpc.exe /episodic /2013 +game /mksln bs2_tiltshift.sln`. Generated project targets v143 and contains the new client source/header.
- PowerShell parser validated build and installation scripts. Installer refuses a missing client build before creating a target directory.
- CPU reference checks passed: normalized Gaussian kernel, zero-strength identity with saturation=1, flat-color preservation, sharp-band preservation, finite output.
- Rendered before/after images on a sharp public-domain elevated Seattle photograph and the user's station screenshot.

## Independent appearance review

The reviewer independently viewed Brohattan, Baseball and Arc in photographer Ben Thomas's Cityshrinker gallery, then inspected our actual previews.

**Round 1: rejected.** The original 17-tap blur caused grid patterns in distant buildings and combing around ceiling lights. Simply weakening the blur to conceal the artifact was not accepted.

**Revision:** 65 samples per pass, retaining the same blur support and focus band. Radius capped at 32 screen pixels so sample spacing stays at or below one source pixel.

**Round 2: approved for offline visual quality.** Reviewer: “The revised Seattle and station previews resolve the earlier grid and combing artifacts. They retain a crisp focus band, smooth foreground/background defocus, gentle transitions and restrained color. Seattle gives the intended miniature impression when compared with Ben Thomas’s photographic references.”

## Not yet completed

- C++ compilation/linking: no installed Visual Studio C++ toolchain was found. User was asked whether to install it.
- GitHub Actions: branch creation and blob upload both returned HTTP 403, “Resource not accessible by integration.” No remote files or branches were changed.
- In-engine launch, screenshots, frame-time measurements, HDR/LDR, HUD, save/load and resolution-change validation.

The visual approval must not be presented as approval of an in-engine result. The source patch and compiled pixel shader alone do not replace the need to build the client DLL. Runtime status will be updated if build access becomes available.
