param(
 [Parameter(Mandatory=$true)][string]$ModPath,
 [Parameter(Mandatory=$true)][string]$OutputPath
)
$ErrorActionPreference='Stop'
$mod=(Resolve-Path -LiteralPath $ModPath).Path
$output=[IO.Path]::GetFullPath($OutputPath)
if($output.TrimEnd('\') -eq $mod.TrimEnd('\')){throw 'Compile overlay must be separate from the runtime mod'}
$gameinfo=Join-Path $mod 'gameinfo.txt'
if(!(Test-Path -LiteralPath $gameinfo)){throw 'Mod gameinfo.txt missing'}
New-Item -ItemType Directory -Force -Path $output | Out-Null
# A separate search path preserves material names in the BSP without changing
# runtime VMTs. Valve's empty shader API cannot load the custom DX9 shader.
$source=Get-Content -LiteralPath $gameinfo -Raw
$source=$source -replace '(?im)^\s*GameBin\s+[^\r\n]+',''
$source=$source.Replace('|gameinfo_path|',($mod -replace '\\','/')+'/')
$source=$source -replace '(?i)(SearchPaths\s*\{)',('$1'+[Environment]::NewLine+' Game "|gameinfo_path|."')
Set-Content -LiteralPath (Join-Path $output 'gameinfo.txt') -Value $source
$count=0
Get-ChildItem -LiteralPath (Join-Path $mod 'materials') -Filter '*.vmt' -Recurse -File | ForEach-Object {
 $material=Get-Content -LiteralPath $_.FullName -Raw
 if($material -match '^\s*"?BS2_Terrain4"?\s*\{'){
  $relative=$_.FullName.Substring($mod.Length+1)
  $target=Join-Path $output $relative
  New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
  $material=$material -replace '^\s*"?BS2_Terrain4"?', '"LightmappedGeneric"'
  Set-Content -LiteralPath $target -Value $material
  $count++
 }
}
if(!$count){throw 'No BS2_Terrain4 materials found'}
Write-Output "Prepared $count terrain compile materials at $output. Runtime materials unchanged."
