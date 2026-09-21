"""CPU reference for the HLSL, not an engine screenshot or a build substitute.
Requires Pillow and numpy. Intentionally uses the exact shader tap weights,
bilinear sampling, sRGB decode/encode, and 8-bit quantization between passes.
"""
from pathlib import Path
import argparse
import json
import math
import re
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
FXC = ROOT / 'sp/src/materialsystem/stdshaders/bs2_tiltshift_ps20b.fxc'
WEIGHTS = [float(x) for x in re.findall(r'\* (0\.\d+);', FXC.read_text())]
assert len(WEIGHTS) == 33 and abs(WEIGHTS[0] + 2*sum(WEIGHTS[1:]) - 1) < 1e-6

def linear(x):
    return np.where(x <= .04045, x/12.92, ((x+.055)/1.055)**2.4)

def srgb(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= .0031308, x*12.92, 1.055*x**(1/2.4)-.055)

def render(image, center=.55, width=.24, falloff=.25, radius=24, strength=1, saturation=1.05, angle=0):
    rgb = np.asarray(image.convert('RGB'), dtype=np.float32)/255
    h,w = rgb.shape[:2]
    yy,xx = np.mgrid[:h,:w].astype(np.float32)
    distance = np.abs(yy/(h-1)-center-(xx/(w-1)-.5)*math.tan(math.radians(angle))*w/h)
    t = np.clip((distance-width*.5)/falloff,0,1)
    mask = t*t*(3-2*t)
    step = radius*strength*mask/32
    for axis in [0,1]:
        source = linear(rgb)
        result = source*WEIGHTS[0]
        for tap in range(1,33):
            for sign in [-1,1]:
                sx = np.clip(xx + (step*tap*sign if axis == 0 else 0),0,w-1)
                sy = np.clip(yy + (step*tap*sign if axis == 1 else 0),0,h-1)
                x0,y0 = sx.astype(np.int32),sy.astype(np.int32)
                x1,y1 = np.minimum(x0+1,w-1),np.minimum(y0+1,h-1)
                dx,dy = (sx-x0)[...,None],(sy-y0)[...,None]
                sample = (source[y0,x0]*(1-dx)+source[y0,x1]*dx)*(1-dy)+(source[y1,x0]*(1-dx)+source[y1,x1]*dx)*dy
                result += sample*WEIGHTS[tap]
        if axis == 1:
            lum = (result*np.array([.2126,.7152,.0722])).sum(axis=2)[...,None]
            result = lum+(result-lum)*saturation
        rgb = np.round(srgb(result)*255)/255
    return Image.fromarray(np.uint8(np.clip(rgb*255,0,255))), Image.fromarray(np.uint8(mask*255))

def checks():
    rng=np.random.default_rng(2026)
    original=Image.fromarray(rng.integers(0,256,(160,240,3),dtype=np.uint8))
    same,_=render(original,strength=0,saturation=1)
    assert np.abs(np.asarray(same,dtype=int)-np.asarray(original,dtype=int)).max() <= 1
    constant=Image.new('RGB',(240,160),(101,134,182))
    blurred,_=render(constant,saturation=1)
    assert np.abs(np.asarray(blurred,dtype=int)-np.asarray(constant,dtype=int)).max() <= 1
    blurred,mask=render(original,saturation=1)
    # Test the true zero-radius band, not mask values rounded down to black.
    sharp=np.broadcast_to((np.abs(np.arange(160)/(160-1)-.55) < .12)[:,None],(160,240))
    assert np.abs(np.asarray(blurred,dtype=int)[sharp]-np.asarray(original,dtype=int)[sharp]).max() <= 1
    assert np.isfinite(np.asarray(blurred)).all()
    print('PASS: normalized kernel, identity, flat-color preservation, sharp-band preservation, finite edges')

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('image',nargs='?')
    parser.add_argument('--out',default='preview')
    parser.add_argument('--center',type=float,default=.55)
    parser.add_argument('--radius',type=float,default=24)
    args=parser.parse_args()
    checks()
    if args.image:
        out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
        original=Image.open(args.image).convert('RGB')
        original.thumbnail((1920,1080),Image.Resampling.LANCZOS)
        effect,mask=render(original,center=args.center,radius=args.radius)
        original.save(out/'before.png');effect.save(out/'after.png');mask.save(out/'focus-mask.png')
        w,h=original.size
        board=Image.new('RGB',(w*2,h+48),(20,23,28));board.paste(original,(0,48));board.paste(effect,(w,48))
        d=ImageDraw.Draw(board)
        d.text((16,16),'ORIGINAL',fill='white')
        d.text((w+16,16),'TILT-SHIFT — CPU shader reference (not in-engine)',fill='white')
        board.save(out/'comparison.jpg',quality=94)
        (out/'settings.json').write_text(json.dumps(dict(center=args.center,width=.24,falloff=.25,radius=args.radius,strength=1,saturation=1.05,angle=0),indent=2))
        print(out.resolve())
