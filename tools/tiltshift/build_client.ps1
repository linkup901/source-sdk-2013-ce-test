param([string]$MsBuild)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath "$PSScriptRoot\..\..\sp\src").Path
if (!$MsBuild) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 C++ Build Tools (v143, MFC, Windows SDK), or pass -MsBuild with its path.' }
    $MsBuild = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if (!$MsBuild -or !(Test-Path -LiteralPath $MsBuild)) { throw 'MSBuild with the C++ toolchain was not found.' }
Push-Location $source
try {
    & '.\devtools\bin\vpc.exe' /episodic /2013 +game /mksln bs2_tiltshift.sln
    if ($LASTEXITCODE -ne 0) { throw 'VPC project generation failed.' }
    # This legacy VPC can omit solution configuration mappings. Build each
    # project explicitly in dependency order rather than accepting a no-op.
    foreach ($project in @('mathlib/mathlib.vcxproj', 'tier1/tier1.vcxproj', 'vgui2/vgui_controls/vgui_controls.vcxproj', 'raytrace/raytrace.vcxproj', 'game/client/client_episodic.vcxproj', 'game/server/server_episodic.vcxproj')) {
        & $MsBuild $project /m:2 /p:Configuration=Release /p:Platform=Win32 /p:WindowsTargetPlatformVersion=10.0 /verbosity:minimal
        if ($LASTEXITCODE -ne 0) { throw "C++ build failed: $project. Do not deploy a partial build." }
    }
} finally { Pop-Location }
foreach ($file in @('client.dll','server.dll')) {
    if (!(Test-Path -LiteralPath "$source\..\game\mod_episodic\bin\$file")) { throw "Missing build output: $file" }
}
Write-Output 'Client and server built in sp/game/mod_episodic/bin.'

