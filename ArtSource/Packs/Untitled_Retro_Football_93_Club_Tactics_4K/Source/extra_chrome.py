"""250 original, unlettered, management-focused components in the approved arcade finish."""
from pathlib import Path
from collections import namedtuple
from PIL import Image, ImageDraw
import style as st

Mode=namedtuple('Mode','key pal edge shine mid low widths layout')
MODES=[
 Mode('negotiation','purple','teal','#e8fff1','#36caba','#12474e',(405,243,185),'offers'),
 Mode('finance','gold','gold','#fff6c2','#e4b94b','#6b481e',(440,286,162),'ledger'),
 Mode('season_calendar','blue','ice','#e0efff','#7496ed','#1c3872',(448,265,173),'calendar'),
 Mode('standings','ice','ice','#e0efff','#7496ed','#1c3872',(465,301,151),'table'),
 Mode('objectives','teal','teal','#e8fff1','#36caba','#12474e',(386,218,159),'checklist'),
 Mode('transfers','red','red','#ffddc9','#f57376','#5c203e',(432,258,171),'market'),
 Mode('scouting','green','gold','#fff6c2','#e4b94b','#6b481e',(419,242,154),'report'),
 Mode('loans','metal','purple','#eee2fb','#aa89e2','#38265f',(375,223,167),'contract'),
 Mode('renewals','stone','gold','#fff6c2','#e4b94b','#6b481e',(397,236,180),'extension'),
 Mode('inbox','dark','ice','#e0efff','#7496ed','#1c3872',(409,271,165),'inbox'),
]

def deco(d,x,y,m,vertical=False):
 # Discrete glint shapes differ by system and never include any lettering.
 n=MODES.index(m)
 blocks=[(0,2),(3,1),(6,0),(9,1),(12,2)]
 if n%2:blocks=[(0,0),(4,1),(8,2),(12,3)]
 for j,(a,b) in enumerate(blocks):
  if vertical:a,b=b,a
  d.rectangle((x+a,y+b,x+a+1,y+b+1),fill=m.shine if j==n%len(blocks) else m.mid)

def shell(w,h,m,kind='normal',notched=False,lip=True):
 states={'focus':('gold','gold'),'disabled':('stone','purple'),
         'armed':('red','red'),'confirmed':('green','teal'),
         'pressed':(m.pal,'purple')}
 pal,edge=states.get(kind,(m.pal,m.edge))
 return st.panel(w,h,pal,edge,lip,notched)

def deck(m,height=166,ptype='main'):
 w=m.widths[0] if ptype in ('main','overlay') else m.widths[1]
 h=height
 im=shell(w,h,m,notched=ptype=='overlay')
 d=ImageDraw.Draw(im)
 x1,y1,x2,y2=10,39,w-12,h-15
 d.rectangle((x1,y1,x2,y2), fill='#090d2b', outline=m.low)
 d.line((x1+1,y1+1,x2-1,y1+1),fill='#1f2a54')
 deco(d,14,14,m)
 if ptype=='overlay':
  d.rectangle((w-30,10,w-11,29),fill='#191735',outline=m.mid)
  d.line((w-24,15,w-17,22),fill=m.shine);d.line((w-24,22,w-17,15),fill=m.shine)
 if ptype!='main':return im
 if m.layout=='calendar':
  for ix in range(1,7):
   xx=x1+(x2-x1)*ix//7;d.line((xx,y1+10,xx,y2-2),fill=m.low)
  for iy in range(1,4):
   yy=y1+(y2-y1)*iy//4;d.line((x1+1,yy,x2-1,yy),fill=m.low)
 elif m.layout in ('table','ledger','market'):
  xfrac={'table':(.11,.54,.68,.82,.91),'ledger':(.15,.67,.86),'market':(.12,.49,.69,.86)}[m.layout]
  for i in range(5):
   yy=y1+21+i*(y2-y1-21)//5
   d.line((x1+4,yy,x2-4,yy),fill=m.low)
  for f in xfrac:
   if f<=0 or f>=1:continue
   xx=x1+round((x2-x1)*f);d.line((xx,y1+2,xx,y2-3),fill=m.low)
 elif m.layout in ('offers','contract','extension'):
  for i in range(3):
   yy=y1+12+i*33
   d.rounded_rectangle((x1+11,yy,x2-10,yy+24),radius=2,fill='#0d1738',outline=m.low)
   d.rectangle((x2-89,yy+4,x2-15,yy+20),fill=m.low)
 elif m.layout in ('checklist','inbox','report'):
  for i in range(4):
   yy=y1+10+i*24
   d.rectangle((x1+8,yy,x1+19,yy+11),fill='#101939',outline=m.mid)
   d.line((x1+26,yy+17,x2-9,yy+17),fill=m.low)
 return im

def column_frame(m,mode):
 w,h=(m.widths[1],114 if mode=='inspector' else 102)
 im=shell(w,h,m,notched=mode=='inspector')
 d=ImageDraw.Draw(im)
 if mode=='inspector':
  d.rectangle((11,12,62,h-14),fill='#0a112b',outline=m.mid)
  for k in (30,51,72):d.line((73,k,w-13,k),fill=m.low)
 elif mode=='history':
  d.line((25,17,25,h-17),fill=m.shine)
  for k in (22,42,62,82):
   d.rectangle((21,k-4,29,k+4),fill='#0b1736',outline=m.mid)
   d.line((39,k+8,w-14,k+8),fill=m.low)
 elif mode=='totals':
  d.rectangle((11,17,w-13,h-16),fill='#09102e',outline=m.low)
  d.line((w*2//3,19,w*2//3,h-17),fill=m.mid)
  for k in (40,61):d.line((12,k,w-14,k),fill=m.low)
 return im

def header(m,w,h,kind):
 im=shell(w,h,m,'focus' if kind in ('highlight','tab_selected') else 'normal',notched=kind=='title')
 d=ImageDraw.Draw(im)
 if kind=='title':
  d.polygon([(12,8),(45,8),(32,h-15),(12,h-15)],fill=m.low,outline=m.mid)
  d.line((15,11,38,11),fill=m.shine)
  deco(d,16,18,m)
 elif kind=='highlight':
  d.line((12,7,57,7),fill=m.shine,width=2)
 elif kind in ('tab_default','tab_selected'):
  d.line((10,7,w-12,7),fill=m.mid)
  if kind=='tab_selected':
   d.rectangle((w//2-8,h-8,w//2+8,h-4),fill='#fff4bd')
   i=MODES.index(m);x=12+i*8;d.line((x,7,x+4,7),fill=m.shine,width=2)
 elif kind=='bar':
  d.line((w//3,7,w//3,h-12),fill=m.mid)
  d.line((w*2//3,7,w*2//3,h-12),fill=m.mid)
 return im

def row(m,variant):
 w,h=m.widths[0],30
 im=shell(w,h,m,'focus' if variant=='focus' else 'disabled' if variant=='disabled' else 'normal',lip=False)
 d=ImageDraw.Draw(im)
 if variant!='focus':
  d.rectangle((7,7,w-8,h-8),fill='#0d1536' if variant=='default' else '#1a203c')
 else:
  d.rectangle((7,7,w-8,h-8),fill='#39304a')
  d.polygon([(10,9),(17,14),(10,19)],fill=m.shine)
 for x in (w//10,w//2,w*3//4,w*9//10):
  d.line((x,6,x,h-7),fill=m.low if variant!='focus' else m.mid)
 return im

def control(m,kind):
 w,h=m.widths[2],34
 if kind=='filter':w=111
 if kind.startswith('stepper'):w=136
 if kind=='amount':w,h=138,36
 if kind=='status':w,h=105,28
 if kind=='date':w,h=112,28
 if kind=='checkbox':w,h=28,28
 if kind=='toggle':w,h=53,25
 im=shell(w,h,m,'focus' if kind=='action_focus' else 'pressed' if kind=='action_pressed' else 'disabled' if kind=='action_disabled' else 'normal')
 d=ImageDraw.Draw(im)
 if kind.startswith('action'):
  i=MODES.index(m);d.line((10+i*6,7,13+i*6,7),fill=m.shine if kind!='action_disabled' else m.low,width=2)
  if kind=='action_focus':d.rectangle((8,h-11,w-9,h-8),fill='#ffeda3')
  if kind=='action_pressed':d.line((10,h-11,w-11,h-11),fill='#171634',width=2)
  if kind=='action_disabled':d.line((9,10,9,h-13),fill='#343957',width=2)
 if kind=='filter':
  d.rectangle((w-27,6,w-7,h-12),fill='#101533',outline=m.mid)
  d.polygon([(w-23,11),(w-12,11),(w-18,18)],fill=m.shine)
 if kind.startswith('stepper'):
  d.rectangle((27,7,w-28,h-12),fill='#101533',outline=m.low)
  sz=5; positions=(16,w-17)
  for j,xx in enumerate(positions):
   d.line((xx-sz,14,xx+sz,14),fill=m.shine,width=2)
   if j:d.line((xx,9,xx,19),fill=m.shine,width=2)
 if kind in ('amount','date','status'):
  d.rectangle((8,7,w-8,h-11),fill='#0c1430',outline=m.mid)
  d.rectangle((10,8,25,h-12),fill=m.low)
  if kind=='date':
   d.rectangle((13,11,21,16),fill='#101637',outline=m.shine)
   for i in (15,19):d.line((i,9,i,11),fill=m.shine)
  elif kind=='status':
   d.ellipse((14,10,21,17),fill=m.mid,outline=m.shine)
  elif kind=='amount':
   for dy in (12,15,18):d.line((13,dy,21,dy),fill=m.shine)
 if kind=='checkbox':
  d.rectangle((7,6,w-8,h-10),fill='#0c1430',outline=m.mid)
 if kind=='toggle':
  d.rounded_rectangle((8,7,w-9,h-10),radius=4,fill=m.low)
  d.rectangle((10,7,20,h-10),fill=m.mid,outline=m.shine)
 return im

def empty_frame(m):
 w,h=m.widths[1],138
 im=shell(w,h,m,notched=True)
 d=ImageDraw.Draw(im)
 d.rectangle((11,11,w-12,h-13),fill=(0,0,0,0))
 return im

def run(a):
 def add(m,key,im,cat,note,margins=None):
  a.add('xch_'+m.key+'_'+key,im,cat,family='core',note=note,margins=margins)
 for m in MODES:
  add(m,'main_board',deck(m,172),'Management Panels','Mode-specific calendar, table, form or checklist geometry. Text and figures separate.')
  add(m,'dialog_shell',deck(m,154,'overlay'),'Dialogs','Blank modal with close glyph; title and content separate.')
  add(m,'blank_panel',deck(m,118,'small'),'Management Panels','Compact dark reading area and bright heading strip.')
  for variant in ('inspector','history','totals'):
   add(m,variant+'_card',column_frame(m,variant),'Management Detail Panels','Empty '+variant+' panel designed to hold independent data layers.')
  add(m,'content_frame',empty_frame(m),'Frames','Transparent, notched frame with live interior.',[12,12,12,14])
  for variant,w,h in [('title',m.widths[0],41),('highlight',m.widths[1],33),('tab_default',118,32),('tab_selected',118,32),('bar',m.widths[0],32)]:
   add(m,variant+'_ribbon',header(m,w,h,variant),'Navigation and Banners','Unlettered '+variant.replace('_',' ')+' asset for management navigation.')
  for variant in ('default','focus','disabled'):
   add(m,'data_row_'+variant,row(m,variant),'Data Rows','Matching 5-column listing row. '+variant.capitalize()+' state.')
  for kind in ('action_default','action_focus','action_pressed','action_disabled','filter','stepper','amount','date','status','toggle'):
   add(m,kind,control(m,kind),'Action Controls' if kind.startswith('action') or kind in ('filter','stepper','toggle') else 'Value Wells','Independent, blank '+kind.replace('_',' ')+' component.')
