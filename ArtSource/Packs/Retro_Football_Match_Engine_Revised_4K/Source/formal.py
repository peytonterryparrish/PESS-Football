"""Formal touchline wardrobe revisions. Input and output: one 64×64 South idle RGBA PNG."""
import numpy as np
from PIL import Image,ImageDraw

WEAR=('suit','suit','polo','suit','polo','suit','suit','polo','suit','suit','suit','polo','suit','suit','polo','suit','suit','polo','suit','suit')
PALETTES=[
 ('191d2a','56647b'),('22232e','6b6d7a'),('293848','7191a1'),('282333','6a5f7b'),
 ('252e2d','627e70'),('302a29','80695f'),('172431','4d677e'),('27323b','667f97'),
 ('2a2b31','737782'),('212a2c','59706d'),('231e2f','675c7e'),('352b35','8b7184'),
 ('27323c','667583'),('282525','746967'),('28343a','68928f'),('2b2936','786e8c'),
 ('1b2433','506983'),('33332b','8a8c68'),('322b2d','80666d'),('222830','61707d')]
TIES=['#855052','#796545','#455474','#63516e','#466e69','#8c5c47','#756858']
SHIRTS=['#d4cabc','#b6c3c6','#c6cad1','#bdafb9','#c7c7ae']
B=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],float)/16
def rgb(s):return np.array(list(bytes.fromhex(s.lstrip('#'))),float)
def coach(im,n):
 if im.size!=(64,64) or not 0<=n<20:raise ValueError('Require 64×64 PNG plus coach index 0–19')
 a=np.array(im.convert('RGBA')); m=a[:,:,3]>0; out=a.copy()
 p=list(map(rgb,PALETTES[n])); pants=p if WEAR[n]=='suit' else list(map(rgb,('212934','686f79') if n%3 else ('39342d','81796a')))
 center=int(np.rint(np.median(np.where(m[6:10])[1])))
 old=a[:,:,:3]@np.array((.23,.67,.10))
 for y in range(11,64):
  xs=np.flatnonzero(m[y]); 
  for x in xs:
   q=x-center
   if y==11 and abs(q)<=2:continue
   if 29<=y<=36 and abs(q)>=7:continue
   l=(.54+.12*np.cos((x-center)*.55)+.18*(B[y%4,(x+n)%4]-.5)+.07*np.sin((y+1.3*x+n)/3))
   if y<37:
    pll=p if y<=(27 if WEAR[n]=='polo' else 34) or abs(q)>6 else pants
    if abs(q)>=6:
     l-=.18
     if y<22 and WEAR[n]=='polo' and y>=20 and abs(q)>7 and y>=21+n%2:
      # Uncolored arm texture already present on hands; use it as the skin palette.
      source=a[30:35,max(0,x-1):x+2,:3]; dark=source.reshape(-1,3)
      sp=dark[dark[:,0]>dark[:,2]+15]
      if len(sp):out[y,x,:3]=np.rint(np.median(sp,axis=0)*(.77+.12*(q<0)+.08*(B[y%4,x%4]))).astype('uint8');continue
    l+=.07*np.cos((abs(q)-4)*1.35)
    if WEAR[n]=='suit' and 18<y<30 and abs(q)==5:l-=.08
   elif y<60:
    pll=pants
    same=xs[xs<=center] if x<=center else xs[xs>center];inner=(x-same[0])/max(1,same[-1]-same[0])
    l=.36+.26*np.sin(inner*np.pi)+.085*(B[y%4,(x+n)%4]-.5)+.075*np.cos((y+x*.5)/5)
    if y>54:l-=.09
   else:
    pll=list(map(rgb,('1a1a22','786047') if n in (4,5,14,19) else ('12151c','626d7a')))
    l={60:.44,61:.76,62:.45,63:.21}[y]
    if x%3==n%3 and y==61:l=min(1,l+.17)
   l=np.clip(np.rint(l*13)/13,.05,.97)
   out[y,x,:3]=np.rint(pll[0]+(pll[1]-pll[0])*l)
 ov=Image.new('RGBA',(64,64)); d=ImageDraw.Draw(ov); x=center
 def pol(pts,co):d.polygon([(x+i,j) for i,j in pts],fill=co)
 def ln(pts,co,wd=1):d.line([(x+i,j) for i,j in pts],fill=co,width=wd)
 sh=SHIRTS[n%len(SHIRTS)]; dark=tuple(np.rint(p[0]).astype(int))+(255,); hem=tuple(np.rint(p[0]*.35+p[1]*.65).astype(int))+(255,)
 if WEAR[n]=='suit':
  tie=n not in (1,8,12,16)
  pol([(-4,12),(-2,12),(0,13),(2,12),(4,12),(2,21),(0,24),(-3,20)],sh)
  if tie:
   pol([(-1,14),(1,14),(1,16),(-1,16)],TIES[n%7])
   pol([(0,16),(1,18),(1,21),(0,23),(-1,21),(-1,18)],TIES[n%7])
  else: pol([(-1,14),(1,14),(0,17)],'#88786f')
  pol([(-5,12),(-3,16),(-4,17),(0,25),(-6,20),(-5,17),(-7,16)],hem)
  pol([(5,12),(3,16),(4,17),(0,25),(6,20),(5,17),(7,16)],hem)
  ln([(0,25),(-1,29),(-2,34)],dark);ln([(0,27),(0,28)],hem)
  ln([(-6,22),(-4,22)],sh if n in (0,5,10,15,18) else hem)
  ln([(3,28),(5,28)],dark);ln([(-5,28),(-3,28)],dark)
  if n in (3,6,13,18):ln([(-3,29),(-3,30)],hem);ln([(2,30),(2,31)],hem)
  for q in (-1,1):ln([(q*8,29),(q*6,29)],sh if n%3==0 else hem)
 else:
  pol([(-5,12),(-2,13),(0,14),(-2,17),(-4,15)],hem)
  pol([(5,12),(2,13),(0,14),(2,17),(4,15)],hem)
  ln([(-5,12),(-2,13)],sh);ln([(5,12),(2,13)],sh)
  ln([(0,15),(0,21)],dark,2);d.point((x,17),fill=sh);d.point((x,19),fill=hem)
  ln([(-6,25),(-6,26),(-3,27),(4,27),(6,26)],hem)
  ln([(-8,20),(-7,20)],hem);ln([(8,20),(7,20)],hem)
  ln([(-6,28),(6,28)],dark);d.point((x,28),fill=sh)
 oa=np.asarray(ov); mm=m&(oa[:,:,3]>0);mm[:12]=False
 out[mm,:3]=oa[mm,:3];out[:11]=a[:11]
 # The old sports collar begins inside rows 8–11 in this atlas. Remove only the
 # red/yellow contemporary garment palette (not head, neck, or hair texels).
 for yy in range(8,12):
  for xx in np.flatnonzero(m[yy]):
   r,g,b=(int(q) for q in a[yy,xx,:3]); off=abs(xx-center)
   red=(r>60 and r>g*1.88 and b>=g*.95 and (off>=2 or yy>=10))
   yellow=(g>125 and b<55 and r>g*1.12)
   if red or yellow:
    l=.48+.15*(off<=4)+.08*(B[yy%4,xx%4]-.5)
    out[yy,xx,:3]=np.rint(p[0]+(p[1]-p[0])*l)
 return Image.fromarray(out,'RGBA')
