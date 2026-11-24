from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np, math,random,hashlib,json,os,csv,zipfile,sys
from concurrent.futures import ProcessPoolExecutor
from layouts import FAMILIES,PASSES
from functools import lru_cache
P=Path(__file__).resolve().parent
OUT=P/'pack'
W,H=1280,720
N=Image.Resampling.NEAREST
def rgb(x):return tuple(bytes.fromhex(x))
def mix(a,b,t):return tuple(max(0,min(255,round(i+(j-i)*t))) for i,j in zip(a,b))
D=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype=np.float32)/16
@lru_cache(maxsize=1)
def grids():
 y,x=np.indices((H,W),dtype=np.int16)
 xx=x/W; yy=y/H
 r=np.maximum(np.abs(xx-.5)/.5,np.abs(yy-.5)/.5)
 return y,x,xx.astype(np.float32),yy.astype(np.float32),r.astype(np.float32)
def foundation(p,f,seed):
 name,dark,mid,hi,shine,red,ice,ex,sl,tag=PASSES[p]
 sty={k:rgb(v) for k,v in zip(('dark','mid','hi','shine','red','ice'),(dark,mid,hi,shine,red,ice))}
 y,x,xx,yy,r=grids()
 # Two peripheral light pools, offset by family and deliberate directional skew.
 a=.27 if f%3==0 else .7 if f%3==1 else .5
 diag=xx+(yy-.5)*(.42+sl)
 orbit=(np.cos(diag*8.6+f*1.13)**2)*.13
 broad=(np.exp(-(((xx-a)*1.33)**2+(yy-.30)**2)*7.3))*.37
 corners=np.clip((r-.24)*1.34,0,1)
 discrete=(corners*(broad+orbit+.12)+.04*np.sin(yy*6.9+tag*.019)+.18+.063*np.cos(xx*5.8+yy*3.2+f*.29))
 # Four-valued central luminance, alternating CRT line with tight fine dither.
 b=D[y%4,x%4]
 acc=np.floor(discrete*13.5+b*1.45).astype('int')
 pen=(y%3==0).astype('int')
 pal=[]
 for q in range(17):
  for j in range(2):
   cc=mix(sty['dark'],sty['mid'],q/15)
   if j:cc=mix(cc,(8,5,25),.11)
   pal.append(cc)
 arr=np.array(pal,dtype=np.uint8)[np.clip(2*acc+pen,0,len(pal)-1)]
 return Image.fromarray(arr,'RGB').convert('RGBA'),sty
def ribbon(im,f,p,s,seed):
 rng=random.Random(seed+37);d=ImageDraw.Draw(im)
 name,_,_,_,_,_,_,ex,sl,tag=PASSES[p]
 quiet=f>=25
 # Raster streaks: disjoint sets of broadcast scan bands at outer edges only.
 for sign in [-1,1]:
  loops=20 if quiet else 38
  for j in range(loops):
   edge=rng.choice([True,True,False]); scale=.59 if quiet else 1
   y=(rng.randrange(6,115) if edge else rng.randrange(152,280)) if sign<0 else (H-(rng.randrange(6,115) if edge else rng.randrange(152,280)))
   w=round(rng.randrange(8,170)*scale); x=(rng.randrange(-25,138) if (f+p+sign)%2 else rng.randrange(1105,1298))
   if x>640:x=x-w//2
   col=mix(s['mid'],s['hi'],rng.uniform(.12,.74)*(ex*.8))
   if j%13==0: col=mix(s['mid'],s['red'],.76*ex)
   d.rectangle((x,y,x+w,y+(1+j%3)),fill=col)
 # stepped geometric diagonal ribbons. Quiet styles get one narrow accent.
 for ii in range(2 if not quiet else 1):
  sign=1 if (f//2+ii+p)%2==0 else -1
  yy=round((0 if (ii+f)%2==0 else H)+rng.uniform(-25,25))
  x0=round((0 if sign==1 else W)+rng.uniform(185,305)*sign)
  length=round(rng.uniform(95,164)*(1 if not quiet else .68))
  z=-1 if yy>H/2 else 1
  yy2=yy+z*length
  n=round(rng.uniform(12,21)*ex)
  for b,(co,t) in enumerate([(s['hi'],.78),(s['red'],.96),(s['shine'],.46)]):
   x=x0+b*(n+9)*sign;fill=mix(s['dark'],co,t*ex)
   d.polygon(((x,yy),(x+sign*n,yy),(x+sign*(n-1-1.32*length),yy2),(x-sign*(1.32*length+2),yy2)),fill=fill)
 # Dithered optical bleed and differentiated scan cadence. Fade from vivid
 # edges into an untouched center; imply the old TV signal, not an overlay.
 for e in range(2):
  left=(e+f+p)%2==0
  edge_x=0 if left else W-1; mult=1 if left else -1
  center=rng.randrange(84,H-45)
  width=rng.randrange(160,310)
  col=mix(s['hi'],s['shine'],.37 if e%2 else .23)
  for mk in range(49 if quiet else 84):
   yy0=center+round(rng.gauss(0,34 if quiet else 42))
   start=max(0,round(rng.random()**2*width));end=min(width,start+rng.randint(4,88))
   edge_alpha=round((100 if quiet else 165)*(1-start/(width+1))*ex*rng.uniform(.2,.9))
   loc=[edge_x+mult*start,edge_x+mult*end]
   if 0<=yy0<H and end>start:d.line((min(loc),yy0,max(loc),yy0),fill=(*col,edge_alpha))

def art(i,write=True,raw=False):
 import motif_club, motif_stadium, motif_signal, motif_match
 bank={k:mod for mod,ks in [(motif_club,'st_george scarf ball'),(motif_stadium,'net stand terrace'),(motif_signal,'prism bars waveform'),(motif_match,'floodlights advertising')] for k in ks.split()}
 f,p=divmod(i,10)
 name,group,placements=FAMILIES[f]
 seed=0x5809+397*f+57*p
 b,s=foundation(p,f,seed)
 ink=Image.new('RGBA',(W,H))
 ribbon(ink,f,p,s,seed)
 intensity=PASSES[p][7]; skew=PASSES[p][8]
 r=random.Random(seed)
 for j,(k,x,y,w,h) in enumerate(placements):
  # The ten variants change framing and scene proportions, not just palette.
  # Preserve anchor edge when resizing. Sparse members reduce secondary motif footprint.
  q=[1.04,.9,.82,1.12,.88,1.02,.78,.98,.91,.82][p]*(1+r.uniform(-.037,.044))
  dx=r.randint(-19,19);dy=r.randint(-23,23)
  tx=round(x+dx+(w-w*q if x>630 else 0))
  ty=round(y+dy+(h-h*q if y>420 else 0))
  tw=max(35,round(w*q));th=max(30,round(h*q))
  if k=='removed':continue # Keep subsequent seeded framing identical.
  layer=Image.new('RGBA',(W,H))
  bank[k].draw(layer,k,tx,ty,tw,th,s,seed+259*j+31*p)
  weight=(1.14 if j==0 else .98 if j==1 else .91)*(.91 if f>=25 else 1)*intensity
  if k in ['waveform','bars','prism']:weight*=.90
  if p in (2,6,9) and j>1:weight*=.77
  layer.putalpha(layer.getchannel('A').point(lambda a:round(a*min(1.,weight))))
  ink.alpha_composite(layer)
 # UI-safe void: remove art overlap progressively. Not drawn as a visible panel.
 y,x,xx,yy,r1=grids()
 axis=np.maximum(np.abs(xx-.5)/.5,np.abs(yy-.50)/.5)
 m=np.clip((axis-.56)/.37,0,1);m=m*m*(3-2*m)
 aa=np.array(ink.getchannel('A'),dtype=np.float32)
 ink.putalpha(Image.fromarray(np.rint(aa*m).astype('uint8'),'L'))
 b.alpha_composite(ink)
 art=b.convert('RGB')
 if raw:return art
 # Broad palette preserves artwork, while the intentional CRT/dither comes from the native draw.
 art=art.quantize(colors=192,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE)
 id=f'BS{i+1:03}'
 n=f'{id}_{name}_{PASSES[p][0]}.png'
 thumb=None
 if write:
  path=OUT/'Backgrounds_4K'/group.replace(' ','_').replace('&','and')/n
  path.parent.mkdir(parents=True,exist_ok=True)
  a=art.resize((3840,2160),N)
  a.save(path,compress_level=6)
  thumb=P/'thumbs'/f'{id}.jpg'
  art.convert('RGB').resize((480,270),N).save(thumb,quality=91,subsampling=0)
  raw=np.asarray(art.convert('RGB'),dtype=float)
  core=raw[192:528,290:990]
  luma=core[:,:,0]*.2126+core[:,:,1]*.7152+core[:,:,2]*.0722
  stats=dict(center_luma_p95=round(float(np.quantile(luma,.95)),2),center_luma_mean=round(float(luma.mean()),2))
  assert stats['center_luma_p95']<62,(id,stats)
  return dict(id=id,file=path.relative_to(OUT).as_posix(),layout=name.replace('_',' '),palette=PASSES[p][0].replace('_',' '),collection=group,width=3840,height=2160,pixel_design_grid=[1280,720],sha256=hashlib.file_digest(path.open('rb'),'sha256').hexdigest(),bytes=path.stat().st_size,**stats)
 return art.convert('RGB')
def run():
 (P/'thumbs').mkdir(exist_ok=True)
 nproc=min(8,os.cpu_count() or 1)
 with ProcessPoolExecutor(nproc) as pool:
  rows=list(pool.map(art,range(300),chunksize=3))
 assert len(rows)==300 and len({r['sha256'] for r in rows})==300
 (OUT/'Background_Manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
 with (OUT/'Background_Manifest.csv').open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 print('300 verified,',sum(r['bytes'] for r in rows), 'bytes, center luma max p95',max(r['center_luma_p95'] for r in rows),flush=True)
if __name__=='__main__':run()
