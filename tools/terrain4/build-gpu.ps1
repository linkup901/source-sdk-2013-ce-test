param([string]$SdkPath='C:/SteamLibrary/steamapps/common/Source SDK Base 2013 Singleplayer')
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$stage=Join-Path $repoRoot 'terrain4-gpu-build'
$game=Join-Path $repoRoot 'sp\game\mod_episodic'
$sdkBin=Join-Path $SdkPath 'bin'
if(!(Test-Path (Join-Path $sdkBin 'shadercompile.exe'))){throw 'SDK shader compiler not found.'}
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'shaders\fxc') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $game 'shaders\fxc') | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'sp\src\dx9sdk\utilities\dx_proxy.dll') -Destination $stage
$commands=''
$programs=@()
foreach($shader in @('bs2_terrain4','bs2_river')){
 foreach($kind in @('vs','ps')){
  $name="${shader}_${kind}30"; $profile="${kind}_3_0"
  $programs+=$name
  Copy-Item -LiteralPath (Join-Path $repoRoot ('sp\src\materialsystem\stdshaders\'+$name+'.fxc')) -Destination $stage
  $oldProgram=Join-Path $stage ('shaders\fxc\'+$name+'.vcs')
  if(Test-Path -LiteralPath $oldProgram){Remove-Item -LiteralPath $oldProgram}
  $commands+=@"
#BEGIN $name
$name.fxc
#DEFINES-D:
#DEFINES-S:
#SKIP:
0
#COMMAND:
fxc.exe /DTOTALSHADERCOMBOS=1 /DCENTROIDMASK=0 /DNUMDYNAMICCOMBOS=1 /DFLAGS=0x0
/T$profile /DSHADER_MODEL_$($profile.ToUpper())=1 /nologo /Foshader.o $name.fxc>output.txt 2>&1
#END

"@
 }
}
Set-Content -LiteralPath (Join-Path $stage 'filelist.txt') -Value $commands -Encoding ascii
$copyList=@($programs | ForEach-Object {$_+'.fxc'})
$copyList+=@('dx_proxy.dll',"$sdkBin/shadercompile.exe","$sdkBin/shadercompile_dll.dll","$sdkBin/tier0.dll","$sdkBin/vstdlib.dll")
Set-Content -LiteralPath (Join-Path $stage 'uniquefilestocopy.txt') -Value $copyList -Encoding ascii
Push-Location $stage
try { & (Join-Path $sdkBin 'shadercompile.exe') -nompi -nop4 -game $game -shaderpath $stage -allowdebug; if($LASTEXITCODE){throw 'GPU compile failed.'} } finally {Pop-Location}
foreach($name in $programs){
 $program=Join-Path $stage ('shaders\fxc\'+$name+'.vcs')
 if(!(Test-Path -LiteralPath $program)){throw "Missing compiled program: $name"}
 Copy-Item -LiteralPath $program -Destination (Join-Path $game ('shaders\fxc\'+$name+'.vcs'))
}
