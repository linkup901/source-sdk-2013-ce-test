param([string]$Map,[string]$Pos,[string]$Yaw,[string]$Tag)
$m="C:\Users\linkup\Desktop\MountainPassMap\bs2maps\BS2 clips\BS2-SSAO-Polish"
$L=@('sv_cheats 1','god','notarget','cl_drawhud 0','r_drawviewmodel 0','jpeg_quality 92','mat_vsync 0','fps_max 0','ent_create point_clientcommand targetname bs2pol')
function F($t,$c){ $script:L += "ent_fire bs2pol Command `"$c`" $t" }
F 5 'noclip'; F 5.1 'bs2_ssgi 0'; F 5.2 'bs2_ssao_debug 0'; F 5.5 "setpos $Pos"; F 5.6 "setang 5 $Yaw 0"
$t=10; F $t 'bs2_ssao 1'; foreach($h in 1,1,1,0,1){ F $t "bs2_ssao_half $h"; F ($t+5) "echo POLISHTIME half $h"; F ($t+5.2) 'bs2_ssao_bench 50'; $t+=7 }
function Shot($name,$pre){ foreach($c in $pre){ F $script:t $c; $script:t+=0.1 }; $script:t+=8; F $script:t "jpeg ${Tag}_$name"; $script:t+=1 }
Shot base_on @('bs2_ssao 1'); Shot base_off @('bs2_ssao 0'); Shot base_dbg @('bs2_ssao 1','bs2_ssao_debug 1'); F $t 'bs2_ssao_debug 0'; $t+=0.2
Shot vm_on @('give weapon_smg1','r_drawviewmodel 1','bs2_ssao 1'); Shot vm_off @('bs2_ssao 0'); F $t 'r_drawviewmodel 0'; $t+=0.2
Shot sky_on @("setang -25 $Yaw 0",'bs2_ssao 1'); Shot sky_dbg @('bs2_ssao_debug 1'); F $t 'bs2_ssao_debug 0'; $t+=0.2
Shot down_on @("setang 35 $Yaw 0",'bs2_ssao 1'); Shot down_off @('bs2_ssao 0'); Shot down_dbg @('bs2_ssao 1','bs2_ssao_debug 1'); F $t 'bs2_ssao_debug 0'; $t+=0.2
Shot fog_on @("setang 5 $Yaw 0",'fog_override 1','fog_enable 1','fog_start 64','fog_end 1500','fog_maxdensity 1','bs2_ssao 1'); Shot fog_off @('bs2_ssao 0'); Shot fog_dbg @('bs2_ssao 1','bs2_ssao_debug 1'); F $t 'bs2_ssao_debug 0'; F ($t+0.1) 'fog_override 0'; $t+=0.3
Shot smoke_on @("setang 20 $Yaw 0",'ent_create env_smokestack targetname pss InitialState 1 BaseSpread 30 SpreadSpeed 15 Speed 40 StartSize 40 EndSize 140 Rate 40 JetLength 220 SmokeMaterial particle/SmokeStack.vmt renderamt 220','ent_fire pss TurnOn','setang 5 '+$Yaw+' 0','bs2_ssao 1'); Shot smoke_off @('bs2_ssao 0')
F $t 'bs2_ssao 1'; F ($t+0.5) 'echo POLISH DONE'; F ($t+1) 'quit'
$L | Set-Content "$m\cfg\bs2polish_auto.cfg" -Encoding ascii
"cfg written, end t=$t"

