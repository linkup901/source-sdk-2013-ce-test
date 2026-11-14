BS2_Glass / BS2_GlassBrush shader build
- DLL: VPC /episodic /2013 +shaders /mksln, then MSBuild game_shader_dx9_episodic (v143, Win32 Release).
- GPU programs: SDK shadercompile.exe -nompi -nop4 over filelist.txt (staged with the .fxc files and
  common_*.h, shader_constant_register_map.h, dx_proxy.dll). Outputs bs2_glass_vs30/ps30.vcs and
  bs2_glassbrush_vs30/ps30.vcs -> <mod>\shaders\fxc. No Perl needed (combo indices set by hand in the .cpp).
- Working build scripts: C:\Users\linkup\Desktop\MountainPassMap\bs2maps\BS2 clips\glass-shader-20261114-01\scripts
