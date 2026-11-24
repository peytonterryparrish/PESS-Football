"""160 modular flat radar pieces. Radar projection is intentionally distinct from broadcast pitch."""
from PIL import Image,ImageDraw
from math import sin,cos,pi
from itertools import product
import numpy as np
D=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])
SHAPES={'horizontal':(280,173),'vertical':(173,280),'compact':(194,126)}
PAL={
 'grass':('#09252e','#104c3a','#1b6647','#e3ffbf','#f9fff0'),
 'broadcast':('#060f30','#142857','#1d3971','#afdbe8','#eaf8ff'),
 'night':('#020617','#0b142c','#152245','#7ea5ee','#dcebff'),
 'warm':('#2b101b','#46233f','#613753','#ffce81','#ffeab4'),
 'paper':('#271b2d','#383143','#4d4359','#e6cc9e','#fff3ce'),
 'aqua':('#03222d','#084449','#136166','#8ef1e6','#d7fff5'),
 'mono':('#080b1d','#20283e','#303d54','#d5dbe7','#fbfcff'),
 'high_visibility':('#050716','#101732','#1a2343','#ffe34f','#ffffff')}
TOKEN={
 'classic':('#3373ff','#ff634f','#ffe78d'),
 'coolwarm':('#41d6ff','#ff6845','#f3f9da'),
 'mintplum':('#48eab3','#df85ff','#fffac0'),
 'blueamber':('#5c8efe','#ffb333','#effbff'),
 'lavenderlime':('#c58cff','#ccf74e','#ffffff'),
 'mono':('#f5f7fc','#8999b9','#fff094')}
FORM=['442','433','4231','451','4141','352','343','532','541','4132']
def rgb(s):return tuple(bytes.fromhex(s.lstrip('#')))
def bounds(w,h):return (10,12,w-11,h-13)
def layout(key,pal,mode):
 w,h=SHAPES[key]
 if key=='vertical':
  im=layout_size(h,w,pal,mode).transpose(Image.Transpose.ROTATE_90)
 else:im=layout_size(w,h,pal,mode)
 return im
def layout_size(w,h,pal,mode):
 color=PAL[pal];a=Image.new('RGBA',(w,h));d=ImageDraw.Draw(a)
 u,v,U,V=bounds(w,h)
 if mode=='base':
  for j in range(v,V+1):
   for i in range(u,U+1):
    c=1 if ((i-u)//max(1,(U-u)//10))%2==0 else 2
    c=c if D[j%4,i%4]<(13 if c==1 else 12) else max(0,c-1)
    d.point((i,j),fill=color[c])
  d.rounded_rectangle((2,2,w-3,h-3),radius=6,outline=color[0],width=4)
  d.rounded_rectangle((5,5,w-6,h-6),radius=4,outline=color[3],width=1)
  d.line((13,4,w-14,4),fill=color[4],width=1)
  return a
 m,n=(u+U)//2,(v+V)//2
 c=color[3]
 d.rectangle((u,v,U,V),outline=c)
 d.line((m,v,m,V),fill=c)
 rx,ry=round((U-u)*.087),round((V-v)*.135)
 d.ellipse((m-rx,n-ry,m+rx,n+ry),outline=c);d.ellipse((m-1,n-1,m+1,n+1),fill=c)
 for i in (-1,1):
  x=u if i==1 else U
  for depth,rad in [(round((U-u)*.156),round((V-v)*.295)),(round((U-u)*.054),round((V-v)*.137)),(-5,round((V-v)*.054))]:
   L,R=sorted((x,x+i*depth));d.rectangle((L,n-rad,R,n+rad),outline=c)
  spot=x+i*round((U-u)*.105)
  d.point((spot,n),fill=c)
 return a
def icon(kind,pal):
 h,a,k=TOKEN[pal]; im=Image.new('RGBA',(17,17));d=ImageDraw.Draw(im);b='#070b26'
 color=h if kind.startswith('home') else a if kind.startswith('away') else k
 if 'keeper' in kind:
  d.polygon(((8,1),(15,8),(8,15),(1,8)),fill=b)
  d.polygon(((8,3),(13,8),(8,13),(3,8)),fill=color)
  d.point((8,5),fill='#ffffff')
 elif kind=='ball':
  d.ellipse((3,3,13,13),fill=b);d.ellipse((4,4,12,12),fill='#ffffff')
  d.point((7,6),fill=color);d.point((9,9),fill='#111932')
 elif kind=='official':
  d.rectangle((4,4,12,12),fill=b);d.rectangle((6,6,10,10),fill=color)
 elif kind=='selected':
  d.polygon(((8,0),(15,4),(15,12),(8,16),(1,12),(1,4)),fill=b)
  d.polygon(((8,2),(13,5),(13,11),(8,14),(3,11),(3,5)),outline=color)
  d.ellipse((7,7,9,9),fill='#ffffff')
 elif kind=='sub':
  d.line(((2,5),(12,5),(9,2)),fill=b,width=4);d.line(((14,11),(4,11),(7,14)),fill=b,width=4)
  d.line(((2,5),(12,5),(9,2)),fill=h,width=2);d.line(((14,11),(4,11),(7,14)),fill=a,width=2)
 else:
  d.ellipse((2,2,14,14),fill=b)
  d.ellipse((4,4,12,12),fill=color)
  d.point((7,5),fill='#ffffff')
 return im
def formation(code,side,shape):
 w,h=SHAPES[shape];p=Image.new('RGBA',(w,h));d=ImageDraw.Draw(p)
 U,V,W,H=bounds(w,h); g=[1,*map(int,code)]
 assert sum(g)==11
 c=TOKEN['classic'][0 if side=='home' else 1]
 for j,n in enumerate(g):
  u=.08+j*.37/(len(g)-1)
  x=round(U+(W-U)*(u if side=='home' else 1-u))
  for i in range(n):
   q=.50 if n==1 else .16+i*.68/(n-1)
   y=round(V+(H-V)*q)
   if j==0:
    d.polygon(((x,y-5),(x+5,y),(x,y+5),(x-5,y)),fill=c,outline='#0a0e21')
   else:d.ellipse((x-4,y-4,x+4,y+4),fill=c,outline='#0a0e21')
   d.point((x-1,y-2),fill='#fff')
 return p
def target(style,where):
 a=Image.new('RGBA',(52,52));d=ImageDraw.Draw(a)
 col={'corner':'#ffa345','goal_kick':'#69e4a0','free_kick':'#feeb6c','throw_in':'#9fcbff','penalty':'#ff6666','offside':'#d69cff'}[style]
 x=6 if where.endswith('left') else 45;y=6 if where.startswith('top') else 45
 dx=1 if x<10 else -1;dy=1 if y<10 else -1
 if style=='corner':
  d.line((x,y,x+18*dx,y,x+18*dx,y+5*dy),fill=col,width=2)
  d.line((x,y,x,y+18*dy,x+5*dx,y+18*dy),fill=col,width=2)
  d.ellipse((x-3,y-3,x+3,y+3),fill=col,outline='#050725')
 elif style=='goal_kick':
  d.rectangle((min(x,x+27*dx),min(y,y+21*dy),max(x,x+27*dx),max(y,y+21*dy)),outline=col,width=2)
  d.line((x+12*dx,y+8*dy,x+33*dx,y+33*dy),fill=col,width=2)
 elif style=='free_kick' or style=='penalty':
  x+=9*dx;y+=10*dy
  d.ellipse((x-8,y-8,x+8,y+8),outline=col,width=2)
  d.line((x+7*dx,y+5*dy,x+23*dx,y+21*dy),fill=col,width=2)
  d.point((x,y),fill='#ffffff')
  if style=='penalty':d.line((x+24*dx,y+4*dy,x+24*dx,y+23*dy),fill=col,width=2)
 elif style=='throw_in':
  d.line((x-4,y+13*dy,x+25*dx,y+13*dy),fill=col)
  d.line((x+8*dx,y,x+21*dx,y+25*dy),fill=col,width=2)
 else:
  for i in range(5):
   yy=y+7*i*dy
   d.line((x,yy,x+27*dx,yy),fill=col if i%2==0 else '#301745',width=2)
 return a
def run(a):
 for sh,p in product(SHAPES,PAL):
  for mode in ['base','linework']:
   a.add(f'rad_field_{sh}_{p}_{mode}',layout(sh,p,'base' if mode=='base' else 'lines'),'Minimap Layers',p,'Pair the linework and base of the same size. Add player positions separately.')
 for pal in TOKEN:
  for kind in ['home','away','home_keeper','away_keeper','ball','official','selected','sub']:
   a.add(f'rad_token_{pal}_{kind}',icon(kind,pal),'Minimap Markers',pal,'Centered 17×17 HUD icon.')
 for code,side,length in product(FORM,['home','away'],['horizontal','compact']):
  a.add(f'rad_shape_{length}_{code}_{side}',formation(code,side,length),'Formation Overlays','core','Training template, not live player positions.')
 for sty,where in product(['corner','goal_kick','free_kick','throw_in','penalty','offside'],['top_left','top_right','bottom_left','bottom_right']):
  a.add(f'rad_target_{sty}_{where}',target(sty,where),'Set Piece Radar','core','Optional diagram overlay; independent of live marker placement.')
