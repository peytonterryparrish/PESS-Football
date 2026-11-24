"""Modular chrome authored with the approved Matchup Proof's original pixel finish."""
from PIL import Image,ImageDraw
import numpy as np

D=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype=float)/16
colors=lambda *xs:[tuple(bytes.fromhex(x)) for x in xs]
PAL={
 'blue':colors('04063c','060ab0','111cec','2a37ff','88a3ff'),
 'red':colors('300817','660e27','a11d28','de503c','f4875b'),
 'green':colors('061f2a','034627','097036','1b9348','38b75f'),
 'gold':colors('2a220b','534014','8b651a','b99124','d0aa4d'),
 'purple':colors('070719','1c0d44','372185','473894','6354bd'),
 'ice':colors('021437','122564','3b62b5','6a9edc','aacbfe'),
 'aqua':colors('041526','073c52','056b72','11a398','60dfc4'),
 'muted':colors('111223','1c203b','292d54','3b4262','62718e'),
 'orange':colors('220d23','673525','aa5426','d97f33','fbb75c'),
 'pink':colors('160b2e','60204e','8f2b6c','b9498c','ea75b7'),
}
EDGE={'blue':'#5479f7','red':'#e37862','green':'#b7ddc5','gold':'#e9ba4f','purple':'#d2eee7','ice':'#c9d8e0','aqua':'#86ffe1','muted':'#69708d','orange':'#ffc37f','pink':'#f0a0d4'}
ROLES={'gk':'purple','df':'green','mf':'gold','fw':'red','res':'muted'}
def texture(w,h,pal,seed=0):
 y,x=np.indices((h,w));B=D[y%4,x%4]
 t=.15+.47*np.sin(np.pi*(y+1)/h)+.11*np.cos((x-y*1.5+seed)/12)+.12*np.sin((-x*.22+y+seed)/4)**2
 return Image.fromarray(np.array(PAL[pal],dtype='uint8')[np.clip(np.floor(t*(len(PAL[pal])-.5)+B*.8),0,len(PAL[pal])-1).astype(int)]).convert('RGBA')
def metal(w,h,pal='purple',rim=None,bevel=True,shadow=True,bead=False,seed=0):
 # Two exterior pixels each side, four for the original deep cast shadow.
 assert w>11 and h>9
 sw=4 if shadow else 0;sh=4 if shadow else 0;x=2 if shadow else 0;y=0
 bw=w-sw;bh=h-sh
 im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
 if shadow:d.rectangle((0,2,w-1,h-1),fill='#080920')
 im.alpha_composite(texture(bw,bh,pal,seed),(x,y))
 d.rectangle((x,y,x+bw-1,bh-1),outline=rim or EDGE[pal])
 if bevel:
  d.line((x+2,2,x+bw-2,2),fill='#d1f0e4')
  d.line((x+2,bh-2,x+bw-2,bh-2),fill='#119e97')
  d.line((x+1,bh-4,x+bw-2,bh-4),fill='#0c3346')
 if bead:
  for i in range(3):
   start=x+8+i*7;d.line((start,7,start+4,7),fill=EDGE[pal] if i else '#fffadc')
 return im
def blank(w,h,pal='purple',rim=None,frame=5):
 im=metal(w,h,pal,rim,True);d=ImageDraw.Draw(im);d.rectangle((frame+2,frame,w-frame-3,h-frame-5),fill=(0,0,0,0))
 return im
def number(w,h,pal='blue',focus=False):
 im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
 sl=min(15,max(5,w//3));pts=[(1,h-1),(sl+1,0),(w-1,0),(w-1,h-1)]
 d.polygon([(x,y+1 if y<h-1 else y) for x,y in pts],fill='#0b0b29')
 mask=Image.new('L',(w,h));ImageDraw.Draw(mask).polygon(pts,fill=255)
 im.paste(texture(w,h,'gold' if focus else pal),mask=mask)
 d.line(pts+[pts[0]],fill='#fcdf7a' if focus else EDGE[pal],width=1)
 d.line((sl+2,2,w-3,2),fill='#fff5ba' if focus else '#9ecaff',width=2)
 d.line((4,h-2,w-2,h-2),fill='#0a1538')
 return im
def role(w,h,pal):
 im=metal(w,h,'muted','#3a3d58',False,False);d=ImageDraw.Draw(im)
 d.rectangle((2,2,w-3,h-3),fill='#121223')
 d.line((w-2,3,w-2,h-4),fill=EDGE[pal])
 return im
def roster(w,h,pal,blend=None,focus=False,team='blue',seg=True):
 if not seg:return metal(w,h,pal,'#ffdd62' if focus else '#b7ddc5',False,True)
 im=Image.new('RGBA',(w,h));nw=min(43,max(28,w//5));rw=17
 im.alpha_composite(number(nw,h-3,team,focus),(0,1))
 im.alpha_composite(role(rw,h-3,pal),(nw,1))
 base=metal(w-rw-nw,h,pal,'#ffdd62' if focus else '#b7ddc5',False,True)
 if blend:
  bw=max(12,round(base.width*.38))
  base.alpha_composite(texture(bw,base.height-9,blend),(base.width-bw-5,2))
 ImageDraw.Draw(base).line((4,h-6,base.width-6,h-6),fill='#152435',width=2)
 im.alpha_composite(base,(nw+rw,0))
 return im
def score(w,h,pal):
 im=metal(w,h,pal,'#f3e3a3',False);d=ImageDraw.Draw(im)
 d.line((4,2,w-6,2),fill='#fff3bc')
 d.line((4,h-6,w-6,h-6),fill='#100e32')
 return im
def tab(w,h,pal,active=False):
 im=metal(w,h,pal,'#efce67' if active else EDGE[pal],True)
 if active:
  d=ImageDraw.Draw(im);d.rectangle((w//2-10,h-6,w//2+10,h-4),fill='#ffe182')
 return im
def grid(w,h,pal='purple',kind='stats'):
 im=metal(w,h,pal,EDGE[pal],True,True);d=ImageDraw.Draw(im)
 d.rectangle((10,9,w-13,30),fill='#151131')
 d.line((10,31,w-13,31),fill='#72d7cd')
 if kind=='stats':
  for y in range(54,h-14,21):d.line((10,y,w-13,y),fill='#292242')
  for x in (round(.73*w),round(.84*w)):d.line((x,32,x,h-12),fill='#594b8e')
 if kind=='comparison':
  for y in range(60,h-14,26):d.line((14,y,w-15,y),fill='#292242')
  x=w//2
  d.line((x,34,x,h-12),fill='#73d7ce')
 if kind=='card':
  dr=(12,39,w-13,round(h*.68));d.rectangle(dr,fill='#080f2c',outline=EDGE[pal])
  for y in range(round(h*.68)+12,h-15,17):d.line((15,y,w-17,y),fill='#6955a2')
 return im
def meter(w,h,pal,n=-1):
 im=metal(w,h,'ice','#a4c9ee',False,True,False)
 d=ImageDraw.Draw(im);d.rectangle((5,4,w-7,h-9),fill='#0b1539')
 if n>0:
  ww=max(1,round((w-13)*n/10))
  im.alpha_composite(texture(ww,h-13,pal),(6,5))
  d.line((6,5,6+ww-1,5),fill=EDGE[pal])
 return im
def led(w,h,pal,point=False,triplet=False):
 im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im);c=EDGE[pal]
 if point:
  d.polygon([(0,h//2),(w//3,1),(w-2,1),(w-1,h//2),(w-2,h-2),(w//3,h-2)],fill=PAL[pal][2],outline=c)
  d.line((w//3,2,w-3,2),fill='#fff4d2')
 elif triplet:
  for x in range(w//6,w,w//3):
   d.ellipse((x-3,h//2-3,x+3,h//2+3),fill=PAL[pal][2],outline=c)
   d.point((x-1,h//2-1),fill='#fff6d7')
 else:
  d.line((1,h//2,w-2,h//2),fill=c,width=3)
  d.line((w//9,h//2,w*8//9,h//2),fill='#fff7d1')
 return im

def run(a):
 def add(k,im,cat,note='',m=None):a.add('core_'+k,im,cat,family='core',note=note,margins=m)
 for k,p,w,h in [
  ('default','purple',124,43),('selected','gold',124,43),('pressed','aqua',124,43),('disabled','muted',124,43),
  ('continue','red',130,41),('continue_focus','red',130,41),
  ('long','purple',182,43),('wide','purple',246,43),('compact','purple',92,29),('square','purple',44,43),
  ('primary_wide','blue',205,47),('danger_wide','red',205,47),('utility','ice',112,37),
  ('mini','purple',63,28),('mini_focus','gold',63,28),('sort','aqua',75,29),('ghost','muted',113,34)]:
  add('menu_'+k,metal(w,h,p,'#ffe384' if k in ('continue_focus','selected','mini_focus') else None,True,True,k=='primary_wide'),'Buttons','Blank label area. Purple/turquoise finish from the approved matchup treatment.',[7,6,7,9])
 for k,p in [('home','blue'),('away','red'),('neutral','purple'),('highlight','gold'),('secondary','aqua')]:
  add('ribbon_'+k,metal(356,59,p,EDGE[p],False,True),'Banners','Wide team or section header.',[7,4,7,7])
  add('team_plate_'+k,metal(152,47,p,'#f6d568',False,True),'Nameplates','Gold-edged editable club or player label.',[7,4,7,7])
  add('flag_case_'+k,blank(77,56,p,'#7599d3',4),'Frames','Transparent opening for an independent badge or flag.',[8,6,8,10])
  add('score_well_'+k,score(50,52,p),'Number Wells','Editable score cell.',[7,6,7,9])
  add('score_well_compact_'+k,score(33,48,p),'Number Wells','Compact score or match minute well.',[6,6,6,9])
  add('nav_tab_'+k,tab(102,33,p,k=='highlight'),'Navigation','Blank tab.')
 for rk,p in ROLES.items():
  add('roster_'+rk,roster(190,43,p),'Roster Rows','Full row: separate painted number/position areas plus an unlettered name lane.')
  add('roster_'+rk+'_away',roster(190,43,p,team='red'),'Roster Rows','Full row with red sleeve number well.')
  add('roster_'+rk+'_focus',roster(190,43,p,focus=True),'Roster Rows','Highlighted gold number rim.')
  add('roster_'+rk+'_compact',roster(150,30,p),'Roster Rows','Compact full row.')
  add('roster_name_'+rk,roster(135,43,p,seg=False),'Nameplates','Independent nameplate cell.',[6,5,6,9])
  add('role_'+rk,role(18,39,p),'Position Wells','Casing for vertical position lettering.')
  add('role_wide_'+rk,role(41,29,p),'Position Wells','Wider casing for horizontal position lettering.')
 for k,p,t in [('df_to_mf','green','gold'),('mf_to_fw','gold','red'),('gk_to_df','purple','green'),('res_to_gk','muted','purple')]:
  add('roster_transition_'+k,roster(190,43,p,t),'Roster Rows','Split dither finish bridging team role colors.')
 for k,p,f in [('home','blue',False),('away','red',False),('focus','blue',True),('neutral','ice',False),('reserve','muted',False),('secondary','aqua',False)]:
  add('shirt_well_'+k,number(39,39,p,f),'Number Wells','Original diagonal squad/shirt number well.')
 for k,p,shape in [('standard','purple','stats'),('compare','ice','comparison'),('training','aqua','stats'),('injury','red','stats'),('scouting','gold','stats'),('small','purple','stats'),('player','ice','card'),('captain','gold','card'),('bench','muted','card'),('record','red','card')]:
  w,h=(230,124) if k=='small' else (267,224) if shape=='card' else (360,169)
  add('stat_panel_'+k,grid(w,h,p,shape),'Stat Panels','Decorative grid only; texts, values and portraits are separate layers.')
 for k,p,w,h in [
  ('mini','ice',116,67),('control','purple',221,157),('pitch','ice',228,166),('form','aqua',344,236),
  ('portrait','gold',161,209),('squad','blue',260,311),('away','red',260,311),('wide','purple',473,226),
  ('alert','red',305,128),('summary','gold',342,178)]:
  add('frame_'+k,blank(w,h,p,'#e2d6a9' if k in ('pitch','portrait') else None),'Frames','Transparent opening. Keep corners fixed when resizing.',[8,6,8,10])
 for k,p,w,h in [('plain','purple',324,184),('subtle','ice',234,120),('news','blue',426,132),('warning','red',264,104),('season','aqua',418,190),('award','gold',277,161),('dialogue','purple',370,125)]:
  add('solid_'+k,metal(w,h,p,None,True,True,True),'Panels','Opaque original pixel panel with room for independently positioned text.',[12,10,12,12])
 for k,p in [('empty','blue'),('pace','blue'),('power','red'),('skill','aqua'),('form','green'),('stamina','gold'),('fitness','orange')]:
  for n in ((0,) if k=='empty' else (2,5,8,10)):
   add('meter_'+k+('' if k=='empty' else '_'+str(n*10)),meter(129,19,p,n),'Progress Bars','Example fill ratio; combine empty casing with separate fill to drive a live value.')
  if k!='empty':
   add('meter_fill_'+k,texture(116,6,p),'Progress Bars','Separate dither fill strip to clip by the live value.')
 for k,p in [('home','blue'),('away','red'),('focus','gold'),('good','aqua'),('warning','orange'),('reserve','muted')]:
  add('rail_'+k,led(177,5,p),'Dividers','Glow line from the original proof treatment.')
  add('divider_'+k,led(87,5,p).transpose(Image.Transpose.ROTATE_90),'Dividers','Vertical highlight divider.')
  add('pointer_'+k,led(25,14,p,True),'Status Chrome','Tiny unlabeled arrow skin.')
  add('status_lights_'+k,led(45,17,p,triplet=True),'Status Chrome','Three metallic LEDs, label not baked in.')
 ff=blank(128,82,'purple')
 x,y=8,6;xr,yb=120,72
 for k,b in {'tl':(0,0,x,y),'tr':(xr,0,128,y),'bl':(0,yb,x,82),'br':(xr,yb,128,82),'t':(x,0,xr,y),'b':(x,yb,xr,82),'l':(0,y,x,yb),'r':(xr,y,128,yb)}.items():
  add('slice_'+k,ff.crop(b),'Frame Slices','Fixed corner or stretchable edge from the matching purple chrome frame.')
 for k,p in [('neutral','ice'),('team','blue'),('training','aqua'),('market','gold'),('settings','purple'),('alert','red')]:
  w,h=432,32
  im=metal(w,h,p,EDGE[p],True)
  d=ImageDraw.Draw(im);d.rectangle((9,6,w-12,h-11),fill='#13172d')
  for x in (98,215,329):d.line((x,8,x,h-13),fill=EDGE[p])
  add('footer_'+k,im,'Footer Rails','Four independent navigation label spaces and an original bevel.',[10,8,10,10])
 for k,p in [('age','aqua'),('wage','gold'),('loan','purple'),('fee','blue'),('contract','ice'),('pending','orange'),('debt','red'),('available','green')]:
  im=metal(86,25,p,EDGE[p],False)
  d=ImageDraw.Draw(im)
  d.rectangle((6,4,18,17),fill='#0c1529')
  d.point((10,7),fill='#fff5c3');d.point((11,8),fill=EDGE[p]);d.point((12,9),fill=EDGE[p])
  d.line((21,4,21,17),fill=EDGE[p])
  d.line((4,19,80,19),fill='#061735')
  add('chip_'+k,im,'Value Chips','Left slot for a separate icon; right zone for age, wages, fee, or contract values.')
 for k,p in [('team','blue'),('training','aqua'),('market','gold'),('settings','purple'),('warning','red')]:
  w,h=305,133
  im=metal(w,h,p,EDGE[p],True);d=ImageDraw.Draw(im)
  d.rectangle((12,36,w-16,h-15),fill='#0a102b',outline='#39436b')
  d.line((14,39,w-17,39),fill='#161b43');d.line((14,h-18,w-17,h-18),fill='#070a1b')
  add('mode_panel_'+k,im,'Panels','Compact heading area and dark reading surface for pixel text.')
