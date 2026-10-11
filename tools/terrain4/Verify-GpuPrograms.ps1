$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'compiled-program-hashes.json') -Raw | ConvertFrom-Json
foreach($record in $manifest.PSObject.Properties){
 $source=Join-Path $root ('sp/src/materialsystem/stdshaders/'+$record.Name+'.fxc')
 $program=Join-Path $root ('sp/game/mod_episodic/shaders/fxc/'+$record.Name+'.vcs')
 $canonical=(Get-Content -LiteralPath $source -Raw).Replace("`r",'').TrimEnd()+"`n"
 $sha=[Security.Cryptography.SHA256]::Create()
 try {$hash=[BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical))).Replace('-','')} finally {$sha.Dispose()}
 if($hash -ne $record.Value.source){throw "GPU source changed without updated compilation record: $($record.Name)"}
 if((Get-FileHash -LiteralPath $program -Algorithm SHA256).Hash -ne $record.Value.program){throw "GPU program hash mismatch: $($record.Name)"}
}
Write-Output 'Verified four GPU sources and compiled program hashes.'
