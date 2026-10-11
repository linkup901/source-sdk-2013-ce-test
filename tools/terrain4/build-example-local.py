from pathlib import Path
import sys, shutil, re, struct, json, hashlib, io
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parent; CLIPS=ROOT.parent; GAME=ROOT/'game'; MAT=GAME/'materials/bs2/terrain4'
sys.path.insert(0,str(CLIPS/'river-lab-20261010'))
from mesh_utils import box_solid,ent
source=CLIPS/'BS2-TiltShift-Preview/materials'
layers=['bs2/terrain_mines_261011/downhill_gravel','bs2/terrain_mines_261011/forest_loam','bs2/terrain_mines_261011/trail_mud','bs2/minekit/mountain_rock']
manifest={}
for l in layers:
 for suffix in ['', '_normal']:
  p=source/(l+suffix+'.vtf'); dst=GAME/'materials'/(l+suffix+'.vtf'); dst.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(p,dst)
  manifest[str(p)]={'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'destination':str(dst)}
def vtf(path,im,flags=0x0C):
 mips=[]
 while True:
  mips.append(im.tobytes())
  if im.width==1 and im.height==1:break
  im=im.resize((max(1,im.width//2),max(1,im.height//2)),Image.Resampling.BOX)
 h=bytearray(80); struct.pack_into('<4sIIIHHIHH',h,0,b'VTF\0',7,2,80,512,512,flags,1,0)
 struct.pack_into('<3f',h,32,.3,.3,.3);struct.pack_into('<fIBI',h,48,1,0,len(mips),0xffffffff);struct.pack_into('<BBH',h,61,0,0,1)
 path.write_bytes(h+b''.join(reversed(mips)))
N=512; yy,xx=np.mgrid[:N,:N]; x=(xx+.5)/N*1024-512;y=(yy+.5)/N*1024-512
center=65*np.sin(y/220); dist=np.abs(x-center)
def smooth(a,b,x):
 t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)
road=1-smooth(160,230,dist)
mud=np.exp(-((x-center-70)/25)**2)*road*(.45+.35*np.sin(y/170)**2)
rock=smooth(270,450,dist)*(.35+.65*(.5+.5*np.sin(y/120+x/210)))
loam=(1-road)*(1-rock)
gravel=road*(1-mud)
w=np.stack([gravel,loam,mud,rock],-1);w/=np.maximum(w.sum(-1,keepdims=True),1e-6)
im=Image.fromarray(np.round(w*255).astype('uint8'),'RGBA');im.save(ROOT/'trail_weights.png');vtf(MAT/'trail_weights.vtf',im)
def readvtf(p):
 data=p.read_bytes();width,height=struct.unpack_from('<HH',data,16);fmt=struct.unpack_from('<I',data,52)[0]
 if fmt==0:return Image.frombytes('RGBA',(width,height),data[-width*height*4:])
 if fmt in (13,14,15):
  size=((width+3)//4)*((height+3)//4)*(8 if fmt==13 else 16)
  h=bytearray(128);h[:4]=b'DDS ';struct.pack_into('<7I',h,4,124,0x81007,height,width,size,0,1)
  struct.pack_into('<II4s',h,76,32,4,{13:b'DXT1',14:b'DXT3',15:b'DXT5'}[fmt]);struct.pack_into('<I',h,108,0x1000)
  return Image.open(io.BytesIO(h+data[-size:])).convert('RGBA')
 raise ValueError(f'{p}: unsupported format {fmt}')
# An accurate static color preview for Hammer; runtime retains independent tiling and normals.
colors=[]
for l in layers:
 a=np.array(readvtf(source/(l+'.vtf')).convert('RGB'))/255
 idx=(np.arange(N)*4).astype(int)%a.shape[0]
 colors.append(a[(-idx[:,None])%a.shape[0],idx[None,:],:]**2.2)
preview=sum(a*w[:,:,i,None] for i,a in enumerate(colors))**(1/2.2)
im=Image.fromarray(np.dstack([np.clip(preview*255,0,255).astype('uint8'),np.full((N,N),255,'uint8')]),'RGBA');im.save(ROOT/'hammer_preview.png');vtf(MAT/'trail_preview.vtf',im,0)
vmt='"BS2_Terrain4"\n{\n'
for i,l in enumerate(layers):
 vmt+=f' "$basetexture{str(i+1) if i else ""}" "{l}"\n "$bumpmap{str(i+1) if i else ""}" "{l}_normal"\n'
vmt+=' "$blendmask" "bs2/terrain4/trail_weights"\n "$maskbounds" "[-512 -512 1024 1024]"\n "$layerscales" "[1 1 1 1]"\n "$blendcontrast" "1"\n "$surfaceprop" "gravel"\n "%tooltexture" "bs2/terrain4/trail_preview"\n "%keywords" "bs2,terrain,fourway,gravel,loam,mud,rock"\n}\n'
(MAT/'trail_fourway.vmt').write_text(vmt)
(MAT/'trail_hammer_preview.vmt').write_text('"LightmappedGeneric"\n{\n "$basetexture" "bs2/terrain4/trail_preview"\n "$surfaceprop" "gravel"\n}\n')
gi=(CLIPS/'river-lab-20261010/game/gameinfo.txt').read_text().replace('BS2 River Lab','BS2 Four-Way Terrain Review');(GAME/'gameinfo.txt').write_text(gi)
for f in ['client.dll','server.dll']:shutil.copy2(CLIPS/'soft-fog-review-20261011/combined-dlls'/f,GAME/'bin'/f)
nid=[10]
def box(c,h,m):return box_solid(nid,c,h).replace('TOOLS/TOOLSTRIGGER',m)
# Closed lab with a real power-3 collision displacement, curved gravel/loam road, mud rut and rock shoulders.
ground=box((0,0,-32),(512,512,32),'tools/toolsnodraw')
start=ground.index('\t\tside');end=ground.index('\t\t}',start)
side=ground[start:end].replace('tools/toolsnodraw','bs2/terrain4/trail_fourway').replace('] 0.25','] 0.5')
disp='\n dispinfo\n {\n "power" "3"\n "startposition" "[-512 -512 0]"\n "elevation" "0"\n "subdiv" "0"\n'
for section,values in [('normals',' '.join(['0 0 1']*9)),('distances',' '.join(['0']*9)),('offsets',' '.join(['0 0 0']*9)),('offset_normals',' '.join(['0 0 1']*9)),('alphas',' '.join(['0']*9))]:
 disp+=f' {section}\n {{\n'+''.join(f' "row{i}" "{values}"\n' for i in range(9))+' }\n'
disp+=' triangle_tags\n {\n'+''.join(f' "row{i}" "'+ ' '.join(['9']*16)+'"\n' for i in range(8))+' }\n allowed_verts\n {\n "10" "-1 -1 -1 -1 -1 -1 -1 -1 -1 -1"\n }\n }\n'
ground=ground[:start]+side+disp+ground[end:]
world='versioninfo\n{ "editorversion" "400" "editorbuild" "10000" "mapversion" "1" "formatversion" "100" }\nworld\n{\n "id" "1"\n "classname" "worldspawn"\n "skyname" "sky_day01_01"\n'+ground
for c,h in [((0,0,520),(544,544,16)),((-528,0,256),(16,544,256)),((528,0,256),(16,544,256)),((0,-528,256),(512,16,256)),((0,528,256),(512,16,256))]:world+=box(c,h,'tools/toolsskybox')
world+=box((0,0,-16),(544,544,16),'tools/toolsnodraw')
world+='}\n'+ent(nid,{'classname':'info_player_start','origin':'0 -400 12','angles':'0 90 0'})+ent(nid,{'classname':'light_environment','origin':'0 0 400','angles':'0 0 0','pitch':'-55','_light':'210 220 230 180','_ambient':'110 130 150 80'})
world+=ent(nid,{'classname':'light','origin':'-300 100 140','_light':'255 178 88 180','_distance':'550'})
world+=ent(nid,{'classname':'point_servercommand','targetname':'terrain_capture'})
world+=ent(nid,{'classname':'logic_auto'},outputs=[('OnMapSpawn','terrain_capture,Command,exec terrain_start.cfg,3,-1'),('OnMapSpawn','terrain_capture,Command,jpeg terrain4_overview,7,-1'),('OnMapSpawn','terrain_capture,Command,quit,10,-1')])
(ROOT/'bs2_terrain4_lab_261011.vmf').write_text(world)
previewmap=world.replace('bs2/terrain4/trail_fourway','bs2/terrain4/trail_hammer_preview').replace('"[1 0 0 0] 0.5"','"[1 0 0 256] 2"').replace('"[0 -1 0 0] 0.5"','"[0 1 0 256] 2"')
(ROOT/'bs2_terrain4_hammer_preview_261011.vmf').write_text(previewmap)
playmap=world.replace('"OnMapSpawn" "terrain_capture,Command,jpeg terrain4_overview,7,-1"','').replace('"OnMapSpawn" "terrain_capture,Command,quit,10,-1"','')
(ROOT/'bs2_terrain4_play_261011.vmf').write_text(playmap)
(ROOT/'source-texture-manifest.json').write_text(json.dumps(manifest,indent=2))
(GAME/'cfg/autoexec.cfg').write_text('sv_cheats 1\nbs2_tiltshift_enable 0\nmat_hdr_level 0\nfps_max 60\n')
(GAME/'cfg/capture.cfg').write_text('sv_cheats 1\ncl_mouseenable 0\nhidehud 4\nr_drawviewmodel 0\nnoclip\nsetpos 0 -450 320\nsetang 37 90 0\nwait 300\njpeg terrain4_overview\nwait 60\nsetpos -50 -190 95\nsetang 25 70 0\nwait 180\njpeg terrain4_close\nwait 60\nquit\n')
(GAME/'cfg/terrain_start.cfg').write_text('sv_cheats 1\nengine_no_focus_sleep 0\ncl_mouseenable 0\nhidehud 4\nr_drawviewmodel 0\nnoclip\nsetpos 0 -450 400\nsetang 43 90 0\nhideconsole\n')
print('Created isolated terrain lab, linear RGBA weights and Hammer preview from existing BS2 textures.')
