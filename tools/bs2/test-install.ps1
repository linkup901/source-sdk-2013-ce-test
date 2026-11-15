$ErrorActionPreference='Stop'
function Assert-Setup($condition,[string]$message){if(!$condition){throw $message}}
$tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$testRoot=Join-Path $tempRoot ('bs2-install-test-'+[guid]::NewGuid().ToString('N'))
$package=Join-Path $testRoot 'package';$mod=Join-Path $testRoot 'mod'
$runtime=Join-Path $package 'runtime'
New-Item -ItemType Directory -Force -Path $runtime,$mod | Out-Null
try {
 $payload=@{
  'bin/client.dll'='new-client';'bin/server.dll'='new-server';'bin/game_shader_dx9.dll'='new-shaders'
  'credits.txt'='packaged credits';'bs2_phase2.fgd'='combined fgd';'bs_soft_fog.fgd'='fog fgd'
  'particles/particles_manifest.txt'="particles_manifest`n{`n `"file`" `"particles/bs2_breath.pcf`"`n}"
  'cfg/skill.cfg'='packaged skills'
 }
 foreach($path in $payload.Keys){
  $file=Join-Path $runtime $path
  New-Item -ItemType Directory -Force -Path (Split-Path $file) | Out-Null
  Set-Content -LiteralPath $file -Value $payload[$path]
 }
 $records=@(Get-ChildItem -LiteralPath $runtime -Recurse -File | ForEach-Object {@{path=$_.FullName.Substring($runtime.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName).Hash}})
 $manifestPath=Join-Path $package 'PACKAGE-MANIFEST.json'
 $originalManifest=@{commit='fixture';files=$records} | ConvertTo-Json -Depth 5
 Set-Content -LiteralPath $manifestPath -Value $originalManifest
 Set-Content -LiteralPath (Join-Path $mod 'gameinfo.txt') -Value 'existing gameinfo'
 New-Item -ItemType Directory -Force -Path (Join-Path $mod 'bin'),(Join-Path $mod 'cfg'),(Join-Path $mod 'particles') | Out-Null
 Set-Content -LiteralPath (Join-Path $mod 'bin/client.dll') -Value 'old-client'
 Set-Content -LiteralPath (Join-Path $mod 'credits.txt') -Value 'campaign credits'
 Set-Content -LiteralPath (Join-Path $mod 'cfg/skill.cfg') -Value 'campaign skills'
 Set-Content -LiteralPath (Join-Path $mod 'particles/particles_manifest.txt') -Value "particles_manifest`n{`n `"file`" `"particles/custom.pcf`"`n}"
 $install=Join-Path $PSScriptRoot 'Install-BS2.ps1'
 & $install -ModPath $mod -PackagePath $package
 $record=Get-Content -LiteralPath (Join-Path $mod 'BS2-INSTALL.json') -Raw | ConvertFrom-Json
 Assert-Setup ((Get-Content -LiteralPath (Join-Path $record.backup 'bin/client.dll')).Trim() -eq 'old-client') 'Existing DLL was not backed up.'
 Assert-Setup ((Get-Content -LiteralPath (Join-Path $mod 'bin/client.dll')).Trim() -eq 'new-client') 'New DLL was not installed.'
 Assert-Setup ((Get-Content -LiteralPath (Join-Path $mod 'credits.txt')).Trim() -eq 'campaign credits') 'Campaign credits were overwritten.'
 Assert-Setup ((Get-Content -LiteralPath (Join-Path $mod 'cfg/skill.cfg')).Trim() -eq 'campaign skills') 'Campaign skills were overwritten.'
 & $install -ModPath $mod -PackagePath $package
 $particles=Get-Content -LiteralPath (Join-Path $mod 'particles/particles_manifest.txt') -Raw
 Assert-Setup ($particles.Contains('particles/custom.pcf')) 'Existing particle registrations were lost.'
 Assert-Setup ([regex]::Matches($particles,'particles/bs2_breath.pcf').Count -eq 1) 'Repeat install duplicated breath registration.'
 $before=(Get-FileHash -LiteralPath (Join-Path $mod 'bin/client.dll')).Hash
 Set-Content -LiteralPath (Join-Path $runtime 'bin/client.dll') -Value 'corrupt'
 $rejected=$false
 try {& $install -ModPath $mod -PackagePath $package} catch {$rejected=$_.Exception.Message -like '*hash mismatch*'}
 Assert-Setup $rejected 'Corrupted package was accepted.'
 Assert-Setup ((Get-FileHash -LiteralPath (Join-Path $mod 'bin/client.dll')).Hash -eq $before) 'Hash rejection modified the installed DLL.'
 Set-Content -LiteralPath (Join-Path $runtime 'bin/client.dll') -Value 'new-client'
 @{commit='fixture';files=@(@{path='../escape.txt';sha256='bad'})} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath
 $rejected=$false
 try {& $install -ModPath $mod -PackagePath $package} catch {$rejected=$_.Exception.Message -like '*Invalid package path*'}
 Assert-Setup $rejected 'Path traversal was accepted.'
 Assert-Setup ((Get-Content -LiteralPath (Join-Path $mod 'gameinfo.txt')).Trim() -eq 'existing gameinfo') 'Installer changed gameinfo.'
 # A fresh mod receives the packaged credits and particle manifest.
 Set-Content -LiteralPath $manifestPath -Value $originalManifest
 $fresh=Join-Path $testRoot 'fresh';New-Item -ItemType Directory -Path $fresh | Out-Null
 Set-Content -LiteralPath (Join-Path $fresh 'gameinfo.txt') -Value 'fresh CE mod'
 & $install -ModPath $fresh -PackagePath $package
 Assert-Setup ((Get-Content -LiteralPath (Join-Path $fresh 'credits.txt')).Trim() -eq 'packaged credits') 'Fresh install is missing credits.'
 Write-Output 'PASS: install, backups, preservation, idempotent particle merge, corruption and traversal rejection, fresh install.'
} finally {
 $resolved=[IO.Path]::GetFullPath($testRoot)
 if(!$resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path $resolved -Leaf) -notlike 'bs2-install-test-*'){throw 'Refusing cleanup outside the test directory.'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
