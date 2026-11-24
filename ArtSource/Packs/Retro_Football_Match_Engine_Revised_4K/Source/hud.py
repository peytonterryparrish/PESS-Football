"""Modular original match HUD, logical pixel sprites (260 assets)."""
from PIL import Image,ImageDraw
import style
from collections import namedtuple

Theme=namedtuple('Theme','key pal edge shade rim white tint')
THEMES=[
 Theme('broadcast','ice','ice','#1d3571','#769df2','#e8f5ff','#2a4581'),
 Theme('night','purple','teal','#193d4d','#4fd3c7','#d9fff1','#263159'),
 Theme('arcade','red','red','#581e40','#ed7479','#ffdcd0','#53264e'),
 Theme('contrast','teal','gold','#55352c','#fadc70','#fff7bb','#213e44'),
 Theme('cup','metal','purple','#3f2b64','#b38de5','#f8e9ff','#3d2b5d'),
]
def frame(w,h,p,stylekind='flat',accent=None):
 im=style.panel(w,h,accent or p.pal,p.edge,h>27,False)
 d=ImageDraw.Draw(im)
 if stylekind=='tab':
  d.polygon([(0,h-1),(0,h-11),(12,h-1)],fill=(0,0,0,0));d.line((1,h-10,12,h-1),fill=p.rim,width=2)
  d.polygon([(w-1,0),(w-1,11),(w-13,0)],fill=(0,0,0,0));d.line((w-13,0,w-1,11),fill=p.rim,width=2)
 elif stylekind=='cut':
  d.polygon([(0,0),(12,0),(0,9)],fill=(0,0,0,0));d.line((1,9,13,0),fill=p.rim,width=2)
  d.polygon([(w-1,h-1),(w-14,h-1),(w-1,h-10)],fill=(0,0,0,0));d.line((w-14,h-1,w-2,h-10),fill=p.rim,width=2)
 elif stylekind=='delta':
  d.polygon([(w-1,h-1),(w-1,0),(w-18,h-1)],fill=(0,0,0,0));d.line((w-19,h-1,w-2,1),fill=p.rim,width=2)
 d.line((20,6,min(w-15,44),6),fill=p.white)
 return im
def well(d,b,p,fill='#0a1029'):
 x,y,X,Y=b
 if min(X-x,Y-y)<4:return
 d.rectangle(b,fill=fill,outline=p.shade)
 d.line((x+2,y+1,X-2,y+1),fill=p.tint)
 d.line((x+2,Y-1,X-2,Y-1),fill='#070a22')
def dash(d,x,y,p,w=18):
 d.line((x,y,x+w,y),fill=p.rim)
 d.line((x+2,y,x+6,y),fill=p.white)
def wedge(d,x,y,p,direction=1):
 d.polygon([(x,y-6),(x+direction*7,y),(x,y+6),(x-direction*3,y+4),(x-direction*3,y-4)],fill=p.rim,outline=p.white)
def scoreboard(p,k):
 w,h={'center_duel':(401,58),'compact_corner':(256,43),'cup_aggregate':(444,72),
      'two_line':(216,79),'shootout_header':(326,64),'home_identity':(157,43),'away_identity':(157,43),'score_pair':(114,43)}[k]
 im=frame(w,h,p,'tab' if k not in ('home_identity','away_identity') else 'delta',
          'blue' if k=='home_identity' else 'red' if k=='away_identity' else None)
 d=ImageDraw.Draw(im)
 if k.endswith('identity'):
  well(d,(10,9,37,h-11),p);well(d,(43,9,w-20,h-11),p)
  if k=='away_identity':im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
  return im
 if k=='two_line':
  for y in (10,38):
   for x,X in ((10,37),(43,w-57),(w-50,w-17)):
    well(d,(x,y,X,y+22),p)
  dash(d,48,67,p,72)
 elif k=='score_pair':
  for x,X in ((9,46),(62,w-14)):well(d,(x,10,X,h-13),p)
  for y in (15,21):d.rectangle((w//2-1,y,w//2+1,y+2),fill=p.white)
 elif k=='compact_corner':
  for x,X in ((10,87),(92,119),(125,152),(158,w-17)):
   well(d,(x,11,X,h-12),p)
 elif k=='shootout_header':
  for x,X in ((10,89),(95,139),(187,233),(240,w-17)):
   well(d,(x,11,X,36),p)
  for x in (20,174):
   for i in range(5):d.ellipse((x+i*21,43,x+i*21+8,51),fill='#121c3a',outline=p.rim)
 else:
  mid=w//2
  for x,X in ((10,47),(53,mid-56),(mid-50,mid-8),(mid+8,mid+50),(mid+56,w-72),(w-66,w-19)):
   well(d,(x,10,X,40),p)
  d.rectangle((mid-2,22,mid+2,25),fill=p.white)
  if k=='cup_aggregate':
   for x,X in ((mid-104,mid-11),(mid+11,mid+104)):
    well(d,(x,46,X,h-12),p)
  else:dash(d,66,45,p,58);dash(d,w-124,45,p,58)
 return im
def clock(p,k):
 w,h={'match_wide':(127,38),'mini_clock':(84,25),'added_time':(83,33),'extra_period':(139,35),
      'countdown':(123,39),'hold_badge':(70,29)}[k]
 im=frame(w,h,p,'cut','gold' if k=='added_time' else None)
 d=ImageDraw.Draw(im);well(d,(13,7,w-13,h-9),p)
 if k in ('match_wide','mini_clock','countdown'):
  x=w//2;y=(h-3)//2
  for yy in (y-4,y+2):d.rectangle((x-1,yy,x+1,yy+1),fill=p.white)
  if k=='countdown':
   for i in range(5):d.rectangle((16+i*9,h-8,21+i*9,h-7),fill=p.rim if i<4 else p.shade)
 elif k=='added_time':
  x,y=22,14;d.line((x-4,y,x+4,y),fill=p.white,width=2);d.line((x,y-4,x,y+4),fill=p.white,width=2);d.line((34,9,34,h-10),fill=p.rim)
 elif k=='extra_period':
  for x in (22,27):d.rectangle((x,12,x+2,17),fill=p.rim)
  d.line((36,9,36,h-10),fill=p.rim)
 else:
  for x in (18,24):d.rectangle((x,10,x+2,16),fill=p.white)
 return im
def namebar(p,k):
 side,state=k.split('_',1)
 w,h={'normal':(250,44),'compact':(191,30),'selected':(250,44),'sub':(250,49)}[state]
 im=frame(w,h,p,'delta','blue' if side=='left' else 'red')
 d=ImageDraw.Draw(im)
 well(d,(9,8,38,h-12),p,'#09112b')
 well(d,(44,8,w-34,25 if state=='sub' else h-12),p)
 if state=='selected':
  d.rectangle((43,h-10,w-38,h-8),fill='#ffdc68')
  wedge(d,w-24,13,p,1)
 elif state=='sub':
  well(d,(44,29,w-34,h-12),p,'#142431')
  for x in (10,21,32):d.point((x,h-9),fill='#85ffd8')
  d.line((12,10,32,10),fill='#90ffb4')
 elif state=='normal':
  for x in (53,62,71):d.line((x,h-9,x+4,h-9),fill=p.rim)
 # mirrored mechanical layout, not mirrored lettering (art contains no letters)
 if side=='right':im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
 return im
def meter(p,k):
 base,level=k.split('_',1)
 w,h=(178,22) if base=='fitness' else (216,26)
 if level=='fill':
  wp=w-20;hp=8 if base=='fitness' else 12
  im=style.texture(wp,hp,p.pal).copy();d=ImageDraw.Draw(im)
  d.line((0,0,wp-1,0),fill=p.rim);d.line((0,hp-1,wp-1,hp-1),fill=p.shade)
  if base=='power':
   x=round(wp*.79);d.line((x,1,x,hp-2),fill='#fff4c5')
  return im
 if level=='marker':
  im=Image.new('RGBA',(21,23));d=ImageDraw.Draw(im)
  if base=='power':
   d.line((10,1,10,22),fill=p.white,width=2)
   d.polygon([(4,1),(16,1),(10,9)],fill=p.rim,outline=p.white)
  else:
   d.polygon([(5,2),(15,2),(19,8),(10,21),(1,8)],fill=p.shade,outline=p.white)
   d.line((6,4,14,4),fill=p.rim,width=2)
  return im
 im=frame(w,h,p,'cut')
 d=ImageDraw.Draw(im);well(d,(9,5,w-10,h-6),p)
 frac=int(level)/100
 span=w-24
 if base=='fitness':
  seq=10; n=round(frac*seq)
  for j in range(seq):
   x=11+j*span//seq;X=7+(j+1)*span//seq
   if j<n:
    fill=('#ee7175' if n<=2 else '#edc65e' if n<=4 else p.rim)
    d.rectangle((x,8,X,h-8),fill=fill)
    d.line((x,8,X,8),fill=p.white)
   else:d.rectangle((x,8,X,h-8),fill=p.tint)
 else:
  limit=11+round(span*frac)
  if frac:
   im.alpha_composite(style.texture(limit-11,10,'green' if frac<=.4 else 'gold' if frac<=.8 else 'red'),(11,8))
  d=ImageDraw.Draw(im)
  for j in (.2,.4,.6,.8):
   xx=11+round(span*j);d.line((xx,17,xx,19),fill=p.white)
  if frac:
   d.line((limit-1,6,limit-1,18),fill=p.white,width=2)
  for j in (.69,.8):
   xx=11+round(span*j);d.point((xx,6),fill='#fce07b')
 return im
def pause(p,k):
 w,h={'title':(336,49),'body':(338,285),'menu_default':(262,40),'menu_selected':(262,40),
      'menu_disabled':(262,40),'quick_option':(143,37),'footer':(337,32),'divider':(277,16)}[k]
 im=frame(w,h,p,'cut','gold' if k=='menu_selected' else 'stone' if k=='menu_disabled' else None)
 d=ImageDraw.Draw(im)
 if k=='body':
  well(d,(16,16,w-17,h-19),p,'#070e2d')
  for y in (41,85,131,179,224):d.line((38,y,w-39,y),fill=p.shade)
 elif k=='title':
  for x in (20,26):d.rectangle((x,15,x+2,28),fill=p.white)
  d.line((39,10,39,h-13),fill=p.rim)
  dash(d,w-55,16,p,24)
 elif k.startswith('menu_'):
  well(d,(15,9,w-16,h-12),p,'#302947' if k=='menu_selected' else '#10152f' if k=='menu_default' else '#1b2036')
  if k=='menu_selected':wedge(d,25,17,p);d.line((18,h-10,w-21,h-10),fill='#ffdc6a',width=2)
  if k=='menu_disabled':d.line((25,13,25,h-16),fill=p.shade,width=2)
 elif k=='quick_option':
  well(d,(13,9,w-13,h-10),p);d.line((31,11,31,h-12),fill=p.rim)
 elif k=='footer':
  for i in range(3):
   x=15+i*103;d.ellipse((x,8,x+11,19),fill=p.shade,outline=p.white)
   d.line((x+16,19,x+85,19),fill=p.tint)
 else:
  d.line((13,8,w-16,8),fill=p.rim)
  d.line((w//2-12,8,w//2+12,8),fill=p.white)
 return im
def setpiece(p,k):
 w,h={'command_ribbon':(318,45),'direction_panel':(194,116),'taker_well':(236,40),
      'range_well':(108,38),'beat_well':(134,38),'defensive_wall_selector':(183,41),
      'accuracy_track':(213,29),'trajectory_key':(175,39)}[k]
 im=frame(w,h,p,'tab');d=ImageDraw.Draw(im)
 if k=='direction_panel':
  well(d,(14,10,w-16,h-15),p)
  x,y=w//2,h//2
  d.ellipse((x-43,y-30,x+43,y+30),outline=p.shade,width=2)
  d.ellipse((x-27,y-19,x+27,y+19),outline=p.rim)
  for a,b,A,B in [(x-48,y,x-12,y),(x+12,y,x+48,y),(x,y-32,x,y-10),(x,y+10,x,y+32)]:
   d.line((a,b,A,B),fill=p.rim,width=2)
  d.polygon([(x,y-7),(x+7,y),(x,y+7),(x-7,y)],fill=p.white)
 elif k=='command_ribbon':
  well(d,(13,10,w-19,h-13),p)
  for x in (28,111,188,262):d.ellipse((x,14,x+11,25),fill=p.shade,outline=p.white)
 elif k=='accuracy_track':
  well(d,(14,7,w-18,h-9),p)
  for x in range(20,w-22,9):d.line((x,11,x,15),fill=p.rim)
  x=w//2;d.rectangle((x-14,9,x+14,18),outline='#f7da6c')
  d.line((x,8,x,17),fill=p.white)
 elif k=='defensive_wall_selector':
  well(d,(13,9,w-15,h-12),p)
  for x in range(23,w-27,27):
   d.ellipse((x,14,x+5,18),fill=p.white)
   d.rectangle((x-2,20,x+7,25),fill=p.rim)
 elif k=='trajectory_key':
  well(d,(13,9,w-15,h-12),p)
  for i in range(6):d.ellipse((19+i*8,11+i//3*4,21+i*8,13+i//3*4),fill=p.rim)
  d.line((69,11,69,h-14),fill=p.tint)
 else:
  well(d,(13,8,w-15,h-12),p)
  d.line((34,9,34,h-13),fill=p.rim)
  if k=='range_well':
   for xx in (19,22,25,28):d.line((xx,13,xx,17+(xx%2)),fill=p.white)
  elif k=='taker_well':
   d.ellipse((19,13,28,21),fill=p.shade,outline=p.white)
  else:
   d.line([(19,20),(22,13),(25,19),(28,15)],fill=p.white,width=2)
 return im

def run(a):
 groups=[
  ('score',scoreboard,'Scoreboards',
   ['center_duel','compact_corner','cup_aggregate','two_line','shootout_header','home_identity','away_identity','score_pair'],
   'Blank score module. Add independent club badge, text and score digits.'),
  ('time',clock,'Timers',
   ['match_wide','mini_clock','added_time','extra_period','countdown','hold_badge'],
   'Live clock or period casing, without digits.'),
  ('name',namebar,'Corner Player Name Bars',
   [s+'_'+k for s in ('left','right') for k in ('normal','compact','selected','sub')],
   'Attach at the named screen corner. Player number and name are independent overlays.'),
  ('meter',meter,'Fitness and Shot Power',
   [s+'_'+k for s in ('fitness','power') for k in ('0','20','40','70','100','fill','marker')],
   'Use the empty casing plus clipped fill and independent marker, or one of the example frames.'),
  ('pause',pause,'Pause Interface',
   ['title','body','menu_default','menu_selected','menu_disabled','quick_option','footer','divider'],
   'Blank pause-menu building block. Text is a separate layer.'),
  ('set',setpiece,'Set Piece Interface',
   ['command_ribbon','direction_panel','taker_well','range_well','beat_well','defensive_wall_selector','accuracy_track','trajectory_key'],
   'Set-piece input/readout shell; gameplay values are independent.')]
 for p in THEMES:
  for prefix,paint,category,keys,note in groups:
   for k in keys:
    im=paint(p,k)
    a.add('mh_'+prefix+'_'+p.key+'_'+k,im,category,family='core',note=note,margins=None)
