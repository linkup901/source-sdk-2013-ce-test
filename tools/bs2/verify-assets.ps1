$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$mod=Join-Path $root 'sp/game/mod_episodic'
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'asset-manifest.json') -Raw | ConvertFrom-Json
foreach($file in $manifest.files){
 $path=Join-Path $mod $file.path
 if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing runtime asset: $($file.path)"}
 if($file.text){
  $canonical=[IO.File]::ReadAllText($path).Replace("`r",'')
  $sha=[Security.Cryptography.SHA256]::Create()
  try {$hash=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))).Replace('-','')} finally {$sha.Dispose()}
 } else {$hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
 if($hash -ne $file.sha256){throw "Runtime asset changed without updating its manifest: $($file.path)"}
}
foreach($file in @('bs2_phase2.fgd','bs_soft_fog.fgd','particles/particles_manifest.txt','cfg/skill.cfg')){
 if(!(Test-Path -LiteralPath (Join-Path $mod $file))){throw "Missing setup file: $file"}
}
foreach($include in Select-String -LiteralPath (Join-Path $mod 'bs2_phase2.fgd') -Pattern '@include "([^"]+)"'){
 $relative=$include.Matches[0].Groups[1].Value
 if(!(Test-Path -LiteralPath (Join-Path $mod $relative))){throw "Missing Hammer definition: $relative"}
}
$particles=Get-Content -LiteralPath (Join-Path $mod 'particles/particles_manifest.txt') -Raw
if($particles -notmatch '"particles/bs2_breath.pcf"'){throw 'Breath particles are not registered.'}
Write-Output "Verified $($manifest.files.Count) runtime asset hashes, Hammer includes and particle registration."
