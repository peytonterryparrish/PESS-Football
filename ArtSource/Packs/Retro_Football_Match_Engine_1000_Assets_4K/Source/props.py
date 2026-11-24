"""60 original football match props. Binary alpha, stepped 2D pixel shading."""
from pathlib import Path
from PIL import Image,ImageDraw
import zipfile,json
import numpy as np
P=Path(__file__).parent
Z=P.parent/'pix_master/source_archives'
MATCH=[
 ('Ivory_Dark_Panels',(238,233,206),(42,48,58),(184,132,63),'Traditional contrasting multi-panel leather look; not an official model.'),
 ('Pearl_Triangle_Track',(240,238,224),(35,81,133),(74,157,154),'Turn-of-century high contrast seam visibility; original chevrons.'),
 ('White_Oxblood_Orbit',(238,234,222),(158,31,53),(34,36,58),'White, oxblood and dark orbit blocks as a visual bridge to the approved pack.'),
 ('Yellow_Plum_HighVis',(249,211,52),(96,34,122),(54,29,77),'Early winter high-visibility era; yellow/purple historically supported, geometry original.'),
 ('Lime_Graphite_Wedges',(206,228,69),(39,60,63),(67,146,81),'Stylized high-visibility game variant, not a historical league release.'),
 ('Parchment_Burgundy_Stitch',(222,197,151),(115,51,54),(146,84,57),'Leather-grain throwback with narrow stitched seam fork.'),
 ('Pearl_Teal_Shocks',(236,241,226),(31,123,129),(82,178,183),'Original asymmetric color-dash panel, period-inspired.'),
 ('Orange_Navy_Winter',(240,132,56),(42,60,102),(89,127,168),'Original snow-play alternate, not an official colourway.'),
 ('White_Blue_Corner_Inlay',(239,238,221),(30,69,163),(65,140,205),'Original staggered blue socket panelling.'),
 ('Floodlight_Cream_Gold',(231,226,189),(157,109,42),(44,69,85),'A low-flash fictional stadium variant with muted gold lobes.')]
FLAGSETS=[
 ('Amber_Solid','#efb647','#77532b','#fff19c'),
 ('Carmine_Light_Chevron','#c93b52','#702e47','#fff0b7'),
 ('HighVis_Split','#dce63e','#799636','#fffab5'),
 ('Blue_White_Quarter','#378cc3','#1d567d','#f8f1d2'),
 ('Cream_Black_Bar','#ead9bd','#635b55','#fff6dd'),
 ('Fluoro_Salmon_Diagonal','#fd815e','#8f4255','#ffddb0')]
STATES=['Inward_Extended','Outward_Extended','Inward_Droop','Outward_Billow','Reset_Hanging']
def provenance():
 return json.loads((P/'Approved_Goal_Projection.json').read_text()),json.loads((P/'Approved_Ball_Index.json').read_text())
def pmap(i,v):
 n=32; yy,xx=np.indices((n,n));x=(xx-15.5)/13.9;y=(yy-15.5)/13.9
 r=x*x+y*y;z=np.sqrt(np.maximum(0,1-r));visible=r<=1
 az=np.deg2rad([12,64,126][v]); el=np.deg2rad([9,-13,24][v]);ca,sa=np.cos(az),np.sin(az);ce,se=np.cos(el),np.sin(el)
 xr=x*ca+z*sa; zr=-x*sa+z*ca
 yr=y*ce-zr*se; zr=y*se+zr*ce
 lon=np.arctan2(xr,zr); lat=np.arcsin(np.clip(yr,-1,1))
 shell,pri,acc=map(lambda q:np.array(q,np.float32),MATCH[i][1:4]); ink=shell*.55+pri*.13
 rgb=np.zeros((n,n,3),float);rgb[:]=shell
 ring=np.abs(yr+.20*xr)
 m1=np.zeros((n,n),bool);m2=m1.copy();seam=m1.copy()
 if i==0:
  # Voronoi panels anchored to icosahedral vertices, plus small side seams.
  s=(1+5**.5)/2;pts=np.array([(0,a,b*s) for a in (-1,1) for b in (-1,1)]+[(a,b*s,0) for a in (-1,1) for b in (-1,1)]+[(b*s,0,a) for a in (-1,1) for b in (-1,1)])
  pts/=np.linalg.norm(pts,axis=1)[:,None];g=np.stack((xr,yr,zr),-1)@pts.T
  first=np.sort(g,axis=-1);m1=g.max(-1)>.944;seam=(first[:,:,-1]-first[:,:,-2])<.060;m2=seam&((xx+yy)%3==0)
 elif i==1:
  t=np.abs(np.mod(lon*2.2+.23,1)-.5);h=np.abs(np.mod(lat*1.6+.16,1)-.5)
  m1=(t+.83*h)<.21;m2=(t+.83*h<.27)&~m1;seam=(t<.035)|(h<.033)
 elif i==2:
  q=np.abs(.78*yr+.64*xr);m1=(q>.26)&(q<.44);m2=(q>.44)&(q<.51);seam=(np.abs(np.sin(lon*3.2+lat*2.4))<.11)&~m1
 elif i==3:
  q=np.abs(.86*yr-.39*xr);m1=(q>.22)&(q<.44);m2=((q>.44)&(q<.52))|((q<.095)&(zr>-.4)&(xr<-.05));seam=(np.abs(np.cos(lon*3.7+lat*3.3))<.14)&~m1
 elif i==4:
  q=np.mod(lon/1.02+.5,1)
  m1=((q>.22)&(q<.46)&(lat<.63)&(lat>-.72)) ^ ((yr>.55)&(q<.55))
  m2=(q>.49)&(q<.62)&(np.abs(yr)<.72);seam=np.abs(q-.16)<.038
 elif i==5:
  k=np.abs(np.sin(lon*3.2+lat*.67));f=np.abs(np.sin((lat+.19)*4.68))
  m1=(k<.16)&(f>.29);m2=(f<.16)&(k>.49);seam=(k<.25)|(f<.25)
 elif i==6:
  k=np.abs(np.sin(lon*2.16+lat*2.72));f=np.sin(lat*3.2-lon*2.52)
  m1=(k>.83)&(f>.21);m2=(k>.64)&(f>.28)&~m1;seam=(k<.07)
 elif i==7:
  q=np.abs(yr*.72+xr*.69);s=np.cos(lon*3.6-lat)
  m1=(q<.18)&(s>-.52);m2=(q>.50)&(q<.59)&(s<.34);seam=(np.abs(np.sin(lon*4.7))<.13)
 elif i==8:
  t=np.abs(np.mod(xr*1.36+.2,1)-.5);h=np.abs(np.mod(yr*1.69+.215,1)-.5)
  m1=(t<.16)&(h<.22);m2=(t<.24)&(h<.29)&~m1;seam=(t<.035)|(h<.035)
 else:
  b=np.sin(lon*3.31+lat*1.78); q=np.cos(lat*3.8+lon*.55)
  m1=(b<-.41)&(q>.34);m2=(b>.58)&(q<-.1);seam=(abs(b)<.13)&(q>.13)
 rgb[seam]=ink; rgb[m2]=acc;rgb[m1]=pri
 # Upper-left stadium light and native shadow ramp. Light stays fixed while
 # the texture rotates in 3D; this avoids rolling highlights around a 2D disk.
 lv=np.clip(-.45*x-.51*y+.65*z,0,1)
 light=np.select([lv>.83,lv>.61,lv>.40,lv>.19],[1.08,1.00,.87,.74],.62)
 rgb*=light[...,None];outline=visible&(r>.895);rgb[outline]=rgb[outline]*.54+np.array([24,24,34])*.46
 glint=visible&(x<-.22)&(y<-.20)&(lv>.77)&((xx+yy)%5==0);rgb[glint]=rgb[glint]*.89+np.array([255,250,229])*.11
 a=np.zeros((n,n,4),np.uint8);a[visible,:3]=np.clip(np.rint(rgb[visible]/5)*5,0,255).astype(np.uint8);a[visible,3]=255
 return Image.fromarray(a)
def flags(i,pose,z):
 n=42;img=Image.new('RGBA',(n,46));d=ImageDraw.Draw(img);key,fill,shadow,light=FLAGSETS[i]
 pale='#edede2';pole='#aaaeb0';dark='#3f4851'
 x,y=20,42; dx=round(float(z[0])/max(1.,abs(z[1]))*30);t=x+dx; top=11
 # Narrow anchored socket, mast and cap. Tilt derives from the stadium
 # elevation basis (except tiny shape cues on the fabric itself).
 d.ellipse((x-3,y-1,x+3,y+2),fill=dark)
 d.line((x,y,t,top),fill=dark,width=4)
 d.line((x,y,t,top),fill=pole,width=2)
 d.line((x-1,y-1,t-1,top),fill=pale,width=1)
 d.ellipse((t-2,top-2,t+2,top+2),fill=dark)
 d.point((t-1,top-1),fill=light)
 side=1 if pose in (0,2,4) else -1
 p=[[(0,0),(7,-1),(15,1),(14,9),(7,12),(0,10)],
    [(0,0),(6,0),(15,-2),(16,6),(7,10),(0,10)],
    [(0,0),(6,2),(12,6),(11,16),(5,15),(0,10)],
    [(0,0),(7,-3),(14,-2),(16,5),(9,12),(0,10)],
    [(0,0),(4,2),(7,8),(5,16),(1,13),(0,10)]][pose]
 ox=t+side;oy=top+2;pts=[(ox+side*a,oy+b) for a,b in p]
 m=Image.new('1',img.size);md=ImageDraw.Draw(m);md.polygon(pts,1)
 cloth=Image.new('RGBA',img.size);q=ImageDraw.Draw(cloth);w=lambda a:ox+side*a
 q.polygon(pts,fill=fill)
 if i==1:q.polygon([(w(2),oy+1),(w(12),oy+4),(w(4),oy+12),(w(0),oy+10),(w(8),oy+4)],fill=light)
 if i==2:q.polygon([(w(8),oy-5),(w(18),oy-5),(w(18),oy+16),(w(7),oy+14)],fill=shadow)
 if i==3:
  for corners in [[(w(0),oy-3),(w(7),oy-3),(w(7),oy+5),(w(0),oy+5)],[(w(7),oy+6),(w(17),oy+6),(w(17),oy+18),(w(7),oy+18)]]:q.polygon(corners,fill=light)
 if i==4:q.line((w(0),oy+5,w(16),oy+7),fill=dark,width=3)
 if i==5:q.line((w(0),oy+12,w(14),oy+1),fill=light,width=3)
 q.line([pts[3],pts[4],pts[5]],fill=shadow,width=2)
 q.line([pts[0],pts[1]],fill=light,width=1)
 img.paste(cloth,(0,0),m)
 d=ImageDraw.Draw(img);d.line((w(0),oy,w(0),oy+9),fill=shadow,width=1)
 return img
def run(a):
 proj,refs=provenance()
 basis=[row[2] for row in proj['world_xyz_to_4k']]
 for i,(name,_,__,___,note) in enumerate(MATCH):
  for view,deg in enumerate((12,64,126)):
   a.add(f'prop_ball_{i+1:02}_{name}_V{view+1}',pmap(i,view),'Match_Balls','core',f'{note} Static yaw {deg}°, not an ordered animation; pivot 0.5,0.5 in sprite.')
 for i,(name,*_) in enumerate(FLAGSETS):
  for pose,state in enumerate(STATES):
   a.add(f'prop_flag_{i+1:02}_{name}_{state}',flags(i,pose,basis),'Corner_Flags','core',f'Mast-bottom pivot native (20,42). {state.replace("_"," ")} pose; one discrete wind/facing state, no frame timing. Apply identical pivot to all variants.')
 S={'ball_count':30,'flag_count':30,'ball_native_size':[32,32],'flag_native_size':[42,46],'ball_pivot_px':[15.5,15.5],'flag_ground_anchor_px':[20,42],
    'motion_note':'Ball V1–V3 are static sampled angles on the same spherical motif. They are not a completed animation or a promise of seamless rolling; optional game code may select them to imply turns. Flag states are selectable wind/facing alternatives without timing.',
    'approved_ball_source':'PIX_2006_07_10_Logo_Free_Ball_Sprites.zip (Ball_Index.json, original Match/Winter pivots and palette inspiration only; prior artwork not duplicated)',
    'source_ball_reference_entries':len(refs),'projection_source':'R08_Final_93_Club_Stadiums_4K.zip / Projection.json; projected Z basis guides mast slope',
    'period_sources':[{'url':'https://www.premierleague.com/en/news/903532','cue':'Official league history identifies the Geo Merlin at the beginning of the 2000/01 partnership; used for era context, not reproduced.'},{'url':'https://www.premierleague.com/en/news/4155220','cue':'Official league history records a 2004/05 bright-yellow winter ball and yellow/purple successor in 2006.'}],
    'originality':'All 10 balls and 6 flag palettes are original logo-free pixel patterns inspired by era conventions. No exact club, brand, product or league replica claimed.'}
 (P/'Match_Props_References.json').write_text(json.dumps(S,indent=2,ensure_ascii=False)+'\n')
