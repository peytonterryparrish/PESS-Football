"""Generate broadcast-crowd art with the approved R08 head sprites and fresh 64px pixel garments/poses."""
from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
import numpy as np
import csv, json, hashlib
P=Path(__file__).resolve().parent.parent
N=Image.Resampling.NEAREST
DITHER=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])/16
WARDROBE=[
 ('Claret scarf','a34850','dda4a0','36455d','e2d5bc','scarf','coat','plain'),
 ('Blue cap','4879a4','bdcde2','514c45','eed69b','cap','polo','plain'),
 ('Green dad jacket','56775c','a1ac83','354859','e7dad3','swept','zip','plain'),
 ('Maroon cardigan','69425c','be819a','46374d','d6b5ad','waves','buttons','skirt'),
 ('White home stripes','b8b7a4','ece2c2','452e38','be5153','curls','stripes','plain'),
 ('Red and amber knit','ad533f','e6a87c','373a51','d9a63c','beanie','hoop','plain'),
 ('Royal windbreaker','40579a','99b5ec','283149','dfd5c0','bald','zip','plain'),
 ('Yellow terrace top','b89c46','eee189','4f5369','41475a','bun','polo','plain'),
 ('Teal rain shell','447c78','99c3ab','373942','d4ddd1','hood','zip','plain'),
 ('Rust club jacket','98583f','d8a286','403b45','d6cba9','afro','buttons','plain'),
 ('Navy coat and scarf','414e76','8b9eb9','484758','dca75f','scarf','coat','plain'),
 ('Scarlet raincoat','a93f4b','e98687','5e5755','e7d8c3','hood','zip','plain'),
 ('Pine flat cap','597356','9eae87','545042','dac69b','cap','buttons','plain'),
 ('Light-blue jersey','578fb3','b4dcef','3f4d6d','dedbc8','swept','hoop','plain'),
 ('Red and white knit','96424e','e4a1a5','3e4655','e4dacf','beanie','hoop','plain'),
 ('Black and white terracer','73777b','e3dfd1','3f3f44','d5d8cc','bald','mono','plain'),
 ('Gold jacket supporter','8d742e','dcc471','52526f','e6d9c6','curls','zip','plain'),
 ('Violet vintage jumper','705378','c598b5','514052','c2d4ab','waves','polo','skirt'),
 ('Forest terrace shirt','477355','94ae7a','465a5a','dbd8c1','afro','stripes','plain'),
 ('Cream away sweater','a39273','e1ceb3','554657','8eb1a2','bun','stripe','plain'),
]
src=sorted((P/'source/Base').glob('*.png'))
def rgb(h):return np.array(tuple(bytes.fromhex(h)),float)
def shape(im, pts, dark, bright, n, *, typ='poly', folds=.09, shift=0, soft=.19):
 w,h=im.size;m=Image.new('L',(w,h));d=ImageDraw.Draw(m)
 if typ=='poly':d.polygon(pts,255)
 else:d.ellipse(pts,255)
 ar=np.asarray(m);xy=np.argwhere(ar>0)
 if len(xy)==0:return
 cx=(xy[:,1].min()+xy[:,1].max())*.5+shift
 rad=max(2,(xy[:,1].max()-xy[:,1].min())*.56)
 lo=rgb(dark);hi=rgb(bright);p=im.load()
 for y,x in xy:
  t=.64-.33*min(1,abs(x-cx)/rad)+folds*np.sin((x*.65+y*.37+n))
  if x==0 or y==0 or x==w-1 or y==h-1 or any(ar[y+dy,x+dx]==0 for dx,dy in ((1,0),(-1,0),(0,1),(0,-1))):
   t-=soft
  t+=.14*(DITHER[(y+n)%4,x%4]-.5)+.105*np.sin(y*.31+x*1.12+n*.36)+.05*np.cos(x*.95-y*.42+n*.88)
  t=min(1,max(0,round(t*12)/12))
  p[int(x),int(y)]=(*tuple(np.rint(lo+(hi-lo)*t).astype(int)),255)
def line(im,pts,c,w=1):ImageDraw.Draw(im).line(pts,fill='#'+c,width=w)
def pix(im, x,y,c):
 if 0<=x<64 and 0<=y<64:im.putpixel((x,y),(*tuple(int(a) for a in rgb(c)),255))
def shaded_hex(h, m):return np.clip(np.rint(rgb(h)*m),0,255).astype('uint8').tobytes().hex()
def render(i,seated):
 name,cloth,bright,trou,club,adorn,top,bottom=WARDROBE[i]
 sub=i%10
 head=Image.open(src[i]).convert('RGBA').crop((26,0,39,11))
 # Preserve the R08 face source; detail additions are drawn on the same 64px grid.
 ys,xs=np.where(np.asarray(head)[:,:,3]>0); skin=np.asarray(head)[5:9,:,:3].reshape(-1,3)
 skin=skin[(skin[:,0]>skin[:,2]*1.11)&(skin[:,0]>47)]
 if len(skin): mid=np.median(skin,axis=0).astype('uint8')
 else:mid=np.array([150,94,67],dtype='uint8')
 light=np.clip(mid.astype(float)*1.25,0,255).astype('uint8').tobytes().hex()
 shade=np.clip(mid.astype(float)*.56,0,255).astype('uint8').tobytes().hex()
 body=Image.new('RGBA',(64,64))
 xc=32; head_y=(13+sub%3 if seated else 2+sub%4); y=head_y
 ww=[8,7,8,7,10,8,9,6,8,9][sub]; ytop=y+8; hip=(41+sub%2 if seated else 34+sub%3)
 dtr=shaded_hex(trou,.52); htr=shaded_hex(trou,1.34)
 dco=shaded_hex(cloth,.48)
 # Left-and-right legs stand separately. Seated thighs project forward to visible knees.
 knees=[]
 if seated:
  shape(body,[(xc-ww+2,hip-4),(xc+ww-2,hip-4),(xc+ww-1,hip+1),(xc+4,hip+5),(xc-4,hip+5),(xc-ww,hip+1)],dtr,htr,i,shift=-1,soft=.11)
 for side in (-1,1):
  lx=lambda v:xc+side*v
  if seated:
   skew=(sub%3)-1
   k=9+(sub%3==0)+((sub==7 or sub==8) and side==1)
   ank=k-2+(side*skew if sub in (1,4,8) else 0)
   knee_y=47+(sub%2); hem_y=59
   thigh=[(lx(1),hip-3),(lx(ww-1),hip-5),(lx(ww+2),hip),(lx(k+4),knee_y-3),(lx(k+4),knee_y+1),(lx(k+3),knee_y+3),(lx(k-3),knee_y+2),(lx(k-5),knee_y-1),(lx(1),hip+4)]
   strip=[(lx(k-3),knee_y),(lx(k+3),knee_y),(lx(ank+3),hem_y-1),(lx(ank+2),hem_y+1),(lx(ank-2),hem_y+1),(lx(ank-3),hem_y-4)]
  else:
   k=4+sub%2; ank=4+((sub+1)%3)
   knee_y=48;hem_y=59
   strip=[(lx(0),hip-4),(lx(ww-1),hip-3),(lx(k+3),knee_y),(lx(ank+2),hem_y+1),(lx(ank-2),hem_y+1),(lx(k-2),knee_y-2),(lx(1),hip+6)]
  if bottom=='skirt' and not seated:
   shape(body,[(lx(k-2),40),(lx(k+2),40),(lx(ank+2),hem_y+1),(lx(ank-1),hem_y+1)],dtr,htr,i+side,shift=-1)
  else:shape(body,strip,dtr,shaded_hex(htr,.80) if seated else htr,i+side,shift=-1)
  if seated:
   shape(body,thigh,dtr,shaded_hex(htr,1.10),i+side,shift=-2,soft=.20)
   line(body,[(lx(k-1),knee_y-4),(lx(k+1),knee_y-4),(lx(k+2),knee_y-3)],shaded_hex(trou,1.36))
   line(body,[(lx(k-2),knee_y+2),(lx(k+1),knee_y+3),(lx(k+2),knee_y+2)],shaded_hex(trou,.77))
  shc='8c7d74' if sub in (0,3,7) else ('c7bba4' if sub in (1,4,5,8,9) else '635d58')
  shape(body,[(lx(ank-2),59),(lx(ank+2),59),(lx(ank+3),61),(lx(ank+3),62),(lx(ank-3),62),(lx(ank-3),61)],'212026',shc,i+side,shift=-1,soft=.05)
  if sub in (1,4,5,8,9):line(body,[(lx(ank-2),62),(lx(ank+3),62)],'938d7e')
  knees.append((lx(k),knee_y))
 if bottom=='skirt':
  sk_y=hip+4 if not seated else hip+5
  shape(body,[(xc-ww+1,hip-4),(xc+ww-1,hip-4),(xc+ww+1,sk_y),(xc-ww-2,sk_y)],dtr,htr,i,shift=-2)
 # Sleeves go under torso: supporting hands down for standing, resting toward lap when sitting.
 for side in (-1,1):
  xx=lambda v:xc+side*v
  arx=ww+1
  elb_y=ytop+(18 if seated else 16)
  wrist_y=(hip+2 if seated else hip+1+(sub%3==0))
  if seated:
   handx=xx(8+(sub in (1,6,9)))
   arm=[(xx(ww-3),ytop+2),(xx(ww+2),ytop+3),(xx(arx+2),elb_y-1),(xx(arx),elb_y+4),(handx+side*1,wrist_y+1),(handx-side*2,wrist_y-2),(xx(arx-1),elb_y),(xx(ww-2),ytop+8)]
  else:
   handx=xx(arx+((sub==4 or sub==8) and side==1))
   arm=[(xx(ww-3),ytop+2),(xx(ww+2),ytop+3),(xx(arx+1),elb_y-1),(handx+side,wrist_y),(handx-side*2,wrist_y),(xx(arx-2),elb_y),(xx(ww-2),ytop+9)]
  short=top in ('polo','stripes','mono') and sub in (1,4,7,8)
  shape(body,arm,dco,bright,i+2*side,shift=-1)
  if short:
   shape(body,[(xx(arx-1),elb_y-2),(xx(arx+1),elb_y-2),(handx+side,wrist_y),(handx-side*2,wrist_y)],shade,light,i+side,shift=-1)
  if not seated:shape(body,[(handx+side,wrist_y),(handx+side,wrist_y+3),(handx,wrist_y+4),(handx-side*2,wrist_y+2),(handx-side*2,wrist_y)],shade,light,i+side,shift=-1,soft=.13)
 # Tapered torso and the actual shoulders.
 tx=[(xc-3,ytop-1),(xc+3,ytop-1),(xc+ww,ytop+3),(xc+ww-1,ytop+15),(xc+ww-2,hip),(xc-ww+1,hip),(xc-ww+1,ytop+15),(xc-ww,ytop+3)]
 shape(body,tx,dco,bright,i,shift=-2,soft=.23)
 torso_mask=Image.new('L',(64,64)); ImageDraw.Draw(torso_mask).polygon(tx,fill=255)
 if top in ('mono','stripes','hoop','stripe'):
  for yy in range(ytop+4,hip-1):
   for xx in range(xc-ww+1,xc+ww-1):
    if torso_mask.getpixel((xx,yy))==0:continue
    if (top=='stripes' and (xx-xc+2)%6<=1) or (top=='mono' and (xx-xc+1)%6<=2) or (top=='hoop' and yy-ytop in (7,8,9,14,15)) or (top=='stripe' and yy-ytop==8+(xx-(xc-ww+1))//4):
     amount=.70+.20*(1-abs(xx-xc)/(ww))+.12*(DITHER[(yy+i)%4,xx%4]-.5)
     pix(body,xx,yy,shaded_hex(club,amount))
 if top in ('zip','buttons','coat'):
  line(body,[(xc,ytop+3),(xc,hip)],dco)
  for yy in range(ytop+7,hip-1,4):
   if top=='buttons':pix(body,xc+1,yy,shaded_hex(club,.74))
   if top=='zip' and yy%3:pix(body,xc,yy,shaded_hex(club,.63))
  for side in (-1,1):
   line(body,[(xc+side*(ww-5),hip-7),(xc+side*(ww-2),hip-5)],dco)
 if top in ('polo','stripes','hoop','stripe','mono'):
  line(body,[(xc-3,ytop),(xc,ytop+3),(xc+3,ytop)],shaded_hex(club,.7))
 # Some seated elbows cross into garment width. Draw the hands on top of the thighs.
 if seated:
  for side in (-1,1):
   hx=xc+side*(8+(sub in (1,6,9))); hy=hip+2
   shape(body,[(hx-side*2,hy-1),(hx+side*2,hy-1),(hx+side*1,hy+2),(hx-side*1,hy+3),(hx-side*3,hy+2)],shade,light,i+side,shift=-1,soft=.08)
 # Hair silhouette extras lie behind original R08 face.
 if adorn in ('hood','waves','afro','bun','curls'):
  hc={'hood':dco,'waves':'2a2131','afro':'231b24','bun':'392c2c','curls':'4e3030'}[adorn]
  hh={'hood':cloth,'waves':'704e42','afro':'57413e','bun':'936753','curls':'ab7956'}[adorn]
  if adorn=='hood':shape(body,(xc-6,y-1,xc+5,y+11),hc,hh,i,typ='ellipse');line(body,[(xc-5,y+7),(xc-3,y+11),(xc+2,y+11),(xc+5,y+7)],dco)
  if adorn=='waves':
   shape(body,[(xc-4,y),(xc+4,y),(xc+6,y+5),(xc+6,y+13),(xc+3,y+12),(xc-5,y+14),(xc-6,y+7)],hc,hh,i,shift=-1)
  if adorn=='afro':shape(body,(xc-6,y-1,xc+5,y+8),hc,hh,i,typ='ellipse')
  if adorn=='bun':shape(body,(xc-2,y-1,xc+2,y+3),hc,hh,i,typ='ellipse')
  if adorn=='curls':shape(body,(xc-6,y,xc+5,y+7),hc,hh,i,typ='ellipse')
 body.alpha_composite(head,(26,y))
 if adorn in ('beanie','cap'):
  dark=shaded_hex(club,.50)
  if adorn=='cap':
   shape(body,[(xc-4,y+1),(xc-2,y-1),(xc+2,y-1),(xc+5,y+2),(xc+7,y+3),(xc+4,y+4),(xc-4,y+3)],dark,club,i,shift=-1)
   line(body,[(xc-4,y+3),(xc+4,y+3)],dark)
  else:
   shape(body,(xc-4,y-1,xc+4,y+4),dark,club,i,typ='ellipse')
   line(body,[(xc-4,y+3),(xc+4,y+3)],club)
 if adorn=='scarf':
  shape(body,[(xc-4,ytop),(xc+3,ytop),(xc+4,ytop+3),(xc+1,ytop+5),(xc+4,ytop+6),(xc+4,ytop+17),(xc+1,ytop+17),(xc,ytop+5),(xc-3,ytop+3)],shaded_hex(club,.49),club,i)
  line(body,[(xc+1,ytop+10),(xc+4,ytop+10)],cloth)
  line(body,[(xc+1,ytop+13),(xc+4,ytop+13)],cloth)
 if sub in (2,8):
  line(body,[(xc-3,y+5),(xc+2,y+5)],'403037')
 if sub==3:
  pix(body,xc+4,y+8,'a9a070')
 return body
def run():
 rows=[]
 for pose in ('standing','seated'):
  for i,(name,*_) in enumerate(WARDROBE):
   seated=pose=='seated'; k=i+1;slug=name.lower().replace(' ','_')
   stem=f'ISS_Fan_{pose}_{k:02}_{slug}_south'
   p=P/'Sprites_256'/pose/f'{stem}.png';n=P/'Native_64'/pose/f'{stem}.png'
   p.parent.mkdir(parents=True,exist_ok=True);n.parent.mkdir(parents=True,exist_ok=True)
   im=render(i,seated);im.save(n,optimize=True);hi=im.resize((256,256),N);hi.save(p,optimize=True)
   px=(47+i%2)*4 if seated else None
   rows.append(dict(id=stem,fan_id=f'ISS-FAN-{k:02}',name=name,pose=pose,facing='south',file=str(p.relative_to(P)),native_file=str(n.relative_to(P)),size_px=[256,256],native_px=[64,64],ground_pivot_px=[128,252],ground_uv=[.5,.984375],seat_mount_px=([128,px] if seated else None),seat_mount_uv=([.5,px/256] if seated else None),alpha_bounds_px=list(hi.getbbox()),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
 assert len({r['sha256'] for r in rows})==40
 assert len({r['fan_id'] for r in rows})==20
 (P/'Manifest.json').write_text(json.dumps({'style':'R08 match-engine family','display_pixel_grid':'64 x 64 native, 4x nearest RGBA; no alpha haze','unique_fans':20,'deliverable_sprite_count':40,'assets':rows},indent=2)+'\n')
 with (P/'Manifest.csv').open('w',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=rows[0].keys());wr.writeheader();wr.writerows(rows)
 preview(rows)
def preview(rows):
 board=Image.new('RGB',(1580,790),'#101928');d=ImageDraw.Draw(board)
 def font(sz,b=False):return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans'+('-Bold' if b else '')+'.ttf',sz)
 d.rectangle((0,0,1579,7),fill='#dcad6b');d.text((40,24),'ISS DELUXE SOCCER / TERRACE SUPPORTERS',font=font(39,True),fill='#f6e7cc')
 d.text((42,80),'R08 MATCH-ENGINE CROWD   /   SOUTH IDLE   /   20 FANS, 40 SPRITES',font=font(18,True),fill='#91cbbf')
 d.text((42,109),'Per tile: standing on the left, seated on the right. The stadium seat is not baked into the sprite.',font=font(13),fill='#a5b6c4')
 for row in range(2):
  for col in range(10):
   ii=row*10+col;rr=rows[ii];sr=rows[ii+20];x=40+150*col;y=151+304*row
   d.rounded_rectangle((x,y,x+141,y+274),radius=8,fill='#213148',outline='#4d6878',width=1)
   d.rectangle((x+7,y+39,x+134,y+235),fill='#1b2a3d')
   d.line((x+7,y+232,x+134,y+232),fill='#466171',width=1)
   d.line((x+70,y+39,x+70,y+236),fill='#31475c',width=1)
   d.text((x+8,y+10),f'{ii+1:02}',font=font(16,True),fill='#f2c583')
   d.text((x+40,y+13),'STAND / SIT',font=font(11,True),fill='#98bace')
   for cx,r in zip((38,106),(rr,sr)):
    im=Image.open(P/r['native_file']).convert('RGBA').resize((128,128),N)
    layer=Image.new('RGBA',(141,198));layer.alpha_composite(im,(cx-64,66))
    board.paste(layer,(x,y+39),layer)
   words=rr['name'].split();a=[];b=[]
   for word in words:
    if len(' '.join(a+[word]))<=17 and not b:a.append(word)
    else:b.append(word)
   d.text((x+8,y+238),' '.join(a),font=font(11,True),fill='#f1ede3')
   if b:d.text((x+8,y+254),' '.join(b),font=font(11,True),fill='#f1ede3')
 board.save(P/'Preview.png',optimize=True)
if __name__=='__main__':run()
