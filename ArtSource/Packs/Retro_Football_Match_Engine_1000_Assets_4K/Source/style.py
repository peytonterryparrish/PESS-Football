from PIL import Image,ImageDraw,ImageFont,ImageFilter
import numpy as np
from pathlib import Path
from functools import lru_cache
FONT='/usr/share/fonts/truetype/dejavu/'
N=Image.Resampling.NEAREST
def colors(*xs):return [tuple(bytes.fromhex(x)) for x in xs]
D=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype=float)/16
PALS={
'blue':colors('05083a','070d92','192ee3','344ffd','7695ff'),
'red':colors('280719','721230','b51f37','de4b45','ff8566'),
'purple':colors('070719','190e44','302176','4c319f','654ec6'),
'ice':colors('09102b','122c6d','3151a3','568bd7','b0ceff'),
'gold':colors('24130c','594014','927023','bcb141','f6de76'),
'green':colors('041f26','094138','087c50','199556','6bce83'),
'teal':colors('04192d','04535c','008e8f','11b8ad','95f6dc'),
'stone':colors('070b21','101c3b','21305d','435376','819ab8'),
'metal':colors('100923','261e45','544667','9182ae','d9c9ed'),
'dark':colors('03051a','060c2d','0d1846','11245e','1f3687')
}
@lru_cache(50)
def texture(w,h,pal='purple',variant='foil'):
 c=PALS[pal]; y,x=np.indices((h,w))
 if variant=='bg':f=.18+.22*np.sin((x+y*.5)/57)**2+.09*np.cos((x-y*.35)/62)**2
 elif variant=='grass':f=.27+.24*((x//max(8,w//11))%2)+.06*np.sin((x+y)/3)+.12*np.sin((x*.4-y)/6)**2
 elif variant=='satin':f=.16+.44*np.sin((np.pi*(y+1)/(h+1)))+.16*np.sin((x*.27+y)/8)**2
 elif variant=='weave':f=.21+.4*np.cos(x*.22+y*.24)**2*np.cos(x*.22-y*.24)**2
 else:f=.16+.4*np.sin(np.pi*(y+1)/(h+1))+.13*np.cos((x-y*1.5)/12)+.15*np.sin((-x*.22+y)/4)**2
 if variant=='bg':f*=.88
 q=np.clip(np.floor(f*(len(c)-.4)+D[y%4,x%4]*.85),0,len(c)-1).astype('int')
 return Image.fromarray(np.array(c,dtype='uint8')[q]).convert('RGBA')
@lru_cache(8)
def backdrop(w,h,mode='classic'):
 im=texture(w,h,'dark','bg').copy(); d=ImageDraw.Draw(im)
 def poly(coords,c):d.polygon([(round(x*w),round(y*h)) for x,y in coords],fill=c)
 poly([(0,.60),(.27,.05),(.39,0),(.34,.07),(0,.71)],'#0c2154')
 poly([(.015,.76),(.47,.18),(.46,.13),(0,.65)],'#091a41')
 poly([(.66,0),(.80,0),(.52,.28),(.44,.29)],'#131947')
 poly([(.91,0),(1,0),(.73,.30),(.67,.34),(.71,.17)],'#37174e')
 poly([(.84,0),(.92,0),(.76,.17),(.65,.25),(.69,.16)],'#781959' if mode!='tactical' else '#174b65')
 poly([(1,.18),(1,.63),(.74,1),(.64,1),(.83,.59)],'#071331')
 poly([(0,.95),(.35,.65),(.56,.60),(.41,.91),(.38,1),(0,1)],'#060e2d')
 for i in range(5):
  d.line((0,h//2-i*7,w//3,10-i*7),fill='#172f67')
  d.line((w,h//2+i*12,w*3//4,h+i*12-30),fill='#153269')
 return im
def panel(w,h,pal='purple',edge='teal',lip=True,notched=False):
 im=texture(w,h,pal).copy(); d=ImageDraw.Draw(im)
 shine,mid,low={
 'teal':('#e8fff1','#36caba','#12474e'),'gold':('#fff6c2','#e4b94b','#6b481e'), 'ice':('#e0efff','#7496ed','#1c3872'),
 'red':('#ffddc9','#f57376','#5c203e'),'purple':('#eee2fb','#aa89e2','#38265f')
 }.get(edge,('#e0efff','#7496ed','#1c3872'))
 d.rectangle((0,0,w-1,h-1),outline='#05071c',width=2)
 d.rectangle((2,2,w-3,h-3),outline=mid)
 d.line((3,3,w-4,3),fill=shine,width=2)
 d.line((3,4,3,h-4),fill=shine)
 if lip and h>13:
  d.rectangle((3,h-9,w-4,h-4),fill=mid)
  d.line((3,h-9,w-4,h-9),fill=shine)
  d.line((3,h-4,w-4,h-4),fill=low)
 if notched and min(w,h)>15:
  d.polygon([(0,0),(8,0),(0,8)],fill=(0,0,0,0));d.polygon([(w-1,h-1),(w-10,h-1),(w-1,h-10)],fill=(0,0,0,0))
  d.line((1,8,8,1),fill=mid);d.line((w-10,h-2,w-2,h-10),fill=mid)
 return im
@lru_cache(240)
def label(s,size=21,c='#fffce3',style='sans',stroke=1,maxw=None):
 f={'mono':'DejaVuSansMono.ttf','slant':'DejaVuSansCondensed-BoldOblique.ttf','sans':'DejaVuSansCondensed-Bold.ttf'}[style]
 t=ImageFont.truetype(FONT+f,size);b=t.getbbox(s);w,h=b[2]-b[0],b[3]-b[1]
 im=Image.new('L',(w+4,h+4));ImageDraw.Draw(im).text((2-b[0],2-b[1]),s,font=t,fill=255)
 im=im.point(lambda p:255 if p>110 else 0)
 if maxw and im.width>maxw:im=im.resize((maxw,im.height),N)
 a=Image.new('RGBA',(im.width+2,im.height+2))
 if stroke:
  g=Image.new('RGBA',im.size,'#080718');g.putalpha(im.filter(ImageFilter.MaxFilter(stroke*2+1)));a.alpha_composite(g,(1,2))
 z=Image.new('RGBA',im.size,c);z.putalpha(im);a.alpha_composite(z)
 return a
def field(w,h,with_grass=True):
 im=texture(w,h,'green','grass').copy() if with_grass else Image.new('RGBA',(w,h)); d=ImageDraw.Draw(im)
 x,y=round(w*.055),round(h*.07);X,Y=w-1-x,h-1-y;m=w//2;n=h//2;col='#e9ffba'
 d.rectangle((x,y,X,Y),outline=col,width=2);d.line((m,y,m,Y),fill=col,width=2)
 d.ellipse((m-w*.078,n-h*.15,m+w*.078,n+h*.15),outline=col,width=2)
 d.ellipse((m-2,n-2,m+2,n+2),fill=col)
 for z in (1,-1):
  xa=x if z>0 else X
  for dep,r in [(w*.153,h*.24),(w*.053,h*.10),(-w*.026,h*.052)]:
   l,R=sorted((xa,xa+z*dep)); d.rectangle((l,n-r,R,n+r),outline=col,width=2)
 return im
