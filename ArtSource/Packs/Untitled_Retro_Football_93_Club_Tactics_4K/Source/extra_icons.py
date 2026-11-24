"""Original approved-palette modular pixel icon expansion: 50 motifs × 5 states."""
from PIL import Image, ImageDraw, ImageFilter
from math import sin,cos,pi
PALS={
 'blue':('#070921','#122564','#183da9','#788ff8','#e9f3ff'),
 'gold':('#100d1b','#534014','#8b651a','#d0aa4d','#fff598'),
 'red':('#100b20','#660e27','#a11d28','#de503c','#ffe7db'),
 'teal':('#081a31','#0c3346','#086e72','#119e97','#d1f0e4'),
 'core':('#080920','#1c0d44','#372185','#6354bd','#fffde5')
}
GROUPS=[
 ('Transfer negotiations','gold','incoming outgoing terms wage extension shortlist bid loan contract_signed'),
 ('Fixtures and form','blue','kickoff postponed streak clean_sheet derby home_leg away_leg weather'),
 ('Club infrastructure','teal','terrace academy floodlight gym turnstile clubhouse groundskeeper trophy_case'),
 ('Medical and fitness','red','physio scan ice_pack brace recovery hydration lifting cardio fatigue'),
 ('Staff and scouting','blue','manager assistant scout analyst referee youth lead_coach watchlist'),
 ('Management and atlas','core','finance ticket landmark zoom_in zoom_out compass layer recenter')
]
STATES={
 'available':('Ready or eligible.','retain'),
 'focused':('Current option or selection.','blue'),
 'attention':('Requires action or review.','gold'),
 'completed':('Confirmed or finished.','teal'),
 'disabled':('Locked, unavailable or inactive.','red'),
}
D=((0,8,2,10),(12,4,14,6),(3,11,1,9),(15,7,13,5))
def rgb(c): return tuple(bytes.fromhex(c[1:]))
def blend(a,b,f): return tuple(round(x+(y-x)*f) for x,y in zip(rgb(a),rgb(b)))
def stamp(n):
 a=Image.new('L',(24,24)); d=ImageDraw.Draw(a)
 def line(*xy,w=2):d.line(xy,fill=255,width=w,joint='curve')
 def box(x0,y0,x1,y1,solid=False):d.rectangle((x0,y0,x1,y1),fill=255 if solid else None,outline=255,width=2)
 def hole(x0,y0,x1,y1):d.rectangle((x0,y0,x1,y1),fill=0)
 def oval(x0,y0,x1,y1,solid=False):d.ellipse((x0,y0,x1,y1),fill=255 if solid else None,outline=255,width=2)
 def poly(*xy):d.polygon(xy,fill=255)
 def arrow(x,y,dx,dy,s=3):
  x1,y1=x+dx,y+dy
  line((x,y),(x1,y1))
  m=max(abs(dx),abs(dy)); u,v=dx/m,dy/m
  poly((x1,y1),(round(x1-s*u-s*v),round(y1-s*v+s*u)),(round(x1-s*u+s*v),round(y1-s*v-s*u)))
 def paper():
  box(5,3,18,21);box(8,7,15,8,True);box(8,11,15,12,True)
 def plus(x,y):line((x-3,y),(x+3,y));line((x,y-3),(x,y+3))
 def person(x,y):oval(x-3,y-5,x+3,y+1,True);poly((x-3,y+2),(x+3,y+2),(x+6,y+6),(x+6,y+8),(x-6,y+8),(x-6,y+6))
 def coin(x,y,r=5):
  oval(x-r,y-r,x+r,y+r);line((x,y-2),(x,y+2),w=1);line((x-2,y),(x+2,y),w=1)
 def star(x,y,r):
  pts=[(round(x+(r if i%2==0 else r*.42)*sin(pi*i/5)),round(y-(r if i%2==0 else r*.42)*cos(pi*i/5))) for i in range(10)]
  poly(*pts)
 def lens():
  oval(3,3,15,15);line((14,14),(21,21),w=3)
 def tick(x,y):line((x-4,y),(x-1,y+3),(x+5,y-3),w=2)
 if n=='incoming':
  line((3,12),(3,21),(21,21),(21,12));arrow(12,3,0,14,5)
 elif n=='outgoing':
  line((3,13),(3,21),(21,21),(21,13));arrow(12,17,0,-14,5)
 elif n=='terms':
  box(4,3,19,20);hole(5,4,9,5);line((7,8),(16,8),w=1);line((7,11),(16,11),w=1);line((7,14),(16,14),w=1);line((7,17),(12,17),w=1)
 elif n=='wage':
  box(2,6,21,18);coin(12,12,4);box(5,11,6,13,True);box(17,11,18,13,True)
 elif n=='extension':
  paper();hole(13,14,22,22);plus(18,18)
 elif n=='shortlist':
  box(3,4,16,21);line((6,8),(13,8));line((6,12),(13,12));hole(12,13,22,22);star(17,17,6)
 elif n=='bid':
  poly((4,5),(7,2),(16,11),(13,14));line((10,8),(20,18),w=3);line((3,20),(17,20),w=2)
 elif n=='loan':
  line((4,6),(4,11),(11,11),(11,16));arrow(13,17,8,0,4);arrow(18,6,-6,0,3);box(3,16,8,21)
 elif n=='contract_signed':
  paper();hole(12,13,22,22);tick(16,17)
 elif n=='kickoff':
  oval(3,3,20,20);poly((11,6),(16,10),(14,15),(8,15),(6,10));line((9,4),(11,6));line((16,10),(20,9));line((8,15),(6,18));line((14,15),(16,19))
 elif n=='postponed':
  oval(3,3,20,20);line((12,6),(12,11),(16,13));hole(14,14,22,22);line((15,15),(21,21));line((15,21),(21,15))
 elif n=='streak':
  poly((6,21),(3,15),(8,9),(9,13),(13,2),(17,8),(17,12),(20,10),(20,17),(16,21))
  d.polygon([(8,20),(7,17),(13,12),(13,16),(16,15),(16,19),(14,21)],fill=0)
 elif n=='clean_sheet':
  poly((4,4),(19,4),(19,14),(12,21),(4,14));d.polygon([(6,6),(17,6),(17,13),(12,18),(6,13)],fill=0);tick(11,11)
 elif n=='derby':
  poly((2,4),(10,4),(10,14),(6,19),(2,14));poly((13,4),(21,4),(21,14),(17,19),(13,14));hole(4,7,7,11);hole(15,7,19,9);line((10,16),(14,11),w=1)
 elif n=='home_leg':
  poly((2,11),(10,3),(18,11));box(5,10,15,19,True);hole(9,14,11,19);line((19,14),(19,21),w=2)
 elif n=='away_leg':
  line((2,6),(2,19),(13,19));box(3,8,9,16);arrow(10,12,11,0,5);line((14,20),(18,20),w=1)
 elif n=='weather':
  oval(3,7,12,15,True);oval(9,4,17,15,True);oval(14,8,21,15,True);box(4,12,20,15,True);line((7,17),(5,21));line((13,17),(11,21));line((19,17),(17,21))
 elif n=='terrace':
  line((2,5),(21,5),(21,21),(2,21),(2,5));line((5,17),(9,17),(9,13),(13,13),(13,9),(18,9),w=2)
 elif n=='academy':
  poly((2,9),(12,3),(21,9),(12,14));line((5,12),(5,17),(12,20),(18,17),(18,13));line((21,9),(21,17),w=1)
 elif n=='floodlight':
  box(3,3,20,11,True)
  for x in (5,9,13,17):hole(x,5,x+1,8)
  line((12,10),(12,21),w=3);line((7,21),(17,21))
 elif n=='gym':
  box(4,6,7,17,True);box(16,6,19,17,True);box(2,9,3,14,True);box(20,9,21,14,True);line((8,12),(15,12),w=3)
 elif n=='turnstile':
  oval(7,3,16,11);line((12,8),(12,21));line((12,10),(4,15));line((12,10),(20,15));line((3,21),(21,21))
 elif n=='clubhouse':
  poly((2,9),(12,3),(21,9));box(5,10,19,21);box(10,14,14,21);line((3,21),(21,21))
 elif n=='groundskeeper':
  poly((4,12),(16,12),(20,16),(20,19),(4,19));line((9,13),(16,5),(21,5));oval(4,17,9,22,True);oval(15,17,20,22,True)
 elif n=='trophy_case':
  box(3,3,20,21);poly((8,6),(16,6),(16,10),(14,13),(13,13),(13,16),(16,16),(16,18),(8,18),(8,16),(11,16),(11,13),(9,11));line((6,7),(7,9));line((18,7),(17,9))
 elif n=='physio':
  box(3,7,21,20);line((8,7),(8,3),(16,3),(16,7));plus(12,13)
 elif n=='scan':
  box(3,4,20,19);line((4,12),(7,12),(10,8),(13,16),(16,11),(19,11),w=1)
 elif n=='ice_pack':
  poly((5,5),(19,5),(21,9),(19,20),(5,20),(3,9));line((12,8),(12,17),w=2);line((8,10),(16,15),w=1);line((8,15),(16,10),w=1)
 elif n=='brace':
  poly((2,8),(8,2),(21,15),(15,21));line((6,7),(16,17),w=1);
  for x,y in [(9,10),(11,12),(13,14)]:hole(x,y,x+1,y)
 elif n=='recovery':
  d.arc((3,3,20,20),32,334,fill=255,width=2);poly((16,3),(21,5),(19,10));plus(12,12)
 elif n=='hydration':
  poly((12,2),(16,8),(20,13),(20,17),(16,21),(8,21),(4,17),(4,13),(8,8));hole(8,14,9,16);hole(9,17,11,18)
 elif n=='lifting':
  poly((4,17),(7,9),(10,7),(13,8),(13,11),(10,13),(10,15),(14,15),(16,12),(20,12),(22,16),(20,19),(17,21),(5,21))
  hole(8,17,11,17)
 elif n=='cardio':
  poly((2,7),(6,4),(9,4),(12,7),(15,4),(18,4),(21,7),(21,12),(12,21),(2,12));d.line([(3,12),(7,12),(9,9),(12,15),(14,11),(20,11)],fill=0,width=2)
 elif n=='fatigue':
  box(2,7,20,17);box(20,10,22,14,True);box(5,10,6,14,True)
 elif n=='manager':
  oval(8,2,16,10,True);poly((6,12),(10,10),(12,12),(14,10),(18,12),(20,20),(4,20));hole(11,13,13,18)
 elif n=='assistant':
  person(8,12);hole(13,13,21,21);line((18,15),(18,21));line((15,18),(21,18))
 elif n=='scout':
  oval(2,8,11,18);oval(13,8,22,18);line((10,11),(13,11));line((5,6),(9,6));line((15,6),(19,6))
 elif n=='analyst':
  box(3,4,20,17);line((8,20),(15,20));line((12,17),(12,20));line((5,13),(8,10),(11,12),(15,7),(18,9),w=1)
 elif n=='referee':
  poly((5,5),(9,3),(12,5),(15,3),(19,5),(22,10),(18,13),(17,10),(17,21),(7,21),(7,10),(6,13),(2,10));hole(9,6,10,20);hole(13,6,14,20)
 elif n=='youth':
  person(10,12);hole(14,4,22,13);star(18,8,5)
 elif n=='lead_coach':
  person(11,12);hole(14,13,23,22);poly((14,14),(18,17),(21,14),(21,17),(18,20),(14,17))
 elif n=='watchlist':
  oval(2,7,21,16);oval(9,8,14,15,True);line((5,19),(18,19),w=1)
 elif n=='finance':
  oval(6,4,18,8);oval(6,8,18,12);oval(6,12,18,16);oval(6,16,18,20);line((6,7),(6,17));line((18,7),(18,17))
 elif n=='ticket':
  box(3,7,20,17,True);hole(2,10,5,14);hole(18,10,22,14)
  for y in (9,12,15):hole(12,y,12,y+1)
 elif n=='wages':
  coin(9,12,6);coin(16,12,5);line((8,7),(8,17),w=1)
 elif n=='landmark':
  poly((3,8),(12,3),(20,8));line((4,9),(19,9));line((6,10),(6,17));line((12,10),(12,17));line((18,10),(18,17));line((3,19),(20,19),w=3)
 elif n=='zoom_in':
  lens();plus(9,9)
 elif n=='zoom_out':
  lens();line((6,9),(12,9),w=2)
 elif n=='compass':
  oval(2,2,21,21);poly((16,6),(13,14),(7,17),(10,9));hole(11,11,11,12)
 elif n=='layer':
  poly((12,3),(21,8),(12,13),(3,8));line((3,12),(12,17),(21,12),w=2);line((3,16),(12,21),(21,16),w=2)
 elif n=='recenter':
  oval(4,4,19,19);line((2,12),(7,12));line((16,12),(21,12));line((12,2),(12,7));line((12,16),(12,21));oval(10,10,13,13,True)
 else:raise ValueError(n)
 return a
def sprite(mask,fam,state):
 p=PALS[fam]
 out=Image.new('RGBA',(44,44));d=ImageDraw.Draw(out)
 shape=[(6,1),(37,1),(42,6),(42,36),(37,41),(6,41),(1,36),(1,6)]
 d.polygon([(x+1,y+1) for x,y in shape],fill='#040519')
 d.polygon(shape,fill=p[0] if state!='disabled' else '#141128',outline=p[3])
 for y in range(5,38):
  for x in range(5,39):
   if ((x<7 or x>36) and (y<7 or y>36)):continue
   a=(.68 if y<15 else .48 if y<25 else .29)+D[y%4][x%4]/87
   if state=='disabled':a*=.3
   out.putpixel((x,y),(*blend(p[0],p[2],a),255))
 d=ImageDraw.Draw(out); d.line((7,3,36,3),fill=p[4] if state!='disabled' else p[3]);d.line((5,38,38,38),fill=p[1],width=2)
 d.line((6,4,31,4),fill=p[3]); d.point((5,6),fill=p[4])
 shadow=Image.new('RGBA',(24,24),'#08091c');shadow.putalpha(mask)
 m=Image.new('RGBA',(24,24))
 for y in range(24):
  a=.55 if y<9 else .37 if y<17 else .15
  if state=='disabled':a*=.24
  ImageDraw.Draw(m).line((0,y,23,y),fill=blend(p[3],p[4],a+.33 if state!='disabled' else .04+a))
 m.putalpha(mask)
 out.alpha_composite(shadow,(11,10));out.alpha_composite(m,(10,9))
 d=ImageDraw.Draw(out); high=p[4]; dark=p[0]
 if state=='focused':
  for (x,y,sx,sy) in [(5,5,1,1),(38,5,-1,1),(5,36,1,-1),(38,36,-1,-1)]:
   d.line([(x+sx*5,y),(x,y),(x,y+sy*5)],fill='#ffe093',width=1)
 elif state=='attention':
  d.polygon([(37,1),(43,7),(37,13),(31,7)],fill=dark,outline='#ffca5e');d.line((37,4,37,7),fill='#fff5bd'); d.point((37,9),fill='#fff5bd')
 elif state=='completed':
  d.rectangle((32,30,42,40),fill=dark,outline='#87d8b5'); d.line([(34,35),(36,37),(40,33)],fill='#d1f0e4',width=2)
 elif state=='disabled':
  d.rectangle((32,30,42,40),fill='#0b0b20',outline='#9d677e'); d.line((34,35,40,35),fill='#a27489',width=2)
 return out
def run(a):
 for cat,default,names in GROUPS:
  for name in names.split():
   g=stamp(name)
   for state,(note,alt) in STATES.items():
    p=default if alt=='retain' else alt
    a.add(f'xico_{name}_{state}',sprite(g,p,state),cat,p,note)
