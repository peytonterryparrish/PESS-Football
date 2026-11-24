from pathlib import Path
import json,random,math
from PIL import Image,ImageDraw
import style as st
def club_shirt(slug, role):
 p=Path(__file__).resolve().parent.parent/'Tactics_Jerseys'/'PNGs'/slug/(role+'.png')
 with Image.open(p) as im:return im.convert('RGBA')
P=Path(__file__).resolve().parent;N=st.N
ROSTER=[('01','GK','S. Rowe',80),('02','RB','M. Doyle',75),('04','CB','R. Gill',77),('14','CB','T. Mercer',78),('03','LB','J. Rivers',84),('06','DM','K. Adams',76),('19','CM','S. Noble',78),('09','RW','L. Hall',82),('07','AM','T. Frost',81),('10','LW','D. Price',76),('11','ST','A. Levine',86)]
MARKET=[('J. Mercer','Redwick City','ST','84','12,500'),('L. Frost','Bellwood Rovers','LW','81','9,700'),('N. Clarke','Elmsford Town','CB','79','7,950'),('P. Sutton','Ironvale United','CM','77','6,800'),('H. Mills','Redwick City','GK','80','8,250'),('D. Vale','Kingswell County','RW','76','5,250'),('M. Ford','Crownshire Albion','RB','74','4,550'),('A. Hurst','Bellwood Rovers','CB','75','4,750')]
TABS=['SQUAD','TRANSFERS','ENGLAND','YOUTH ACADEMY']
RoleColor=lambda p:'purple' if p=='GK' else 'green' if p in ('CB','RB','LB','DF') else 'red' if p in ('ST','LW','RW','FW') else 'gold'
def crest(w,h,col,trim,var=0):
 p=Image.new('RGBA',(w,h));d=ImageDraw.Draw(p)
 m=w//2
 d.polygon([(0,1),(w-1,1),(w-2,int(h*.62)),(m,h-1),(2,int(h*.62))],fill=col,outline=trim,width=max(1,w//18))
 if var%2:
  d.polygon([(w*.21,h*.72),(w*.67,h*.15),(w*.81,h*.15),(w*.35,h*.86)],fill=trim)
 else:
  d.polygon([(m,h*.75),(w*.19,h*.42),(w*.19,h*.31),(m,h*.58),(w*.8,h*.31),(w*.8,h*.42)],fill=trim)
 return p
def jersey(seed,w=142,h=140):
 r=random.Random(seed);s=r.choice(['#a86748','#6d3c2b','#d99f78','#f2b98a','#b57e60'])
 ss=r.choice(['#261835','#0a1329','#4a2a23','#6f442f'])
 k=seed%5; c=['#3455ed','#dd4238','#176248','#7954b6','#c49a42'][k]; c2=['#142b8e','#761f3a','#064731','#3a275c','#866923'][k]; c3=['#77d5fd','#ff915e','#72d59b','#b48cd3','#f7df7a'][k]
 im=Image.new('RGBA',(100,98));d=ImageDraw.Draw(im)
 d.polygon([(41,10),(59,10),(68,16),(70,35),(67,45),(64,55),(64,65),(91,73),(99,96),(1,96),(9,73),(35,65),(37,54),(30,41),(30,28),(34,18)],fill='#100e26')
 d.polygon([(39,28),(60,25),(67,32),(65,50),(60,59),(59,69),(41,69),(40,57),(34,47),(33,33)],fill=s)
 d.polygon([(49,28),(62,26),(67,34),(63,49),(57,58),(46,57),(56,48),(56,33)],fill='#632e35')
 d.polygon([(34,19),(43,11),(59,11),(70,23),(67,33),(59,28),(46,31),(34,37),(30,32)],fill=ss)
 for j in range(4):d.line((36+j*4,20+(j%2)*3,53+j*3,16+(j%2)*2),fill='#564360')
 d.line((39,39,45,38),fill='#251624',width=2);d.line((56,39,60,39),fill='#251624',width=2)
 d.line((49,43,47,48),fill='#efaa84',width=2);d.line((45,52,54,52),fill='#47252d')
 d.polygon([(9,74),(38,65),(51,76),(63,65),(90,74),(98,97),(2,97)],fill=c)
 d.polygon([(63,65),(89,74),(98,97),(75,97),(65,82)],fill=c2)
 d.line((37,68,50,78,63,68),fill=c3,width=3)
 if k%2:
  d.polygon([(19,79),(31,74),(76,97),(51,97)],fill=c3)
 else:
  for x in [26,47,67]:d.polygon([(x,80),(x+7,80),(x+10,97),(x+1,97)],fill=c3)
 for x in range(16,87,4):d.point((x,90+(x%3)),fill=c2)
 return im.resize((w,h),N)
class Screen:
 def __init__(self,kit,title,tab,sub):
  self.k=kit;self.i=st.backdrop(1280,720).copy()
  self.p(76,39,1128,51,'blue','gold',False)
  self.p(91,46,160,37,'red','gold',False)
  self.t(title,267,64,32,sty='slant',left=True)
  self.t(sub,1184,64,18,'#fbe494',right=True)
  for j,t in enumerate(TABS):
   self.p(91+j*276,103,267,39,'red' if t==tab else 'purple','gold' if t==tab else 'teal')
   self.t(t,224+j*276,119,21,mid=True)
  self.p(76,660,1128,34,'ice','ice',False)
  self.t('A   SELECT       B   BACK       L/R   SWITCH',92,676,16,'#e5ffff',left=True)
 def p(self,x,y,w,h,pal='purple',edge='teal',lip=True):
  self.i.alpha_composite(st.panel(w,h,pal,edge,lip),(x,y))
 def rect(self,b,c,outline=None):ImageDraw.Draw(self.i).rectangle(b,fill=c,outline=outline)
 def t(self,text,x,y,size=21,c='#fffde9',sty='sans',mid=False,left=False,right=False,maxw=None):
  a=st.label(str(text),size,c,sty,1,maxw)
  self.i.alpha_composite(a,(x-a.width//2 if mid else x-a.width if right else x,y-a.height//2))
 def place(self,img,x,y,w=None,h=None):
  if not img:return
  if w is not None:
   h=round(w*img.height/img.width) if h is None else h
   img=img.resize((w,h),N)
  self.i.alpha_composite(img,(x,y))
 def asset(self,id,x,y,w=None,h=None,default=None):
  self.place(self.k.im(id,default),x,y,w,h)
 def box(self,xy,title,edge='teal',head='purple'):
  x,y,w,h=xy;self.p(x,y,w,h,'ice',edge,False)
  self.rect((x+4,y+38,x+w-5,y+h-5),'#10163b')
  self.p(x+3,y+3,w-6,34,head,edge)
  self.t(title,x+13,y+17,19)
 def bar(self,label,score,x,y,w):
  self.t(label,x,y,16,'#d5eef5',left=True)
  self.rect((x+103,y-8,x+w-43,y+8),'#090f2a','#475d99')
  step=(w-149)/12
  for i in range(12):
   start=round(x+106+i*step);end=round(x+104+(i+1)*step)
   self.rect((start,y-5,end,y+5),('#27c79d' if score>=80 else '#eeb545') if i<round(score*.12) else '#202e58')
  self.t(score,x+w-2,y,19,'#ffdf76','slant',right=True)
 def btn(self,text,x,y,w,active=False):
  self.p(x,y,w,34,'red' if active else 'purple','gold' if active else 'teal')
  self.t(text,x+w//2,y+15,17,mid=True)
 def row(self,r,x,y,w,selected=False):
  n,role,name,ovr=r
  self.p(x,y,w,29,RoleColor(role),'gold' if selected else 'teal',False)
  self.p(x+1,y+1,37,28,'blue','ice',False)
  self.t(n,x+18,y+15,19,'#ffe05a','slant',True)
  self.t(role,x+62,y+15,14,'#c6ffe6',mid=True)
  self.t(name,x+83,y+15,19,sty='mono',maxw=w-122)
  self.t(ovr,x+w-13,y+15,18,'#ffe05a','slant',right=True)
 def write(self,path):self.i.convert('RGB').resize((3840,2160),N).save(path,compress_level=7)
def markers(s,x,y,w,h,roles=ROSTER,shape=(4,2,3,1),club='arsenal'):
 rows=[[roles[0]]];j=1
 for n in shape: rows.append(roles[j:j+n]);j+=n
 pts=[]
 for i,row in enumerate(rows):
  xx=x+round(w*(.10+i*.195))
  for j,a in enumerate(row):
   yy=y+round(h*(.5 if len(row)==1 else .15+j*.7/(len(row)-1)))
   pts.append((xx,yy,a))
 for xx,yy,r in pts:
  keeper=r[1]=='GK'
  ww,hh=(42,37) if w>600 else (36,32)
  s.place(club_shirt(club,'keeper1' if keeper else 'home'),xx-ww//2,yy-hh//2,ww,hh)
  s.t(str(int(r[0])),xx,yy+1,11,c='#fff6d2',mid=True)
  s.t(r[1],xx,yy+hh//2+7,12,c='#fff5c4',mid=True)
def squad(k,tactics=False,club='arsenal',club_name='ARSENAL'):
 s=Screen(k,'TACTICS BOARD' if tactics else 'TEAM MANAGEMENT','SQUAD',club_name+'  /  TACTICS PREVIEW')
 if not tactics:
  s.box((88,162,354,478),'STARTING XI')
  for j,r in enumerate(ROSTER):s.row(r,99,207+j*36,331,j==4)
  s.btn('BENCH  /  RESERVES',100,609,330)
  s.box((454,162,485,478),'FORMATION   /   4-2-3-1')
  s.place(st.field(453,315),470,214)
  markers(s,470,214,453,315,roles=[ROSTER[i] for i in (0,1,2,3,4,5,6,9,8,7,10)],club=club)
  s.btn('FORMATION',470,546,137,True);s.btn('SET PIECES',623,546,137);s.btn('ROLES',776,546,147)
  s.t('FITNESS  93%      TEAM RATING  80',476,608,17,'#a5f4d4')
  s.box((950,162,244,478),'PLAYER INFO','gold','red')
  s.place(jersey(3),1005,216,140,139)
  s.t('J. RIVERS',1072,381,27,mid=True)
  s.t('LEFT BACK  /  84',1072,413,18,'#ffe275',mid=True)
  for j,(lab,q) in enumerate([('PACE',86),('DEF',82),('PASS',80),('FIT',94)]):s.bar(lab,q,966,454+j*30,208)
  s.btn('SWAP PLAYER',957,594,229)
 else:
  s.box((88,162,770,478),'TRAINING GROUND  /  STARTING SHAPE')
  s.place(st.field(734,384),105,212)
  markers(s,105,212,734,384,roles=[ROSTER[i] for i in (0,1,2,3,4,5,6,8,9,10,7)],shape=(4,3,3),club=club)
  s.box((870,162,324,478),'TACTIC PRESETS')
  for j,(lab,val) in enumerate([('FORMATION','4-3-3'),('PRESSING','HIGH'),('DEFENSIVE LINE','MEDIUM'),('BUILD UP','DIRECT'),('ATTACK WIDTH','WIDE'),('SET PIECES','CUSTOM')]):
   s.p(884,216+j*55,294,43,'red' if j==0 else 'purple','gold' if j==0 else 'teal')
   s.t(lab,896,235+j*55,17,maxw=153);s.t(val,1168,235+j*55,17,'#fff7a5',right=True)
  s.btn('SAVE TACTIC',884,575,294,True)
 return s
def market(k,detail=False):
 s=Screen(k,'SCOUT REPORT' if detail else 'TRANSFER MARKET','TRANSFERS','WINDOW OPEN  /  16 DAYS')
 if not detail:
  s.box((88,162,222,476),'SEARCH FILTERS')
  for j,(a,v) in enumerate([('POSITION','ANY'),('LEAGUE','ALL'),('PRICE','UNDER 15K'),('AGE','18—27'),('SORT BY','RATING')]):
   s.t(a,102,217+j*69,16,'#f3d068');s.btn(v,101,235+j*69,195,j==0)
  s.btn('RESET FILTERS',101,586,195)
  s.box((321,162,595,476),'RESULTS  /  SCOUTING LIST')
  s.t('PLAYER / CLUB',335,215,16,'#e5ffc3');s.t('POS   OVR      ASKING',898,215,16,'#e5ffc3',right=True)
  for j,(nm,cl,r,ov,p) in enumerate(MARKET):
   y=232+j*45;s.p(331,y,575,41,'red' if j==0 else 'ice','gold' if j==0 else 'teal',False)
   s.t(nm,344,y+13,19);s.t(cl,344,y+29,14,'#9be9ea');s.t(r,735,y+20,17,mid=True)
   s.t(ov,782,y+20,20,'#f9dc57','slant',mid=True);s.t(p,895,y+20,20,'#ffea93',right=True)
  s.box((926,162,268,476),'PLAYER PREVIEW','gold','red')
  s.place(jersey(1),978,219,163,165)
  s.t('J. MERCER',1059,414,25,mid=True)
  s.t('REDWICK CITY',1059,451,17,'#f2efce',mid=True)
  s.t('23 YEARS  /  ST  /  OVR 84',1059,476,15,'#9cf7eb',mid=True)
  s.btn('VIEW PLAYER',940,525,241,True);s.btn('ADD TO WATCHLIST',940,571,241)
 else:
  s.box((88,162,354,476),'J. MERCER   /   #09','gold','red')
  s.place(jersey(1),170,222,191,192)
  s.t('J. MERCER',265,456,30,mid=True)
  s.t('REDWICK CITY    /    ENGLAND',265,490,16,'#9cf7eb',mid=True)
  s.t('AGE 23    /    RIGHT FOOT',265,523,16,mid=True)
  s.btn('WATCHLIST',107,572,316)
  s.box((454,162,382,476),'SCOUTING  /  ATTRIBUTES')
  for j,(a,v) in enumerate([('PACE',87),('SHOOT',86),('PASS',74),('DRIBBLE',85),('DEFEND',44),('PHYSICAL',80),('FITNESS',94)]):s.bar(a,v,470,236+j*50,344)
  s.box((847,162,347,476),'OFFER DESK','gold')
  for j,(a,v) in enumerate([('ASKING FEE','12,500'),('WEEKLY WAGE','1,050'),('CONTRACT','2 YEARS'),('STATUS','AVAILABLE')]):
   s.p(864,218+j*64,314,45,'ice','teal',False);s.t(a,875,240+j*64,16);s.t(v,1166,240+j*64,17,'#ffde82',right=True)
  s.btn('MAKE OFFER',864,506,312,True);s.btn('REQUEST LOAN',864,555,312)
 return s
def atlas(k):
 s=Screen(k,'ENGLAND CLUB ATLAS','ENGLAND','ENGLAND ONLY  /  SCOUT NETWORK')
 s.box((88,162,232,476),'REGION SELECT')
 for j,n in enumerate(['ALL ENGLAND','NORTH EAST','NORTH WEST','YORKSHIRE','MIDLANDS','EAST','LONDON','SOUTH EAST','SOUTH WEST']):
  s.btn(n,99,207+j*43,210,j==0)
 s.box((331,162,566,476),'ENGLAND   /   CLUB CITIES')
 mob=k.im('map_england_flat')
 x,y,w,h=393,204,430,419
 if mob:
  h=420;w=round(mob.width*h/mob.height);x=614-w//2;s.place(mob,x,y,w,h)
 pin=k.im('map_pin_active')
 points=[]
 for f in P.glob('*Anchors*.json'):
  doc=json.loads(f.read_text()); data=doc.get('cities',[]) if isinstance(doc,dict) else doc
  if not isinstance(data,list):continue
  for c in data:
   xx,yy=c.get('flat_px',c.get('flat',c.get('pixel',[None,None]))) if isinstance(c,dict) else (None,None)
   if isinstance(xx,(float,int)) and isinstance(yy,(float,int)):points.append((c.get('name',''),xx,yy))
 if not points and mob:points=[('North East',.73*mob.width,.25*mob.height),('North West',.42*mob.width,.40*mob.height),('Midlands',.58*mob.width,.58*mob.height),('London',.76*mob.width,.78*mob.height),('South Coast',.62*mob.width,.89*mob.height),('South West',.30*mob.width,.87*mob.height)]
 for j,(name,xx,yy) in enumerate(points):
  if mob:
   u=x+round(xx*w/mob.width);v=y+round(yy*h/mob.height)
   a=k.im('map_pin_active' if j==1 else 'map_pin_idle')
   if a:s.place(a,u-8,v-11,17,21)
   else:s.rect((u-3,v-3,u+3,v+3),'#ffda55' if j==1 else '#90ebff')
 s.box((908,162,286,476),'ENGLAND  /  REGION')
 orb=k.im('map_england_orb')
 if orb:s.place(orb,964,203,175,175)
 for j,(lab,c) in enumerate([('CLUB CITY','#83dbff'),('SELECTED','#ffd650'),('SCOUT POST','#89ffcf')]):
  s.rect((932,392+j*35,944,404+j*35),c);s.t(lab,960,399+j*35,17)
 s.t('50 CITY ANCHORS',930,514,17,'#f6ce75');s.btn('BROWSE CLUBS',920,562,260,True)
 s.btn('RECENTER ATLAS',920,604,260)
 return s
def setup(k,select=True):
 s=Screen(k,'CLUB SELECT' if select else 'PLAYER ATTRIBUTES','SQUAD','EXHIBITION  /  CLUB DATA')
 if select:
  s.box((88,162,289,476),'HOME SIDE','ice','blue');s.box((905,162,289,476),'AWAY SIDE','red','red')
  for i,lab in enumerate(['HARBOUR ATHLETIC','REDWICK CITY','BELLWOOD ROVERS','CROWNSHIRE ALBION','ELMSFORD TOWN','IRONVALE UNITED','KINGSWELL COUNTY']):
   s.btn(lab,100,215+i*52,265,i==0);s.btn(lab,916,215+i*52,265,i==1)
  s.box((389,162,504,476),'TEAM COMPARISON','gold')
  s.place(crest(101,112,'#313cef','#ffd860'),455,226)
  s.place(crest(101,112,'#e44942','#ffe2b4',1),728,226)
  if k.im('type_vs_classic'):s.asset('type_vs_classic',580,259,116,64)
  else:s.t('VS',641,280,49,'#ffcf60','slant',mid=True)
  for i,(a,v) in enumerate([('ATTACK',86),('MIDFIELD',79),('DEFENCE',82),('RATING',84)]):s.bar(a,v,425,373+i*40,436)
  s.btn('CONFIRM CLUBS',423,571,435,True)
 else:
  s.box((88,162,320,476),'FIRST TEAM')
  for j,r in enumerate(ROSTER):s.row(r,101,209+j*35,291,j==4)
  s.box((419,162,376,476),'J. RIVERS   /   84 OVR','gold','red')
  s.place(jersey(3),512,216,182,181);s.t('LEFT BACK',607,438,27,'#fff3b9',mid=True)
  s.t('HARBOUR ATHLETIC',607,478,21,'#a3f6e3',mid=True);s.btn('COMPARE PLAYER',452,550,309)
  s.box((807,162,387,476),'ATTRIBUTE BARS')
  for j,(a,v) in enumerate([('SPEED',86),('TACKLE',82),('PASS',80),('CURVE',75),('STAMINA',89),('CROSS',84),('JUMP',77)]):s.bar(a,v,824,235+j*50,344)
 return s
def academy(k):
 s=Screen(k,'YOUTH ACADEMY','YOUTH ACADEMY','HARBOUR ATHLETIC  /  U18 DEVELOPMENT')
 s.box((88,162,566,474),'YOUTH SQUAD  /  PROSPECTS')
 prospects=[('E. Carter','CM',17,64,'79–85','READY'),('N. Roberts','CB',17,61,'76–83','DEVELOPING'),('A. James','ST',16,59,'77–87','DEVELOPING'),('S. Rahman','LB',16,57,'73–81','ASSESSING'),('L. Ward','GK',17,62,'74–82','DEVELOPING'),('T. Ellis','RW',15,54,'71–81','ASSESSING'),('I. Moore','DM',16,56,'72–80','ASSESSING')]
 for x,l in [(109,'PROSPECT'),(266,'POS'),(328,'AGE'),(380,'OVR'),(432,'POT.'),(523,'STATUS')]:s.t(l,x,219,16,'#a9efde')
 for j,(name,pos,age,ovr,pot,status) in enumerate(prospects):
  y=243+j*46;s.p(101,y,540,38,'red' if j==0 else 'ice','gold' if j==0 else 'teal',False)
  s.t(name,112,y+19,19,maxw=143);s.t(pos,272,y+19,17,mid=True)
  s.t(age,338,y+19,17,mid=True);s.t(ovr,389,y+19,19,'#ffe67c','slant',True)
  s.t(pot,455,y+19,17,'#d2ffee',mid=True);s.t(status,525,y+19,14,'#ffeeb3',maxw=104)
 s.btn('SORT: POTENTIAL',103,583,257,True);s.btn('SCOUTING REPORTS',374,583,262)
 s.box((667,162,527,474),'DEVELOPMENT REPORT  /  E. CARTER','gold','red')
 s.place(jersey(5),695,216,113,112)
 s.t('CENTRAL MIDFIELDER',837,233,22,'#fff5d2')
 s.t('17 YEARS    /    64 OVR',837,267,18,'#b7f2eb')
 s.t('SCOUT POTENTIAL  79–85',837,301,17,'#ffdb76')
 s.p(685,341,491,41,'purple','teal');s.t('TRAINING FOCUS   /   PASSING',699,360,19)
 for j,(a,v) in enumerate([('PASSING',71),('CONTROL',65),('STAMINA',72)]):s.bar(a,v,696,414+j*38,461)
 s.btn('CHANGE TRAINING',684,546,245)
 s.btn('PROMOTE TO FIRST TEAM',684,588,494,True)
 return s
def run(k,path):
 for i,(name,fn) in enumerate([('Squad_Management',lambda:squad(k)),('Tactics_Board',lambda:squad(k,1,'newcastle-united','NEWCASTLE UNITED')),('Transfer_Market',lambda:market(k)),('Player_Scout_Report',lambda:market(k,1)),('England_Club_Atlas',lambda:atlas(k)),('Club_Select',lambda:setup(k)),('Player_Attributes',lambda:setup(k,0)),('Youth_Academy',lambda:academy(k))],1):
  s=fn()
  s.write(path/f'{i:02}_{name}.png')
