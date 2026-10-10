$ErrorActionPreference='Stop'
Push-Location sp/src
try {
 & ./devtools/bin/vpc.exe /episodic /2013 +shaders /mksln river.sln
 if($LASTEXITCODE){throw 'VPC failed'}
 $project=Get-ChildItem materialsystem/stdshaders/*episodic*.vcxproj | Select-Object -First 1
 if(!$project){throw 'Shader project missing'}
 & msbuild $project.FullName /m:2 /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /verbosity:minimal
 if($LASTEXITCODE){throw 'Shader DLL build failed'}
} finally {Pop-Location}
if(!(Test-Path sp/game/mod_episodic/bin/game_shader_dx9.dll)){throw 'DLL missing'}
