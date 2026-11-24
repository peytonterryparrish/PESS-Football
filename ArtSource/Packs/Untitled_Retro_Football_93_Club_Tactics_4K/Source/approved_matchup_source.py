from pathlib import Path
import sys, math, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
P=Path(__file__).parent
S=3; W,H=1280,720; L=280
N=Image.Resampling.NEAREST
colors=lambda *xs:[tuple(bytes.fromhex(x)) for x in xs]
D=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype=float)/16
def fill(w,h,cols,mode='metal',seed=0):
 y,x=np.indices((h,w)); B=D[y%4,x%4]
 if mode=='back':
  t=.22+.22*np.sin((x*1.03+y*.54)/44)**2+.15*np.cos((x-y*.41)/72)**2
 elif mode=='grass':
  t=.25+.24*((x//14)%2)+.06*np.sin((x+y)/3)+.13*np.sin((x*.4-y)/6)**2
 else:
  t=.15+.47*np.sin(np.pi*(y+1)/h)+.11*np.cos((x-y*1.5+seed)/12)+.12*np.sin((-x*.22+y+seed)/4)**2
 a=np.clip(np.floor(t*(len(cols)-.5)+B*.8),0,len(cols)-1).astype(int)
 return Image.fromarray(np.array(cols,dtype='uint8')[a]).convert('RGBA')
im=fill(W,H,colors('02051b','061134','0b255a','112f79','193b88'),'back')
d=ImageDraw.Draw(im)
# Hand-drawn navy stadium rails and offset fluorescent paper streaks, outside the interface stack.
for pts,c in [
 ([(0,398),(436,13),(476,4),(34,453)],'#081741'),
 ([(100,0),(502,0),(431,86),(119,169)],'#091741'),
 ([(853,0),(1153,0),(1042,116),(898,198),(835,165),(936,56)],'#4a1460'),
 ([(906,34),(1070,0),(1133,8),(922,125),(857,177),(851,149)],'#831361'),
 ([(981,39),(1021,33),(924,108),(917,119),(866,145),(887,109)],'#b01572'),
 ([(754,0),(814,6),(588,175),(573,163)],'#0b3866'),
 ([(396,7),(429,7),(638,212),(606,193)],'#112a69'),
 ([(0,610),(233,469),(449,453),(306,571),(147,719),(0,719)],'#06102c'),
 ([(844,632),(1009,607),(1280,396),(1280,719),(978,719)],'#08153a')]:
 d.polygon(pts,fill=c)
for z in range(4):
 d.line((0,241+z*7,358,13+z*7),fill='#192d66',width=1)
 d.line((1280,377+z*11,917,662+z*11),fill='#16265c',width=1)
# Darken under the corner rails without introducing extra panels.
ys,xs=np.indices((H,W));mask=Image.fromarray((np.clip((np.abs(xs-W/2)-385)*.19,0,68)).astype('uint8'),'L')
sh=Image.new('RGBA',(W,H),(0,0,15));sh.putalpha(mask);im.alpha_composite(sh)
def cx(x):return L+x
def draw():return ImageDraw.Draw(im)
def block(x,y,w,h,cols,edge='#99bfbf',rim='#080920',bevel=True):
 x=cx(x); draw().rectangle((x-2,y+2,x+w+1,y+h+3),fill=rim)
 im.alpha_composite(fill(w,h,cols),(x,y))
 t=draw();t.rectangle((x,y,x+w-1,y+h-1),outline=edge)
 if bevel:
  t.line((x+2,y+2,x+w-2,y+2),fill='#d1f0e4')
  t.line((x+2,y+h-2,x+w-2,y+h-2),fill='#119e97')
  t.line((x+1,y+h-4,x+w-2,y+h-4),fill='#0c3346')
def txt(s,x,y,h,c='#fffde5',mode='roster',slant=False,stroke=0,grad=None,mid=True,maxw=None):
 f=('/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-BoldOblique.ttf' if slant else '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf')
 if mode=='roster': f='/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
 if mode=='label':f='/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf'
 ft=ImageFont.truetype(f,h)
 box=ft.getbbox(s,stroke_width=0);w=box[2]-box[0];ht=box[3]-box[1]
 a=Image.new('L',(w+2,ht+2));ImageDraw.Draw(a).text((1-box[0],1-box[1]),s,font=ft,fill=255)
 a=a.point(lambda v:255 if v>=108 else 0)
 if maxw and a.width>maxw:a=a.resize((maxw,a.height),N)
 if mid:x-=a.width//2
 x=cx(x)
 if stroke:
  g=a.filter(ImageFilter.MaxFilter(stroke*2+1));shadow=Image.new('RGBA',a.size,'#070921');shadow.putalpha(g)
  im.alpha_composite(shadow,(x+1,y+2))
 if grad:
  fillim=Image.new('RGBA',a.size); g=ImageDraw.Draw(fillim)
  for yy in range(a.height):g.line((0,yy,a.width,yy),fill=grad[min(len(grad)-1,yy*len(grad)//a.height)])
  fillim.putalpha(a)
 else:fillim=Image.new('RGBA',a.size,c);fillim.putalpha(a)
 im.alpha_composite(fillim,(x,y))
blue=colors('04063c','060ab0','111cec','2a37ff','88a3ff')
red=colors('300817','660e27','a11d28','de503c','f4875b')
green=colors('061f2a','034627','097036','1b9348','38b75f')
gold=colors('2a220b','534014','8b651a','b99124','d0aa4d')
purp=colors('070719','1c0d44','372185','473894','6354bd')
ice=colors('021437','122564','3b62b5','6a9edc','aacbfe')
# Low twin ribbons: the reference has no large modern title header.
block(0,88,356,57,blue,'#5479f7',bevel=False)
block(364,88,356,57,red,'#e37862',bevel=False)
draw().line((cx(0),145,cx(720),145),fill='#ddad3b',width=2)
for x,pal,lab,code in [(131,blue,'HARBOUR','1P'),(441,red,'REDWICK','CPU')]:
 block(x,104,148,44,pal,'#f6d568',bevel=False)
 txt(lab,x+74,113,27,slant=True,stroke=2,mode='sport')
 flagx=x-77 if x<200 else x+151
 block(flagx,97,73,53,ice if x<200 else red,'#212348',bevel=False)
 xx=cx(flagx);dd=draw()
 if x<200:
  dd.polygon([(xx,100),(xx+73,101),(xx+72,112),(xx+51,154),(xx+24,149),(xx,147)],fill='#245ee9')
  dd.polygon([(xx,114),(xx+73,116),(xx+73,131),(xx,135)],fill='#eee7bf')
  dd.polygon([(xx+36,115),(xx+43,123),(xx+36,133),(xx+29,124)],fill='#fbc044')
 else:
  dd.rectangle((xx+1,101,xx+71,148),fill='#cf3546')
  for j in [9,25,41,57]:dd.polygon([(xx+j,104),(xx+j+11,104),(xx+j-5,145),(xx+j-16,145)],fill='#f4e4c9')
 txt(code,flagx+36,62,31,mode='sport',stroke=2,grad=['#fff2a2','#ffdb43','#f19f36','#f54e27'])
for x,pa in [(283,blue),(408,red)]:block(x,103,29,46,pa,'#e3dabc',bevel=False)
# Large warm/cool VS, redrawn pixel contours with no screenshot sample.
x=360
for dx,dy,c in [(5,6,'#06091d'),(3,3,'#152650'),(-2,0,'#fff4dc'),(2,0,'#fff4dc'),(0,-2,'#fff4dc'),(0,2,'#ffeabe')]:
 txt('VS',x+dx,63+dy,85,c,slant=True,mode='sport',maxw=104)
txt('VS',360,63,85,slant=True,mode='sport',maxw=104,grad=['#491728','#702139','#b93132','#f86635','#ffb53c','#f06531','#135bcf','#103889'])
# 22 narrow squad rows, three independently coloured UI cells per row.
left=['S. Rowe','M. Doyle','R. Gill','T. Mercer','J. Rivers','K. Adams','S. Noble','L. Hall','T. Frost','D. Price','A. Levine']
right=['I. Hayes','T. Brook','W. Kerr','S. Oliver','B. York','K. Berg','A. Mora','J. Finch','E. Green','M. Vale','L. Cobb']
roles=['GK','DF','DF','DF','DF','MF','MF','MF','MF','FW','FW']
nums=[[1,2,4,14,3,6,19,9,7,10,11],[1,6,20,25,5,10,3,7,8,9,11]]
for side,players,positions in [(0,left,47),(1,right,483)]:
 x=positions
 draw().line((cx(x+7),173,cx(x+184),173),fill='#4b61ee' if side==0 else '#f94964',width=3)
 draw().line((cx(x+24),173,cx(x+141),173),fill='#add1ff' if side==0 else '#ffd0df',width=1)
 for i,(name,role,num) in enumerate(zip(players,roles,nums[side])):
  y=186+i*42
  pal={'GK':purp,'DF':green,'MF':gold,'FW':red}[role]
  block(x+54,y,134,39,pal,'#b7ddc5',bevel=False)
  q=draw();q.line((cx(x+55),y+37,cx(x+186),y+37),fill='#162631',width=2)
  if role in ['DF','MF'] and i%3!=2:
   alt=fill(53,35,gold if role=='DF' else red);im.alpha_composite(alt,(cx(x+134),y+2))
  q.polygon([(cx(x+2),y+39),(cx(x+15),y+1),(cx(x+39),y+1),(cx(x+39),y+39)],fill='#183da9',outline='#627cf2')
  q.line((cx(x+15),y+3,cx(x+36),y+3),fill='#9ecaff',width=2)
  q.rectangle((cx(x+39),y+1,cx(x+53),y+39),fill='#121223')
  txt(str(num),x+24,y+7,27,mode='sport',slant=True,maxw=28,grad=['#fff598','#f4c842','#fd7332','#e42645'],stroke=1)
  c={'GK':'#d19df6','DF':'#6ff2a0','MF':'#ffd76b','FW':'#ff829c'}[role]
  txt(role[0],x+46,y+3,17,c,mode='sport',stroke=1);txt(role[1],x+46,y+19,17,c,mode='sport',stroke=1)
  txt(name,x+59,y+9,25,mid=False,maxw=123,stroke=1)
# Center menu with tiny, bright embossed teal rails.
block(295,163,130,41,red,'#eea6af')
txt('Continue',361,171,25,mode='label',stroke=1,maxw=116)
for i,(a,b) in enumerate([('Squad','Controls'),('Formation','Camera'),('Tactics','Shirt No.'),('Defence','Edit Team')]):
 y=211+i*47
 for x,s in [(235,a),(361,b)]:
  block(x,y,124,43,purp,'#d2eee7')
  txt(s,x+62,y+9,24,mode='label',stroke=1,maxw=115)
# Small glossy green minimap. Deliberately sits below the controls instead of dominating the screen.
px,py,pw,ph=252,426,221,159
block(px-3,py-3,pw+6,ph+6,ice,'#e2d6a9',bevel=False)
im.alpha_composite(fill(pw,ph,colors('062b1a','0c4729','0b7327','188834','2baf39'),'grass'),(cx(px),py))
q=draw();x0=cx(px)+7;x1=x0+pw-15;y0=py+7;y1=py+ph-8;cy=(y0+y1)//2;mid=(x0+x1)//2
q.rectangle((x0,y0,x1,y1),outline='#e8ffbb',width=2);q.line((mid,y0,mid,y1),fill='#e8ffbb',width=2)
q.ellipse((mid-22,cy-23,mid+22,cy+23),outline='#d0f9ad',width=2)
for sign,ex in [(1,x0),(-1,x1)]:
 for dep,rad in [(40,46),(19,21)]:
  a,b=sorted([ex,ex+sign*dep]);q.rectangle((a,cy-rad,b,cy+rad),outline='#e4f7bf',width=2)
basepts=[(.05,.50),(.19,.24),(.21,.43),(.21,.66),(.19,.82),(.37,.14),(.34,.33),(.36,.52),(.35,.72),(.48,.38),(.47,.61)]
for side in [0,1]:
 for fx,fy in basepts:
  a=x0+round((fx if side==0 else 1-fx)*(x1-x0));b=y0+round(fy*(y1-y0))
  q.rectangle((a-3,b-4,a+3,b+5),fill='#30152c' if side else '#0b223d')
  q.rectangle((a-3,b-5,a+2,b+2),fill='#f36e5d' if side else '#426cec');q.point((a-2,b-4),fill='#ffd1b5' if side else '#b0ccff')
block(297,614,126,42,ice,'#c9d8e0',bevel=False)
draw().ellipse((cx(307),621,cx(334),648),fill='#14267f',outline='#7983f8',width=2)
txt('A',320,627,16,mode='sport',stroke=0)
txt('CONFIRM',376,626,20,mode='sport',maxw=72,stroke=1)
im=im.convert('RGB')
im.resize((3840,2160),N).save(P/'Matchup_4K.png',compress_level=7)
im.crop((280,0,1000,720)).save(P/'Matchup_Preview.jpg',quality=96,subsampling=0)
