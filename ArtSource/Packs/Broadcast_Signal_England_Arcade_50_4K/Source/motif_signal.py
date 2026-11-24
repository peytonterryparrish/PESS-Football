"""Text-free pixel broadcaster motifs for Broadcast Signal alternate plates."""
import math
import random
from PIL import Image,ImageDraw
import numpy as np

BAYER=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])
def mix(a,b,t):return tuple(round(x*(1-t)+y*t) for x,y in zip(a,b))
def painter(w,h):
 im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
 def ln(seq,fill,width=1):
  d.line([(round(x*(w-1)),round(y*(h-1))) for x,y in seq],fill,width=max(1,round(width*min(w,h)/100)))
 def box(a,b,c,e,fill):d.rectangle((round(a*(w-1)),round(b*(h-1)),round(c*(w-1)),round(e*(h-1))),fill)
 def p(seq,fill):d.polygon([(round(x*(w-1)),round(y*(h-1))) for x,y in seq],fill)
 return im,d,ln,box,p
def stipple(im,strength,rng):
 a=np.asarray(im).copy()
 y,x=np.indices(a.shape[:2]); edges=(x<.10*a.shape[1])|(x>.90*a.shape[1])|(y<.16*a.shape[0])|(y>.83*a.shape[0])
 b=BAYER[y%4,x%4]; m=(a[:,:,3]>0)
 # Keep curated shape outlines sharp, weather only occasional rib pixels.
 al=a[:,:,3].astype(float)
 al[m&edges&(b<strength)]=al[m&edges&(b<strength)]*.56
 scan=m&((y+2)%4==0)&(b<10);al[scan]*=.86
 a[:,:,3]=al.astype(np.uint8);return Image.fromarray(a)
def draw(layer,kind,x,y,w,h,style,seed):
 w,h=int(w),int(h); assert w>0 and h>0
 x,y=round(x),round(y)
 im,d,ln,box,p=painter(w,h);R=random.Random(seed)
 dark,mid,hi,shine,red,ice=[tuple(style[k][:3]) for k in ('dark','mid','hi','shine','red','ice')]
 if kind=='prism':
  # Three long signal wedges broken by a clean stepped splice. The open
  # shell trims toward one side for a purposeful title-safe negative wedge.
  p([(0,.08),(.98,.08),(.81,.27),(.47,.27),(.35,.39),(0,.45)],(*dark,154))
  p([(0,.05),(.96,.05),(.84,.20),(.51,.22),(.42,.30),(0,.37)],(*hi,145))
  p([(0,.095),(.94,.075),(.86,.115),(.55,.137),(.45,.19),(0,.27)],(*ice,210))
  p([(0,.122),(.71,.122),(.61,.16),(.44,.17),(.33,.26),(0,.29)],(*shine,150))
  p([(0,.43),(.27,.36),(.33,.40),(.52,.41),(.66,.29),(.82,.29),(.71,.44),(.61,.50),(.43,.52),(.36,.62),(0,.75)],(*mid,218))
  p([(0,.465),(.25,.405),(.35,.445),(.59,.418),(.69,.326),(.77,.326),(.65,.478),(.48,.52),(.29,.62),(0,.67)],(*red,210))
  p([(0,.565),(.25,.48),(.36,.50),(.51,.472),(.60,.399),(.64,.399),(.57,.49),(.43,.57),(.30,.61),(0,.69)],(*red,147))
  for j in range(15):
   yy=.13+j*.044;end=min(.99,.62+j*.026)
   cut=R.uniform(.09,.40)
   ln([(cut,yy),(end,yy)],(*mix(hi,ice,j/16),R.randrange(27,83)))
   if j%3==0:ln([(end,yy),(end+(.96-end)*.43,yy)],(*shine,57))
  for a,b,c,e in [(.11,.67,.31,.691),(.32,.59,.50,.61),(.57,.51,.66,.535),(.64,.465,.74,.487),(.03,.75,.16,.77)]:
   box(a,b,c,e,(*ice,178))
  for j in range(13):
   a=R.uniform(.04,.44); t=R.uniform(.26,.88)
   if t>.50+.55*(.5-a):continue
   box(a,t,min(.95,a+R.uniform(.01,.09)),min(.97,t+.011),(*shine,R.randrange(35,114)))
 elif kind=='bars':
  # Intentional CRT engineering sigil: contrasting test strips with no
  # lettering or channel numerals, beveled side and small warm afterglow.
  p([(.03,.16),(.90,.16),(.98,.30),(.98,.77),(.88,.92),(.03,.92),(0,.84),(0,.25)],(*dark,188))
  p([(.025,.24),(.91,.24),(.945,.31),(.945,.78),(.895,.85),(.025,.85)],(*mid,133))
  cols=[shine,(244,218,69),ice,(87,199,83),(189,86,201),red,mix(red,ice,.35)]
  # Unequal bars and small cells below the transitions evoke a color card
  # specifically from television hardware rather than a generic gradient.
  xs=[.070,.185,.305,.424,.543,.659,.777,.885]
  for j,col in enumerate(cols):
   α=202 if j in (1,5) else 171
   y0=.31+(.016 if j%3==2 else 0.0);y1=.59-((j%3)*.009)
   box(xs[j],y0,xs[j+1]-.021,y1,(*col,α))
   box(xs[j],y1+.025,xs[j+1]-.021,y1+.068,(*mix(col,dark,.39),164))
   box(xs[j],.707,xs[j+1]-.022,.738,(*mix(col,shine,.36),116))
   if j in (0,2,5):ln([(xs[j],.29),(xs[j+1]-.025,.29)],(*shine,145))
  ln([(.03,.23),(.89,.23),(.93,.29)],(*mix(hi,ice,.42),104),2)
  ln([(.05,.796),(.81,.796),(.86,.765)],(*red,105),2)
  for j in range(9):
   xx=.10+j*.095;box(xx,.873,xx+.032,.890,(*ice,61))
 elif kind=='waveform':
  # Edge-sized analog vectors. Graticule and dashes sit under the traces;
  # a glass outer frame is deliberately incomplete.
  p([(0,.20),(.72,.20),(.88,.29),(1,.29),(1,.81),(.11,.81),(0,.70)],(*dark,142))
  for j in range(8):
   zz=.25+j*.072
   ln([(.03,zz),(.94,zz)],(*ice,25))
  for j in range(12):
   xx=.05+j*.079
   ln([(xx,.24),(xx,.75)],(*hi,27))
  for j in range(30):
   xx=.04+j*.031; ln([(xx,.519),(xx,.54 if j%3==0 else .53)],(*shine,52))
  # Localized burst envelopes avoid flattening the composition into stripes.
  ts=np.linspace(.015,.984,max(36,min(160,w)))
  for j,(hue,alpha,cy,f) in enumerate([(hi,127,.56,10),(red,119,.55,7),(ice,199,.48,6)]):
   env=.032+.11*np.exp(-((ts-(.26+j*.225))/.15)**2)
   wv=env*(np.sin(ts*f*math.tau+.62*j)+.27*np.cos(ts*math.tau*(f*1.9)))
   path=list(zip(ts,np.clip(cy+wv,.25,.77)))
   ln(path,(*hue,alpha//4),5)
   ln(path,(*hue,alpha),1.8)
  ln([(.005,.21),(.63,.21),(.71,.25),(.91,.25)],(*mix(hi,ice,.49),103),2)
  ln([(.12,.787),(.32,.787)],(*hi,113),2)
  ln([(.83,.783),(.94,.783),(.97,.747)],(*red,148),2)
 else:raise KeyError(kind)
 layer.alpha_composite(stipple(im,5,R),(x,y))
