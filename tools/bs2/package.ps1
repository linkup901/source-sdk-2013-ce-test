param([string]$OutputPath)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$mod=Join-Path $root 'sp/game/mod_episodic'
if(!$OutputPath){$OutputPath=Join-Path $root 'bs2-package'}
$output=[IO.Path]::GetFullPath($OutputPath)
if(Test-Path -LiteralPath $output){throw 'Choose a new package directory to avoid stale files.'}
& (Join-Path $PSScriptRoot 'verify-assets.ps1')
& (Join-Path $root 'tools/terrain4/Verify-GpuPrograms.ps1')
foreach($dll in @('client.dll','server.dll','game_shader_dx9.dll')){
 if(!(Test-Path -LiteralPath (Join-Path $mod "bin/$dll"))){throw "Build the combined game and terrain/river shaders first: missing $dll"}
}
foreach($file in @('materials/effects/bs_focus_blur.vmt','materials/effects/bs2_tiltshift.vmt','shaders/fxc/bs_focus_blur_ps20b.vcs','shaders/fxc/bs2_tiltshift_ps20b.vcs')){
 if(!(Test-Path -LiteralPath (Join-Path $mod $file))){throw "Missing screen effect asset: $file"}
}
$runtime=Join-Path $output 'runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach($folder in @('materials','models','particles','resource','shaders','fgd')){
 Copy-Item -LiteralPath (Join-Path $mod $folder) -Destination $runtime -Recurse
}
foreach($folder in @('bin','cfg')){New-Item -ItemType Directory -Path (Join-Path $runtime $folder) -Force | Out-Null}
foreach($dll in @('client.dll','server.dll','game_shader_dx9.dll')){
 Copy-Item -LiteralPath (Join-Path $mod "bin/$dll") -Destination (Join-Path $runtime 'bin')
}
foreach($cfg in @('bs2_tiltshift_on.cfg','bs2_tiltshift_off.cfg','terrain_play.cfg','terrain_start.cfg','skill.cfg','skill_episodic.cfg','skill_manifest.cfg')){
 Copy-Item -LiteralPath (Join-Path $mod "cfg/$cfg") -Destination (Join-Path $runtime 'cfg')
}
foreach($file in @('bs2_phase2.fgd','bs_soft_fog.fgd','credits.txt')){
 Copy-Item -LiteralPath (Join-Path $mod $file) -Destination $runtime
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install-BS2.ps1') -Destination $output
Copy-Item -LiteralPath (Join-Path $root 'docs/BS2-Setup.md') -Destination (Join-Path $output 'README.md')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'asset-manifest.json') -Destination $output
New-Item -ItemType Directory -Path (Join-Path $output 'terrain-tools') -Force | Out-Null
foreach($helper in @('Prepare-TerrainCompile.ps1','Compile-TerrainMap.ps1','make-mask.py')){
 Copy-Item -LiteralPath (Join-Path $root ('tools/terrain4/'+$helper)) -Destination (Join-Path $output 'terrain-tools')
}
Copy-Item -LiteralPath (Join-Path $root 'docs/BS2-Terrain4.md') -Destination (Join-Path $output 'terrain-tools/README.md')
Copy-Item -LiteralPath (Join-Path $root 'examples/terrain4') -Destination (Join-Path $output 'examples') -Recurse
$files=@(Get-ChildItem -LiteralPath $runtime -Recurse -File | ForEach-Object {
 @{path=$_.FullName.Substring($runtime.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
@{commit=(& git -C $root rev-parse HEAD);files=$files} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'PACKAGE-MANIFEST.json')
Write-Output "Combined runtime package: $output"
