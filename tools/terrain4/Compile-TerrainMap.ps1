param(
 [Parameter(Mandatory=$true)][string]$ModPath,
 [Parameter(Mandatory=$true)][string]$VmfPath,
 [string]$SdkPath='C:/SteamLibrary/steamapps/common/Source SDK Base 2013 Singleplayer',
 [string]$OutputPath
)
$ErrorActionPreference='Stop'
$vmf=(Resolve-Path -LiteralPath $VmfPath).Path
if(!$OutputPath){$OutputPath=Join-Path (Split-Path $vmf) ([IO.Path]::GetFileNameWithoutExtension($vmf)+'-compile')}
$output=[IO.Path]::GetFullPath($OutputPath)
$overlay=Join-Path $output 'compile-game'
& (Join-Path $PSScriptRoot 'Prepare-TerrainCompile.ps1') -ModPath $ModPath -OutputPath $overlay
$staged=Join-Path $output ([IO.Path]::GetFileName($vmf))
Copy-Item -LiteralPath $vmf -Destination $staged
$bsp=[IO.Path]::ChangeExtension($staged,'.bsp')
foreach($tool in @('vbsp','vvis','vrad')){
 $toolArgs=@('-game',$overlay)
 if($tool -eq 'vrad'){$toolArgs=@('-both')+$toolArgs}
 $toolArgs+= $(if($tool -eq 'vbsp'){$staged}else{$bsp})
 & (Join-Path $SdkPath ('bin/'+$tool+'.exe')) @toolArgs 2>&1 | Tee-Object -FilePath (Join-Path $output ($tool+'.log'))
 if($LASTEXITCODE){throw "$tool failed ($LASTEXITCODE). BSP not installed."}
}
$data=[IO.File]::ReadAllBytes($bsp)
foreach($lump in @(8,53)){
 if([BitConverter]::ToInt32($data,8+$lump*16+4) -le 0){throw 'Baked lighting missing. BSP not installed.'}
}
$maps=Join-Path $ModPath 'maps'
New-Item -ItemType Directory -Force -Path $maps | Out-Null
$target=Join-Path $maps ([IO.Path]::GetFileName($bsp))
if(Test-Path -LiteralPath $target){Copy-Item -LiteralPath $target -Destination ($target+'.before-terrain-'+(Get-Date -Format yyyyMMdd-HHmmss))}
Copy-Item -LiteralPath $bsp -Destination $target
Write-Output "Installed lightmapped terrain example: $target"
