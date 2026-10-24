param([Parameter(Mandatory)][string]$ModPath,[string]$PackagePath=$PSScriptRoot)
$ErrorActionPreference='Stop'
$mod=(Resolve-Path -LiteralPath $ModPath).Path
$package=(Resolve-Path -LiteralPath $PackagePath).Path
if(!(Test-Path -LiteralPath (Join-Path $mod 'gameinfo.txt'))){throw 'ModPath must be an existing Source SDK Base 2013 Singleplayer CE mod.'}
if(Get-Process hl2 -ErrorAction SilentlyContinue){throw 'Close the Source game before replacing DLLs.'}
$runtime=(Resolve-Path -LiteralPath (Join-Path $package 'runtime')).Path
$manifest=Get-Content -LiteralPath (Join-Path $package 'PACKAGE-MANIFEST.json') -Raw | ConvertFrom-Json
if(!$manifest.files){throw 'Empty package manifest.'}
$validated=@()
$seen=@{}
# Validate every hash and destination before backing up or changing any mod file.
foreach($file in $manifest.files){
 $relative=[string]$file.path
 if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)' -or $relative.Contains(':')){throw "Invalid package path: $relative"}
 if($seen.ContainsKey($relative)){throw "Duplicate package path: $relative"}
 $seen[$relative]=$true
 $source=[IO.Path]::GetFullPath((Join-Path $runtime $relative))
 $target=[IO.Path]::GetFullPath((Join-Path $mod $relative))
 if(!$source.StartsWith($runtime+'\',[StringComparison]::OrdinalIgnoreCase) -or !$target.StartsWith($mod+'\',[StringComparison]::OrdinalIgnoreCase)){throw "Package path escapes its root: $relative"}
 if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.sha256){throw "Package hash mismatch: $relative. Nothing installed."}
 $validated+=@{path=$relative;source=$source;target=$target;sha256=$file.sha256}
}
foreach($required in @('bin/client.dll','bin/server.dll','bin/game_shader_dx9.dll','particles/particles_manifest.txt','credits.txt','bs2_phase2.fgd','bs_soft_fog.fgd')){
 if(!$seen.ContainsKey($required)){throw "Incomplete package: $required"}
}
$particlePath=Join-Path $mod 'particles/particles_manifest.txt'
$newParticles=Get-Content -LiteralPath (Join-Path $runtime 'particles/particles_manifest.txt') -Raw
if(Test-Path -LiteralPath $particlePath){
 $merged=Get-Content -LiteralPath $particlePath -Raw
 $lastBrace=$merged.LastIndexOf('}')
 if($lastBrace -lt 0){throw 'Existing particle manifest is malformed. Nothing installed.'}
 foreach($match in [regex]::Matches($newParticles,'"file"\s+"([^"]+)"')){
  $entry=$match.Groups[1].Value
  # ! means preload; either form already registers this effect.
  $registered=@([regex]::Matches($merged,'"file"\s+"!?([^"]+)"') | ForEach-Object {$_.Groups[1].Value})
  if($entry.TrimStart('!') -notin $registered){
   $lastBrace=$merged.LastIndexOf('}')
   $merged=$merged.Insert($lastBrace,"    `"file`" `"$entry`"`r`n")
  }
 }
} else {$merged=$newParticles}
$backup=Join-Path $mod ('installation-backups/bs2-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$installed=@();$preserved=@()
foreach($file in $validated){
 # Keep a campaign's existing credits and tuned NPC skill settings.
 if($file.path -in @('credits.txt','cfg/skill.cfg','cfg/skill_episodic.cfg','cfg/skill_manifest.cfg') -and (Test-Path -LiteralPath $file.target)){$preserved+=$file.path;continue}
 if(Test-Path -LiteralPath $file.target){
  $saved=Join-Path $backup $file.path
  New-Item -ItemType Directory -Path (Split-Path $saved) -Force | Out-Null
  Copy-Item -LiteralPath $file.target -Destination $saved
 }
 New-Item -ItemType Directory -Path (Split-Path $file.target) -Force | Out-Null
 if($file.path -eq 'particles/particles_manifest.txt'){
  Set-Content -LiteralPath $file.target -Value $merged -Encoding ascii
 } else {
  Copy-Item -LiteralPath $file.source -Destination $file.target
  if((Get-FileHash -LiteralPath $file.target -Algorithm SHA256).Hash -ne $file.sha256){throw "Installed hash mismatch: $($file.path). Restore from $backup"}
 }
 $installed+=@{path=$file.path;sha256=(Get-FileHash -LiteralPath $file.target -Algorithm SHA256).Hash}
}
@{commit=$manifest.commit;installed=(Get-Date).ToString('o');backup=$backup;files=$installed;preserved=$preserved} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $mod 'BS2-INSTALL.json')
Write-Output "Installed combined BS2 runtime. Backup: $backup"
