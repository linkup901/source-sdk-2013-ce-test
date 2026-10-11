$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$modRoot=Join-Path $repoRoot 'sp\game\mod_episodic'
$packageRoot=Join-Path $repoRoot 'terrain4-package'
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'runtime\bin') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'runtime\cfg') | Out-Null
Copy-Item -LiteralPath (Join-Path $modRoot 'cfg\terrain_start.cfg') -Destination (Join-Path $packageRoot 'runtime\cfg')
Copy-Item -LiteralPath (Join-Path $modRoot 'bin\game_shader_dx9.dll') -Destination (Join-Path $packageRoot 'runtime\bin')
foreach($folder in @('terrain4','terrain_mines_261011','minekit')){
 $target=Join-Path $packageRoot ('runtime\materials\bs2\'+$folder)
 New-Item -ItemType Directory -Force -Path $target | Out-Null
 $patterns=@('*')
 if($folder -eq 'minekit'){$patterns=@('mountain_rock.vtf','mountain_rock_normal.vtf')}
 if($folder -eq 'terrain_mines_261011'){$patterns=@('downhill_gravel*.vtf','forest_loam*.vtf','trail_mud*.vtf')}
 foreach($pattern in $patterns){Copy-Item -Path (Join-Path $modRoot ('materials\bs2\'+$folder+'\'+$pattern)) -Destination $target}
}
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'runtime\shaders\fxc') | Out-Null
foreach($program in @('bs2_terrain4_vs30','bs2_terrain4_ps30','bs2_river_vs30','bs2_river_ps30')){
 Copy-Item -LiteralPath (Join-Path $modRoot ('shaders\fxc\'+$program+'.vcs')) -Destination (Join-Path $packageRoot 'runtime\shaders\fxc')
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install-Terrain4.ps1') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'make-mask.py') -Destination $packageRoot
foreach($helper in @('Prepare-TerrainCompile.ps1','Compile-TerrainMap.ps1')){
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $helper) -Destination $packageRoot
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs\BS2-Terrain4.md') -Destination (Join-Path $packageRoot 'README.md')
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot 'examples') | Out-Null
Get-ChildItem -LiteralPath (Join-Path $repoRoot 'examples\terrain4') -File | ForEach-Object {
 Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $packageRoot 'examples')
}
$records=Get-ChildItem -LiteralPath (Join-Path $packageRoot 'runtime') -Recurse -File | ForEach-Object {
 @{path=$_.FullName.Substring($packageRoot.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
}
@{commit=(& git -C $repoRoot rev-parse HEAD);files=@($records)} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $packageRoot 'PACKAGE-MANIFEST.json')
