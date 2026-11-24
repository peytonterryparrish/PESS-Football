"""Original native pixel England-only UI plates using ONS 2024 England GeoJSON."""
import math,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from scipy.ndimage import binary_dilation as dil,binary_erosion as ero
HERE=Path(__file__).resolve().parent
SRC=HERE/'england_ons.geojson'
SOURCE='https://geoportal.statistics.gov.uk/datasets/ons::countries-december-2024-boundaries-uk-buc-2/about'
BBOX=(-6.5,49.75,1.91,55.92)
POSITIONS={
 'Newcastle upon Tyne':(-1.613,54.978),'Sunderland':(-1.382,54.905),'Middlesbrough':(-1.235,54.575),'Carlisle':(-2.939,54.893),
 'Leeds':(-1.549,53.80),'Bradford':(-1.759,53.795),'Huddersfield':(-1.783,53.645),'Hull':(-.329,53.745),
 'York':(-1.083,53.957),'Sheffield':(-1.470,53.382),'Barnsley':(-1.48,53.554),'Doncaster':(-1.134,53.522),
 'Manchester':(-2.242,53.480),'Liverpool':(-2.991,53.407),'Blackburn':(-2.479,53.746),'Burnley':(-2.244,53.790),
 'Bolton':(-2.429,53.577),'Wigan':(-2.632,53.545),'Bury':(-2.298,53.593),'Blackpool':(-3.05,53.817),'Preston':(-2.704,53.764),
 'Nottingham':(-1.149,52.955),'Derby':(-1.477,52.923),'Leicester':(-1.133,52.637),'Coventry':(-1.510,52.407),
 'Stoke-on-Trent':(-2.187,53.007),'Birmingham':(-1.890,52.486),'Wolverhampton':(-2.128,52.587),
 'Norwich':(1.297,52.63),'Ipswich':(1.155,52.056),'Peterborough':(-.243,52.573),'Northampton':(-.898,52.24),
 'Watford':(-.396,51.656),'Luton':(-.417,51.879),'Oxford':(-1.255,51.752),'London':(-.124,51.507),
 'Reading':(-.972,51.455),'Bristol':(-2.587,51.454),'Bath':(-2.360,51.381),'Swindon':(-1.780,51.556),
 'Gillingham':(.549,51.389),'Colchester':(.904,51.89),'Southend-on-Sea':(.712,51.546),
 'Brighton':(-.137,50.822),'Portsmouth':(-1.094,50.798),'Southampton':(-1.404,50.909),
 'Bournemouth':(-1.879,50.719),'Exeter':(-3.534,50.726),'Plymouth':(-4.143,50.376),'Yeovil':(-2.637,50.941),
}
def projection(kind):
 if kind=='flat':
  # Cylindrical local plate with cos(mean latitude) longitude correction.
  xx=lambda l:112+(l+2.295)*20.1
  yy=lambda p:125-(p-52.835)*32.8
  return (224,256),lambda l,p:(xx(l),yy(p))
 r=105; lon0,lat0=map(math.radians,(-2.295,52.835))
 def f(l,p):
  lo,la=map(math.radians,(l,p)); q=lo-lon0
  ox=math.cos(la)*math.sin(q);oy=math.cos(lat0)*math.sin(la)-math.sin(lat0)*math.cos(la)*math.cos(q)
  # A UI regional lens, not a globe scale model: magnify England around the
  # orthographic globe centre and keep it inside the navigable instrument.
  return 120+1448*ox,118-1435*oy
 return (240,240),f
def geography(sz,proj):
 js=json.loads(SRC.read_text()); props=[q.get('properties',{}) for q in js['features']]
 assert len(props)==1 and props[0]['CTRY24CD']=='E92000001' and props[0]['CTRY24NM']=='England'
 geom=js['features'][0]['geometry']; ps=geom['coordinates'] if geom['type']=='MultiPolygon' else [geom['coordinates']]
 mask=Image.new('L',sz);d=ImageDraw.Draw(mask)
 for poly in ps:
  rings=[]
  for ring in poly:
   rings.append([tuple(map(round,proj(*p))) for p in ring])
  if rings:
   d.polygon(rings[0],255)
   for ring in rings[1:]:d.polygon(ring,0)
 return np.asarray(mask)>0
BAYER=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],dtype=np.float32)/16.
def map_art(kind):
 size,proj=projection(kind); mask=geography(size,proj); y,x=np.indices(mask.shape)
 im=np.zeros((*mask.shape,4),dtype=np.uint8)
 def draw(m,z):
  im[m,:3]=z[m] if isinstance(z,np.ndarray) and z.ndim==3 else z; im[m,3]=255
 br=BAYER[y%4,x%4]
 if kind=='orb':
  rx=(x-119.5)/104;ry=(y-119.5)/104;rr=rx*rx+ry*ry; disk=rr<=1
  z=np.sqrt(np.clip(1-rr,0,1)); illum=np.clip(-.44*rx-.37*ry+.77*z,0,1)
  tone=np.clip(np.floor(illum*6+br*.68),0,6)/6
  pal=np.stack((6+tone*9,12+tone*36,31+tone*69),axis=-1)
  draw(disk,pal)
  ring=(rr>=.937)&disk
  draw(ring,np.stack((12+tone*38,46+tone*108,87+tone*123),axis=-1))
  shine=(rr>=.973)&(rr<=.988)&(rx+ry<-.25)
  draw(shine,(80,185,255))
  # Abstract globe latitude/meridian guides: no other land contours.
  grid=Image.new('L',size);gd=ImageDraw.Draw(grid)
  for at in [-.68,-.35,0,.35,.68]:
   xs=math.sqrt(1-at*at)*102
   gd.arc((120-xs,120+at*88-xs*.18,120+xs,120+at*88+xs*.18),0,180,fill=255,width=1)
  for lon in [.38,.72]:
   gd.ellipse((120-103*lon,17,120+103*lon,223),outline=255,width=1)
  gg=(np.asarray(grid)>0)&disk&~dil(mask,iterations=2)&(rr<.934)
  draw(gg,np.stack((8+tone*17,28+tone*47,59+tone*67),axis=-1))
  # A minimal four-axis instrument bracket beyond the rim.
  guides=((abs(x-120)<1)&((abs(y-120)>=109)&(abs(y-120)<=112)))|((abs(y-120)<1)&((abs(x-120)>=109)&(abs(x-120)<=112)))
  draw(guides,(78,150,211))
 out2=dil(mask,iterations=2)&~mask
 draw(out2,(13,45,87) if kind=='flat' else (20,74,130))
 outline=mask&~ero(mask,iterations=1)
 yv=np.clip((1-y/(mask.shape[0]-1))*.8+.18+.16*np.sin(x/56.-y/78.),0,1)
 k=(np.floor(yv*6+br*.8)/6)
 stripes=((x+2*y)%11==0)&((x//28+y//16)%2==0)
 g=(k*.70+((y%20==0)&(x%3!=0))*.10+stripes*.09)
 body=np.stack([12+24*g,53+111*g,111+123*g],axis=-1)
 if kind=='orb':body=np.stack([25+59*g,87+113*g,155+75*g],axis=-1)
 draw(mask,body)
 edge=np.stack([42+73*yv,125+89*yv,200+48*yv],axis=-1)
 tier=outline&((x+y)%7==0)&(yv>.45)
 draw(outline,edge);draw(tier,(139,225,251))
 return Image.fromarray(im,'RGBA')
def pin(mode):
 s=17;im=Image.new('RGBA',(s,s));d=ImageDraw.Draw(im)
 if mode=='cluster':
  d.polygon([(8,1),(15,5),(15,11),(8,15),(1,11),(1,5)],fill='#092043',outline='#79b0e4',width=1)
  for x,y in [(8,5),(5,9),(11,9)]:d.rectangle((x-1,y-1,x+1,y+1),fill='#b4dcfb')
 elif mode=='anchor':
  d.arc((1,1,15,15),7,83,fill='#f2ce74',width=1);d.arc((1,1,15,15),98,170,fill='#f2ce74',width=1)
  d.arc((1,1,15,15),185,262,fill='#f2ce74',width=1);d.arc((1,1,15,15),276,350,fill='#f2ce74',width=1)
  d.line((8,7,8,9),fill='#fff4af');d.line((7,8,9,8),fill='#fff4af')
 else:
  fill,glow,high={'idle':('#225ca8','#112d62','#9cdaff'),'active':('#edab38','#91612a','#fff1a2'),'locked':('#3a495f','#283647','#93a0ad')}[mode]
  d.polygon([(8,1),(14,7),(8,16),(2,7)],fill=glow)
  d.polygon([(8,3),(13,7),(8,13),(3,7)],fill=fill)
  d.polygon([(8,3),(11,6),(8,10),(5,6)],fill=high)
  if mode=='active':
   for x,y in [(1,3),(15,3),(2,11),(14,11)]:d.point((x,y),fill='#f7db82')
 return im
def scan():
 size=(224,256); im=Image.new('RGBA',size);d=ImageDraw.Draw(im)
 for y in range(12,256,16):
  for x in range((y//16)%2*4,224,8):d.point((x,y),fill=(54,135,220,100))
 for u,v in [(13,18),(212,18),(13,237),(212,237)]:
  sg=1 if u<112 else -1;sy=1 if v<128 else -1
  d.line((u,v,u+sg*12,v),fill=(96,188,244,194))
  d.line((u,v,u,v+sy*10),fill=(96,188,244,194))
 return im
def anchors():
 q={'coordinate_reference':'WGS84 decimal degrees; approximate centre-of-city anchors; not venue positions.',
 'controls':'Origin top-left. Pixel anchors are for native sprite, use normalized counterparts or scale to drawn size. Pin alignment anchor is (8,13) idle/active/locked or (8,8) anchor & cluster.',
 'source':SOURCE,'maps':{k:{'size':list(projection(k)[0])} for k in ('flat','orb')},
 'cities':[]}
 for label,(l,p) in POSITIONS.items():
  r={'name':label,'lon_lat':[l,p]}
  for kind in ('flat','orb'):
   size,proj=projection(kind);x,y=proj(l,p);r[kind+'_px']=[round(x,2),round(y,2)];r[kind+'_uv']=[round(x/size[0],5),round(y/size[1],5)]
  q['cities'].append(r)
 (HERE/'England_City_Anchors.json').write_text(json.dumps(q,indent=2,ensure_ascii=False)+'\n')
 return q
def run(a):
 for mode in ('flat','orb'):
  a.add('map_england_'+mode,map_art(mode),'Maps',family='england',note='Official ONS simplified England boundary. This stylized globe is a regional locator, not a world navigation map.')
 for name,mode in [('pin_idle','idle'),('pin_active','active'),('pin_locked','locked'),('cluster_idle','cluster'),('city_anchor_ring','anchor')]:
  a.add('map_'+name,pin(mode),'Maps',family='england',note='Independent reusable pixel map marker; coordinate manifest specifies its anchor.')
 a.add('map_scan_overlay',scan(),'Maps',family='england',note='Unmarked handheld locator overlays only.')
 anchors()
if __name__=='__main__':
 class Local:
  def add(self,name,im,*arg,**kw):
   folder=HERE/'map_preview';folder.mkdir(exist_ok=True);im.save(folder/(name+'.png'))
 run(Local())
