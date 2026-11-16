#!/usr/bin/env python3
"""Compare NanoOS frames against the ANGLE GLES2 reference (tools/host_ref).
usage: compare.py REF.ppm|png NANO.png|.bin OUT.png
NANO may be a raw 240x432 ARGB8888 frame (.bin, as captured from the emulator queue buffers).
A landscape reference (432x240, HOST_LANDSCAPE=1) is compared against the portrait panel frame
rotated back to the logical view (NanoOS landscape mode 1, home button on the right).
Prints mean absolute difference (0-255) and the share of pixels differing by more than 24."""
import sys
from PIL import Image, ImageChops
def load(p):
    if p.endswith('.bin'):
        d=open(p,'rb').read()[:240*432*4]
        return Image.frombytes('RGBA',(240,432),d,'raw','BGRA').convert('RGB')
    return Image.open(p).convert('RGB')
a,b=load(sys.argv[1]),load(sys.argv[2])
if a.size[0]>a.size[1] and b.size[0]<b.size[1]:b=b.transpose(Image.ROTATE_90)
W,H=a.size
d=ImageChops.difference(a,b)
px=list(d.getdata());n=len(px)
mad=sum(sum(p) for p in px)/(3*n);big=sum(1 for p in px if max(p)>24)/n
sheet=Image.new('RGB',(W*3+20,H),(255,0,255))
sheet.paste(a,(0,0));sheet.paste(b,(W+10,0));sheet.paste(d.point(lambda v:min(255,v*3)),(2*W+20,0))
sheet.save(sys.argv[3])
print('mean_abs_diff %.1f  pixels_off %.1f%%  (reference | nanoos | diff x3) -> %s'%(mad,100*big,sys.argv[3]))
