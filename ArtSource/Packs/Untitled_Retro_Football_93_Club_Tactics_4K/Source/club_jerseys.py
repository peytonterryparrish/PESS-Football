from pathlib import Path
from io import BytesIO
from zipfile import ZipFile
import json, csv, math
from PIL import Image, ImageDraw, ImageFont
import numpy as np

P=Path(__file__).resolve().parent
N=Image.Resampling.NEAREST
KINDS=('home','away','keeper1','keeper2')
OUTLINE='#080b25'
def linear(v): 
 v=np.clip(v,0,255)
 return tuple(int(x) for x in v)
def source_sprite(z,slug,k):
 tag={'home':'F001','away':'F011','keeper1':'G001','keeper2':'G014'}[k]
 p=f'Clubs/{slug}/South_Native64/{k}/{tag}.png'
 if p not in z.namelist():p=next(n for n in z.namelist() if n.startswith(f'Clubs/{slug}/South_Native64/{k}/') and n.endswith('.png'))
 return Image.open(BytesIO(z.read(p))).convert('RGBA'),p
def make_shirt(im,k):
 pix=np.array(im)
 # Unbend the shirt, without taking any skin, hands, shorts or boots.
 keeper=k.startswith('keeper')
 w,h=72,64
 mask=Image.new('L',(w,h));d=ImageDraw.Draw(mask)
 shape=[(20,12),(28,8),(30,12),(35,14),(41,12),(43,8),(52,12)]
 if keeper:shape.extend([(57,17),(64,24),(69,46),(57,49),(50,31)])
 else:shape.extend([(57,14),(68,25),(60,36),(51,30)])
 shape.extend([(53,56),(50,59),(20,59),(18,56),(20,30)])
 if keeper:shape.extend([(14,49),(2,46),(7,24),(14,17)])
 else:shape.extend([(11,36),(3,25),(14,14)])
 d.polygon(shape,fill=255)
 out=np.zeros((h,w,4),dtype=np.uint8)
 ba=np.array(mask)>0
 # Torso and both sleeve regions are sampled independently from the original R08
 # garment. Approved colors, dither clusters and asymmetric panels survive.
 for y,x in np.argwhere(ba):
  t=np.clip((y-12)/45,0,1)
  sy=round(14+t*10)
  if x<21 and y<(49 if keeper else 36):
   zz=np.clip((y-13)/(30 if keeper else 20),0,1)
   sy=round(16+zz*(9 if keeper else 2))
   sx=round(23+np.clip((x-5)/16,0,1))
  elif x>50 and y<(49 if keeper else 36):
   zz=np.clip((y-13)/(30 if keeper else 20),0,1)
   sy=round(16+zz*(9 if keeper else 2))
   sx=round(38+np.clip((x-50)/17,0,1))
  else:
   sx=round(27+np.clip((x-20)/31,0,1)*9)
  c=pix[sy,sx]
  if not c[3]:c=pix[min(24,max(14,sy)),max(27,min(36,sx))]
  # One dithered shadow pixel at the hem, plus a tiny outside contour for dark kits.
  if y>=56:c=(*linear(np.array(c[:3],dtype=float)*.83),255)
  out[y,x]=(*c[:3],255)
 base=Image.fromarray(out,'RGBA')
 # Outer edge helps white and black kits alike read over field markings.
 stroked=Image.new('RGBA',(w,h));draw=ImageDraw.Draw(stroked)
 draw.line(shape+[shape[0]],fill=OUTLINE,width=2,joint='curve')
 stroked.alpha_composite(base)
 return stroked
def export_roster(source,dest):
 dest.mkdir(parents=True,exist_ok=True)
 with ZipFile(source) as z:
  clubs=json.loads(z.read('Club_Kit_Palettes.json'))
  assert len(clubs)==93 and len({x['slug'] for x in clubs})==93
  rows=[]
  for i,c in enumerate(clubs):
   slug=c['slug'];row={'club':c['name'],'slug':slug,'group':c['group'],'variants':[]}
   q=dest/'PNGs'/slug;q.mkdir(parents=True,exist_ok=True)
   for j,k in enumerate(KINDS):
    front,sp=source_sprite(z,slug,k)
    a=make_shirt(front,k);f=q/f'{k}.png'
    a.save(f)
    assert a.getchannel('A').getextrema()==(0,255)
    # Standalone sprite 72×64; atlas coordinate is the active 3x enlarged rect.
    p=(i*4+j); x=24+(p%16)*237;y=24+((p//16)%10)*211
    row['variants'].append(dict(role=k,path=f'PNGs/{slug}/{k}.png',r08_source=sp,base=c[k]['shirt'],trim=c[k]['trim'],pattern=c[k]['pattern'],atlas=f'Atlas_4K_{1+p//160:02}.png',rect=[x,y,216,192]))
   rows.append(row)
  for j in range(3):
   atlas=Image.new('RGBA',(3840,2160))
   for r in rows:
    for v in r['variants']:
     if v['atlas']!=f'Atlas_4K_{1+j:02}.png':continue
     x,y,w,h=v['rect']
     atlas.paste(Image.open(dest/v['path']).resize((w,h),N),(x,y))
   atlas.save(dest/f'Atlas_4K_{1+j:02}.png',compress_level=8)
  (dest/'Club_Jerseys.json').write_text(json.dumps({'width':72,'height':64,'pivot':[36,33],'clubs':rows},indent=2,ensure_ascii=False)+'\n')
  with (dest/'Club_Jerseys.csv').open('w',newline='') as f:
   wr=csv.writer(f);wr.writerow(['Club','Slug','Division','Variant','PNG','R08 source','Base','Trim','Pattern','Atlas','X','Y','Width','Height'])
   for r in rows:
    for v in r['variants']:wr.writerow([r['club'],r['slug'],r['group'],v['role'],v['path'],v['r08_source'],v['base'],v['trim'],v['pattern'],v['atlas'],*v['rect']])
  return rows
