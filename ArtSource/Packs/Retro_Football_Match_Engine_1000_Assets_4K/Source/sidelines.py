"""80 original R08-compatible sideline redresses and illustrated prop poses."""
from pathlib import Path
from zipfile import ZipFile
from io import BytesIO
from PIL import Image,ImageDraw
import hashlib, random
import numpy as np
P=Path(__file__).resolve().parent
Z=P.parent/'pix_tactics/r08_input/R08_93_Clubs_South_Idle_Only.zip'
N=Image.Resampling.NEAREST
# Distinct palettes share the R08 controlled-midtone approach, not a recolor of club shirts.
FAB=[
 ('252734','767885'),('222b38','628198'),('332d24','9a8558'),('283630','6d8872'),
 ('2b2035','755f86'),('2c2526','8b5b57'),('1a3038','4c8c91'),('373534','938775'),
 ('233529','778c4e'),('293044','5380ad'),('3a2c21','97765d'),('31263a','827294'),
 ('2b3230','748987'),('40272c','9a646c'),('24272e','646872'),('37352f','9b9069')]
BIB=[('627437','d2d76c'),('794e3a','e39265'),('376877','76c1ce'),('72535e','d98caa'),('2c6e49','78c987'),
 ('2b5684','71aeec'),('816a35','e0c56b'),('7f4a37','d59979'),('62418c','a780d1'),('396767','83c4ba')]
B=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])/16
def H(s): return np.array(tuple(bytes.fromhex(s)),float)
def pt(pos,col,s,x,y,n):
 a,b=map(H,col)
 v=.22+.47*s+.10*np.sin((x+n)*1.09-y*.52)+.17*(B[(y+n)%4,x%4]-.5)
 v=np.clip(round(v*11)/11,0,1)
 return tuple(np.rint(a+(b-a)*v).astype(int))+(255,)
def source():
 v=sorted((P/'sideline_inputs').glob('F*.png'))
 assert len(v)==80,len(v)
 return [Image.open(p).convert('RGBA') for p in v]
def match_line(xs,y,x):
 L,R=xs[0],xs[-1]
 return (x-L)/max(1,R-L)
def finish(im,n,role):
 # Details isolated to an authentic shirt/sleeve region; added props are original pixels.
 d=ImageDraw.Draw(im)
 base=np.asarray(im); m=base[:,:,3]>0; ys,xs=np.where(m)
 x0=int(np.median(np.where(np.any(m[22:34],axis=0))[0]));mid=32 if not 27<=x0<=36 else x0
 accent=['#a4a287','#7e9b9f','#a19880','#98a5af','#b5a078','#8b9d8b'][n%6]
 def stroke(seq,color,width=1):
  for a,b in zip(seq,seq[1:]):
   mask=Image.new('1',(64,64));ImageDraw.Draw(mask).line((a,b),fill=1,width=width)
   ma=np.asarray(mask)&m
   for yy,xx in zip(*np.where(ma)):im.putpixel((xx,yy),tuple(bytes.fromhex(color[1:]))+(255,))
 if role=='head_coach':
  if n%4==0:
   stroke([(mid-5,11),(mid,16),(mid+5,11)],'#b2aa96')
   stroke([(mid,15),(mid+1,22),(mid,26)],['#835946','#536e87','#7b6740'][n%3],2)
   stroke([(mid-6,11),(mid-4,17),(mid-6,21)],accent)
   stroke([(mid+6,11),(mid+4,17),(mid+7,21)],accent)
  elif n%4==1:
   stroke([(mid-5,11),(mid,13),(mid+6,11)],accent,2)
   stroke([(mid,14),(mid,33)],'#29283a',2)
   stroke([(mid-7,28),(mid-4,24)],accent);stroke([(mid+7,28),(mid+4,24)],accent)
  elif n%4==2:
   stroke([(mid-8,15),(mid-2,19),(mid,17),(mid+2,19),(mid+8,15)],accent,2)
   stroke([(mid-6,22),(mid+6,22)],'#444237')
  else:
   stroke([(mid,13),(mid+1,33)],'#22232f',2)
   for y in (18,23,28):d.point((mid+1,y),fill=accent)
 elif role=='assistant':
  if n%2==0:
   stroke([(mid-5,11),(mid,24),(mid+5,11)],accent)
   d.rectangle((mid-2,23,mid+2,28),fill='#869d98',outline='#344050')
  if n%3==0:
   d.polygon([(mid-10,26),(mid-4,26),(mid-5,37),(mid-11,36)],fill='#a58c62',outline='#343745')
   d.line((mid-8,29,mid-6,29),fill='#dfcf9a')
  else:
   stroke([(mid-9,17),(mid-4,17)],accent,2)
   if n%3==1:stroke([(mid+8,13),(mid+9,23)],accent,2)
 elif role=='warmup':
  pal=BIB[n%len(BIB)]
  for y in range(11,28):
   xl=mid-(5 if y<14 else 7);xr=mid+(5 if y<14 else 7)
   for x in range(xl,xr+1):
    if not m[y,x] or (y<15 and abs(x-mid)<(15-y)//2):continue
    im.putpixel((x,y),pt('',pal,.63+.22*(1-abs(x-mid)/8),x,y,n))
  stroke([(mid-6,27),(mid+6,27)],'#'+pal[0],1)
  if n%3==0:stroke([(mid-6,18),(mid-3,19),(mid+5,19)],'#'+pal[1],1)
 elif role=='ball_boy':
  if n%2==0:
   stroke([(mid-7,16),(mid+7,16)],accent,2)
   stroke([(mid-4,12),(mid,15),(mid+4,12)],'#3a4051',2)
  elif n%3==0:stroke([(mid,12),(mid,27)],accent)
  else:stroke([(mid-6,22),(mid,25),(mid+6,22)],accent,1)
 return im
def redress(source,n,role):
 src=np.asarray(source);out=np.array(source); mask=src[:,:,3]>0
 pal=FAB[(n+(1 if role=='camera' else 0))%len(FAB)]
 pants=FAB[(n//2+2)%len(FAB)] if role in ('assistant','ball_boy') else FAB[(n*3+7)%len(FAB)]
 if role=='camera':pal=FAB[(n%4)*2];pants=FAB[(n%4)*2+1]
 if role=='warmup':pants=pal
 rgb=src[:,:,:3]@np.array([.23,.66,.11]); rand=random.Random(n+7)
 # Trouser hems follow each individual's existing leg axes; softly widen inward with no joint shift.
 for y in range(11,63):
  xs=np.flatnonzero(mask[y])
  if not len(xs):continue
  L,R=int(xs[0]),int(xs[-1]); C=(L+R)*.5
  if 37<=y<=57 and role!='ball_boy':
   for isright in (False,True):
    ss=xs[(xs>=32) if isright else (xs<32)]
    if ss.size>1:
     s1=int(ss[-1] if isright else ss[0]);s2=int(ss[0] if isright else ss[-1])
     if y<52:s2-=1 if isright else -1
     if 40<=y<51 and (y+n)%8<5:s1+=1 if isright else -1
     for x in range(min(s1,s2),max(s1,s2)+1):
      if 1<=x<63 and not mask[y,x]:
       dnr=min(ss,key=lambda q:abs(x-q))
       out[y,x]=pt('',pants,.38,x,y,n)
  for x in xs:
   left=x<=C;edge=min(x-L,R-x)
   shade=np.clip(.29+rgb[y,x]/340+.24*(1-abs(x-C)/(1+max(3,(R-L)/2))),0,1)
   if y<=12 and abs(x-C)<3 and not (int(src[y,x,0])-int(src[y,x,1])<42 and int(src[y,x,1])>95 and int(src[y,x,2])<75):continue
   if 29<=y<=35 and edge<3:continue # original exposed hands and fingertips
   if role=='ball_boy' and 38<=y<=47:
    continue
   if y>=59:
    shoecol=('#282733','#66695a') if n%3 else ('#3a332d','#92806c')
    out[y,x]=pt('',tuple(s[1:] for s in shoecol),shade,x,y,n)
    continue
   change=(role in ('head_coach','camera') and 35+n%3<=y) or (role in ('assistant','warmup','ball_boy') and y>=30)
   cloth=pants if change else pal
   if role=='ball_boy' and 48<=y<=58:cloth=('3a403f','a5a69a')
   val=shade-(.22 if edge==0 and 11<y<30 else 0)
   # Leg fabric creases differ from source skin texture; add cloth folds and longer kit stripe.
   if 36<=y<=58 and role!='ball_boy':
    leg=xs[xs>=32] if x>=32 else xs[xs<32]
    if len(leg):
     off=x-int(leg[0]);val=.36+.24*np.cos((off/(max(2,len(leg)-1))-.38)*2.65)+.19*np.sin((y-x*.4+n)/4)
     if role=='warmup' and (x==int(leg[0]) or x==int(leg[-1])):val+=.24
   out[y,x]=pt('',cloth,val,x,y,n)
 im=finish(Image.fromarray(out,'RGBA'),n,role)
 if role=='ball_boy':
  # Scale the whole person uniformly, retaining the original anatomical ratios.
  k=51+n%4
  im=im.resize((k,k),N);b=im.getbbox()
  whole=Image.new('RGBA',(64,64));whole.alpha_composite(im,((64-k)//2,63-b[3]));im=whole
 return im
def ball(im,n):
 d=ImageDraw.Draw(im);x=43+(n%2)*3 if n%4<2 else 19-(n%2)*3;y=58
 d.ellipse((x-5,y-6,x+5,y+4),fill='#dedbca',outline='#333442')
 d.polygon([(x-2,y-3),(x+1,y-4),(x+3,y-1),(x+1,y+2),(x-2,y+1)],fill='#514d51')
 d.line((x-4,y-1,x-2,y+1),fill='#929891');d.point((x+3,y+2),fill='#94948a')
 return im
def cam(im,n):
 # Offset original camera body/tripod; keep rear leg and foot pixels as independent supports.
 d=ImageDraw.Draw(im); x=(46 if n%2==0 else 17); y=23+n%5
 dark='#202532';sl='#414a57';hi='#899292';cool=['#60798a','#7b837a','#86776b','#797690'][n%4]
 if n%5 in (1,4):
  d.line((x,y+6,x,56),fill=sl,width=2);d.line((x-2,57,x+2,58),fill=dark,width=2)
  d.line((x-3,y+4,x-10,y+9),fill=dark,width=2)
 else:
  d.line((x,y+4,x-7,62),fill=dark,width=2);d.line((x,y+4,x+8,62),fill=dark,width=2)
  d.line((x,y+7,x,63),fill=sl,width=2)
  d.line((x,y+12,x-6,62),fill=hi);d.line((x+1,y+10,x+8,61),fill=cool)
  for xx in (x-7,x,x+8):d.line((xx-1,62,xx+1,62),fill='#24262c')
 if n%3==0:
  d.rectangle((x-7,y-6,x+7,y+4),fill=sl,outline=dark,width=1)
  d.rectangle((x-5,y-8,x,y-6),fill=hi);d.line((x-6,y-5,x+4,y-5),fill=hi)
 elif n%3==1:
  d.polygon([(x-8,y-6),(x+4,y-6),(x+9,y-3),(x+9,y+3),(x-8,y+3)],fill=cool,outline=dark)
  d.rectangle((x-8,y-9,x-2,y-6),fill=sl,outline=dark);d.point((x-7,y-8),fill=hi)
 else:
  d.polygon([(x-7,y-5),(x-3,y-7),(x+7,y-6),(x+8,y+4),(x-7,y+4)],fill=sl,outline=dark)
  d.line((x-5,y-5,x+5,y-5),fill=cool);d.rectangle((x-3,y-10,x+3,y-7),fill=sl)
 d.rectangle((x-3,y-4,x+3,y+3),fill=dark);d.ellipse((x-2,y-3,x+2,y+2),fill='#344759')
 d.point((x-1,y-2),fill='#8eabb4');d.point((x+6,y-4),fill='#a45757')
 return im
def make_all():
 src=source();i=0
 for role,count,notes in [
  ('head_coach',20,'Touchline manager in suit, zipped coat, pullover or button-front outerwear'),
  ('assistant',20,'Technical staff: jackets with notebooks, staff passes or practical outer pockets'),
  ('warmup',20,'Training kit with distinct bib, long sleeves and long tracksuit trousers'),
  ('ball_boy',10,'Ball attendant in neutral athletic kit; smaller scale with native ground anchor'),
  ('camera_operator',10,'Broadcast crew with monopod or tripod camera; display as one idle silhouette')]:
  for n in range(count):
   im=redress(src[i],i,role if role!='camera_operator' else 'camera')
   if role=='ball_boy':im=ball(im,n)
   if role=='camera_operator':im=cam(im,n)
   i+=1
   yield f'sid_{role}_{n+1:03}',im,role,notes
def run(a):
 for k,im,fam,n in make_all():
  a.add(k,im,'sidelines','sideline_'+fam,n+'; south idle; native pivot (32,63).')
