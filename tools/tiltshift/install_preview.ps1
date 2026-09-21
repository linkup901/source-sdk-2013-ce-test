param(
    [Parameter(Mandatory=$true)][string]$SdkRoot,
    [Parameter(Mandatory=$true)][string]$ModDir
)
$ErrorActionPreference = 'Stop'
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$source = (Resolve-Path -LiteralPath "$PSScriptRoot\..\..\sp\game\mod_episodic").Path
$ModDir = [IO.Path]::GetFullPath($ModDir)
if (Test-Path -LiteralPath $ModDir) { throw 'Choose a NEW empty test-mod directory; this script does not overwrite an existing mod.' }
if (!(Test-Path -LiteralPath "$SdkRoot\hl2.exe")) { throw 'SdkRoot must be Source SDK Base 2013 Singleplayer.' }
foreach ($file in @('bin/client.dll','bin/server.dll','materials/effects/bs2_tiltshift.vmt','shaders/fxc/bs2_tiltshift_ps20b.vcs')) {
    if (!(Test-Path -LiteralPath (Join-Path $source $file))) { throw "Build both client and shader first. Missing: $file" }
}
New-Item -ItemType Directory -Path $ModDir | Out-Null
foreach ($folder in @('bin','materials','shaders','cfg')) {
    Copy-Item -LiteralPath (Join-Path $source $folder) -Destination $ModDir -Recurse
}
$search = [Collections.Generic.List[string]]::new()
$search.Add('        Game+Mod+Mod_Write+Default_Write_Path "|gameinfo_path|."')
$search.Add('        GameBin "|gameinfo_path|bin"')
foreach ($folder in @('ep2','episodic','hl2')) {
    $dir = Join-Path $SdkRoot $folder
    if (!(Test-Path -LiteralPath $dir)) { throw "Missing mounted content directory: $dir" }
    $search.Add(('        Game "{0}/custom/*"' -f $dir.Replace('\','/')))
    Get-ChildItem -LiteralPath $dir -Filter '*_dir.vpk' | Sort-Object Name | ForEach-Object {
        $vpk = $_.FullName.Replace('_dir.vpk','.vpk').Replace('\','/')
        $search.Add(('        Game "{0}"' -f $vpk))
    }
    $search.Add(('        Game "{0}"' -f $dir.Replace('\','/')))
}
$platform = (Join-Path $SdkRoot 'platform').Replace('\','/')
$search.Add(('        Platform "{0}/platform_misc.vpk"' -f $platform))
$search.Add(('        Platform "{0}"' -f $platform))
@"
"GameInfo"
{
    game "BS2 Tilt-Shift Preview"
    title "BS2 Tilt-Shift Preview"
    type singleplayer_only
    FileSystem
    {
        SteamAppId 243730
        SearchPaths
        {
$($search -join "`r`n")
        }
    }
}
"@ | Set-Content -LiteralPath (Join-Path $ModDir 'gameinfo.txt') -Encoding ascii
'exec bs2_tiltshift_on.cfg' | Set-Content -LiteralPath (Join-Path $ModDir 'cfg\autoexec.cfg') -Encoding ascii
Write-Output "Preview installed: $ModDir"
Write-Output "Launch with Steam running: `"$SdkRoot\hl2.exe`" -game `"$ModDir`" -novid -console +map part3dot"
