$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$mb = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
Push-Location 'E:\SourceDev\bs2-ssao-polish-wt\sp\src'
& $mb game/client/client_sdk2013ce.vcxproj /m:4 /p:Configuration=Release /p:Platform=Win32 /p:WindowsTargetPlatformVersion=10.0 /verbosity:minimal
'EXIT ' + $LASTEXITCODE
Pop-Location

