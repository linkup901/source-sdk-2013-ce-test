param([Parameter(Mandatory)][string]$ModPath, [string]$PackagePath=$PSScriptRoot)
$ErrorActionPreference='Stop'
$modRoot=(Resolve-Path -LiteralPath $ModPath).Path
if(!(Test-Path -LiteralPath (Join-Path $modRoot 'gameinfo.txt'))){throw 'ModPath must contain gameinfo.txt.'}
if(Get-Process hl2 -ErrorAction SilentlyContinue){throw 'Close the Source game before installing its shader DLL.'}
$payloadRoot=Join-Path $PackagePath 'runtime'
if(!(Test-Path -LiteralPath (Join-Path $payloadRoot 'bin\game_shader_dx9.dll'))){throw 'Missing combined shader DLL in runtime/bin.'}
$backupRoot=Join-Path $modRoot ('installation-backups\terrain4-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$installed=@()
foreach($file in Get-ChildItem -LiteralPath $payloadRoot -File -Recurse){
 $relative=$file.FullName.Substring($payloadRoot.Length+1)
 $destination=Join-Path $modRoot $relative
 if(Test-Path -LiteralPath $destination){
  $backup=Join-Path $backupRoot $relative
  New-Item -ItemType Directory -Force -Path (Split-Path $backup) | Out-Null
  Copy-Item -LiteralPath $destination -Destination $backup
 }
 New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
 Copy-Item -LiteralPath $file.FullName -Destination $destination
 $sourceHash=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
 if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $sourceHash){throw "Verification failed: $relative"}
 $installed+=@{path=$relative;sha256=$sourceHash}
}
@{installed=(Get-Date).ToString('o');backup=$backupRoot;files=$installed} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $modRoot 'TERRAIN4-INSTALL.json')
Write-Output "Installed terrain + river shader package into $modRoot. Backup: $backupRoot"
