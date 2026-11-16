param([string]$Batch='base',[string]$Only='')
$tools='C:\Users\linkup\Desktop\MountainPassMap\bs2maps\BS2 clips\tools'; $mod='C:\Users\linkup\Desktop\MountainPassMap\bs2maps\BS2 clips\BS2-SSAO-Polish'; $sdk='C:\SteamLibrary\steamapps\common\Source SDK Base 2013 Singleplayer'
$lock="$tools\engine-lock\engine_lock.ps1"; $who='claude-seppma-A'; $st="$mod\status.txt"
$runs=@(@('bs2_garage_menu_261026','5420 10320 196','50','gar'),@('bs2_lundqvist_house_texpass_v1','750 120 42','0','lq'),@('bs2_autumn_m1_rural_outskirts','-6335 -99 80','24','am1'),@('bs2_lq_river_custom_65','2360 -1580 -15','180','riv'))
while ($true) { $r = & powershell -NoProfile -ExecutionPolicy Bypass -File $lock acquire $who "SEPPMA A SSAO polish: 4 minimized map runs (cost + edge-case shots), about 16 min"; if ($r -contains 'ACQUIRED') { break }; Start-Sleep 10 }
try {
 foreach($x in $runs){ if ($Only -and ($Only -notmatch $x[3])) { continue }
  $tg = $Batch + '_' + $x[3]; & powershell -NoProfile -ExecutionPolicy Bypass -File "$mod\make_cfg.ps1" -Map $x[0] -Pos $x[1] -Yaw $x[2] -Tag $tg | Out-Null
  Remove-Item "$mod\console.log" -ea 0
  $a="-game `"$mod`" -windowed -w 1920 -h 1080 -novid -console -condebug +engine_no_focus_sleep 0 +fps_max 0 +mat_vsync 0 +map $($x[0])"
  $p = Start-Process -FilePath "$sdk\hl2.exe" -ArgumentList $a -WorkingDirectory $sdk -WindowStyle Minimized -PassThru
  $end=(Get-Date).AddSeconds(330); while(-not $p.HasExited -and (Get-Date) -lt $end){ Start-Sleep 1 }
  "$Batch $($x[0]) exited=$($p.HasExited) at $(Get-Date -f HH:mm:ss)" | Add-Content $st
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; Start-Sleep 3 }
  Copy-Item "$mod\console.log" ("$mod\console_" + $tg + ".log") -ea 0
 }
} finally { & powershell -NoProfile -ExecutionPolicy Bypass -File $lock release $who | Out-Null; "$Batch ALL DONE $(Get-Date -f HH:mm:ss)" | Add-Content $st }



