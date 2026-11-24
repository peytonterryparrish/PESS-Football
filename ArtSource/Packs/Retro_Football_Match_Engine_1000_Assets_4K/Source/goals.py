"""Custom England football goal visual families, exact approved ground projection."""
from pathlib import Path
from PIL import Image, ImageDraw
import json, math

HERE=Path(__file__).parent
P=json.loads((HERE/'Approved_Goal_Projection.json').read_text())
M=P['world_xyz_to_4k']; O=P['origin_4k_px']
# Geometry dimensions denote back bottom depth, back top depth and back rail height.
# Styles, architecture and palette are invented evocations, never replicas.
DESIGNS=[
('Foundry Timber','1890s–1910s',1.85,.12,.05,'square',8,10,'wood','rope',(220,204,164),(194,181,151),'Squared ivory timber; sparse knotted jute-colored wedge net'),
('Municipal Lattice','1920s–1930s',2.05,.46,.37,'square',10,13,'square','grid',(227,222,201),(197,208,181),'Painted municipal frame, kicked steel brackets and tall low rail'),
('Postwar Stanchion','1940s–1950s',1.92,.88,.19,'round',11,14,'round','diamond',(239,232,208),(199,203,170),'Round ivory tubes, offset bowed stanchions and diamond cotton cord'),
('Television Arc','1960s',2.22,.96,.18,'round',13,15,'round','grid',(236,236,224),(197,218,204),'Broad bowed supports; bright frame and rectangular broadcast mesh'),
('Terrace Candystripe','1970s',2.00,.73,.10,'square',11,18,'red','diamond',(242,234,216),(238,209,182),'Painted cream-and-claret lower banding and deep diamond mesh'),
('Floodlight Hook','1980s',2.31,1.21,.13,'round',15,18,'round','hex',(223,234,239),(185,215,224),'Deep hooked rear corners and silvery open hexagonal pattern'),
('Continental Box','1990s',2.38,2.39,2.50,'hidden',12,20,'round','grid',(240,247,239),(209,232,224),'Full depth freestanding rectangle; discreet rear tension poles'),
('Millennium Mono','2000s',2.13,2.12,2.28,'round',10,15,'round','hex',(244,247,255),(197,214,238),'Pale aluminium and large tensioned hex mesh; slight rear roof drop'),
('Two-Tone Cup','2010s',2.26,2.25,2.37,'hidden',10,14,'round','two',(244,251,249),(207,227,253),'White and cobalt two-tone net paneling; floating roof'),
('Modern Slimline','2020s',1.80,1.76,2.39,'hidden',15,20,'round','fine',(229,248,246),(192,249,235),'Compact thin high-frequency mint mesh; hidden rear tie points'),
]
SIZE=(3840,2160)
def proj(x,y,z):
    return tuple(O[k]+sum(t*p for t,p in zip(M[k],(x,y,z))) for k in range(2))
def roundpt(p):return tuple(round(q) for q in p)
def tint(c,g):return tuple(min(255,round(k*g)) for k in c)
def lerp(a,b,t):return tuple(x+(y-x)*t for x,y in zip(a,b))
def shadow(radius, x,sg, depth):
    im=Image.new('RGBA',SIZE)
    d=ImageDraw.Draw(im)
    # Simulated cast light screen upper-right, rendered in world space.
    p=lambda d0,y,z:roundpt(proj(x+sg*d0-.89*z,y+.43*z,0))
    silhouette=[p(-.21,-3.77,0),p(depth+.24,-3.77,0),p(depth+.36,3.76,0),p(-.23,3.82,0)]
    d.polygon(silhouette,fill=(19,34,26,34))
    d.line([p(0,-3.72,0),p(0,-3.72,2.5),p(0,3.72,2.5),p(0,3.72,0)],fill=(15,28,20,105),width=3 if radius>.05 else 2)
    d.line([p(depth,-3.72,0),p(depth,3.72,0)],fill=(28,43,30,80),width=2)
    d.line([p(0,-3.72,0),p(depth,-3.72,0)],fill=(17,31,23,79),width=2)
    d.line([p(0,3.72,0),p(depth,3.72,0)],fill=(17,31,23,79),width=2)
    box=im.getbbox(); pix=im.load()
    for yy in range(box[1],box[3]):
        for xx in range(box[0],box[2]):
            if pix[xx,yy][3]==34:
                if (xx+2*yy)%5==1:pix[xx,yy]=(13,33,22,61)
                elif (xx+yy)&1:pix[xx,yy]=(17,34,24,14)
    return im
def make(i,end):
    name,era,D,dt,zt,support,nside,nback,coat,pattern,F,N,note=DESIGNS[i-1]
    sg=-1 if end=='south' else 1;x=sg*52.5
    def world(p):return proj(x+sg*p[0],p[1],p[2])
    def line(draw,points,col,w=1): draw.line([roundpt(world(p)) for p in points],fill=col,width=w)
    frame=Image.new('RGBA',SIZE); ff=ImageDraw.Draw(frame)
    net=Image.new('RGBA',SIZE); nn=ImageDraw.Draw(net)
    width=3
    def rail(draw,pts,pale=F,w=3,style=coat):
        shifted=lambda p,n:(p[0]+n*.028,p[1],p[2])
        line(draw,[shifted(p,-.9) for p in pts],tint(pale,.48)+(255,),w+1)
        line(draw,pts,tint(pale,.86)+(255,),w)
        line(draw,[shifted(p,.61) for p in pts],pale+(255,),1)
        if style=='wood':
            for pa,pb in zip(pts,pts[1:]):
                for k in range(1,9):
                    if k%3 ==0:
                        p=lerp(pa,pb,k/9)
                        line(draw,[shifted(p,-.3),shifted(lerp(pa,pb,k/9+.014),.2)],(133,112,86,255))
    y0,y1=-3.72,3.72
    corners=[(0,y0,0),(0,y0,2.5),(0,y1,2.5),(0,y1,0)]
    roof=[(0,y0,2.5),(0,y1,2.5),(dt,y1,zt),(dt,y0,zt)]
    rear=[(dt,y0,zt),(dt,y1,zt),(D,y1,0),(D,y0,0)]
    A=[(0,y0,0),(0,y0,2.5),(dt,y0,zt),(D,y0,0)]
    B=[(0,y1,0),(0,y1,2.5),(dt,y1,zt),(D,y1,0)]
    def surface(q,u,v,curvature=.0):
        p=lerp(lerp(q[0],q[1],u),lerp(q[3],q[2],u),v)
        # Mild fabric sag is interior only; ties/surrounds remain fixed.
        return (p[0]+curvature*math.sin(math.pi*u)*math.sin(math.pi*v),p[1],p[2]-.035*math.sin(math.pi*u)*math.sin(math.pi*v))
    def strand(q,coords,k,face):
        rgb=N
        if pattern=='two' and (face+k//3)%3==1:rgb=(96,156,240)
        gain=(.69,.90,1.0,.77)[face]
        alpha={'rope':166,'grid':147,'diamond':146,'hex':153,'two':172,'fine':119}[pattern]
        col=tint(rgb,gain)+(alpha+(25 if k%5==0 else 0),)
        line(nn,[surface(q,u,v,.10 if i<7 else .038) for u,v in coords],col)
    def cloth(q,n,m,face):
        if pattern in ('grid','rope','two','fine'):
            for k in range(n+1):
                strand(q,[(k/n,v/5) for v in range(6)],k,face)
            for k in range(m+1):
                strand(q,[(u/12,k/m) for u in range(13)],k,face)
            if pattern=='rope':
                for k in range(1,n):
                    for l in range(1,m):
                        if (k+l)%2==0:line(nn,[surface(q,k/n,l/m),surface(q,k/n+.013,l/m)],(216,203,164,214))
        elif pattern=='diamond':
            for slope in (-1,1):
                for k in range(-n,n*2+1):
                    b=k/n; vs=[j/30 for j in range(31)]
                    t=[(b+slope*v*.95,v) for v in vs if -.0001<=b+slope*v*.95<=1.0001]
                    if len(t)>1:strand(q,t,k,face)
        else:
            # Clipped connected hex lattice built from six shared edges in UV.
            m=max(2,m//2); n=max(3,n//2)
            drawn=set()
            for c in range(n+2):
                u=c/n
                for r in range(-1,m+2):
                    v=(r+.5*(c%2))/m
                    poly=[(u+du/n,v+dv/m)for du,dv in [(-2/3,0),(-1/3,-.5),(1/3,-.5),(2/3,0),(1/3,.5),(-1/3,.5)]]
                    for j in range(6):
                        aa,bb=poly[j],poly[(j+1)%6]
                        key=tuple(sorted((tuple(round(w,5) for w in aa),tuple(round(w,5) for w in bb))))
                        if key in drawn:continue
                        drawn.add(key)
                        seq=[lerp(aa,bb,k/6) for k in range(7)]
                        seq=[p for p in seq if 0<=p[0]<=1 and 0<=p[1]<=1]
                        if len(seq)>=2:strand(q,seq,c+r,face)
        for j in range(4):strand(q,[((0,0),(1,0),(1,1),(0,1))[j],((0,0),(1,0),(1,1),(0,1))[(j+1)%4]],0,face)
    # Thin layers rendered farther to nearer on fixed camera; open apertures remain transparent.
    for face,q,n,m in ((0,B,nside,max(4,nside//2)),(1,rear,nback,max(4,nside//2)),(2,roof,nback,max(4,nside//2)),(3,A,nside,max(4,nside//2))):
        cloth(q,n,m,face)
    for y in (y0,y1):
        if support=='square':
            rail(nn,[(0,y,.03),(D,y,.03),(dt,y,zt)],tint(F,.70),2)
            if i==2:rail(nn,[(.25,y,.07),(dt,y,.85),(.05,y,2.32)],tint(F,.78),2)
        elif support=='round':
            if i<7:
                rail(nn,[(0,y,2.49),(dt*.62,y,2.32),(D*.68,y,1.55),(D*.88,y,.64),(D,y,.05)],tint(F,.74),2)
            else:rail(nn,[(D,y,.02),(dt,y,zt)],tint(F,.73),2)
        else:
            line(nn,[(D,y,.02),(dt,y,zt)],tint(F,.63)+(214,),1 if i==10 else 2)
    if i!=1:rail(nn,[(D,y0,.035),(D,y1,.035)],tint(F,.67),2)
    if i>=7:rail(nn,[(dt,y0,zt),(dt,y1,zt)],tint(F,.73),2 if i!=10 else 1)
    rail(ff,corners,F,width)
    for y in (y0,y1):
        if coat=='red':
            for lo,hi in [(.20,.52),(.77,1.06)]:
                rail(ff,[(0,y,lo),(0,y,hi)],(175,66,64),3,'round')
        if i in (2,4,7,8,9,10):
            line(ff,[(0,y,.015),(0,y,.08)],(156,165,143,255),2)
        if i==3:
            for z in [1.04,1.78]:line(ff,[(0,y,z),(0,y,z+.05)],(167,154,131,255),3)
    if i==9:
        for t in range(1,18):
            y=y0+(y1-y0)*t/18
            line(ff,[(0,y,2.49),(.05,y,2.49)],(187,209,230,255))
    sh=shadow(.06,x,sg,D)
    co=Image.alpha_composite(sh,net);co.alpha_composite(frame)
    return dict(zip(('shadow','rear_net','front_frame','composite'),(sh,net,frame,co))),dict(
      title=name,era=era,note=note,geometry={'front_world_x_m':x,'bar_center_height_m':2.5,'post_center_y_m':[y0,y1],'clear_opening_m':[7.32,2.44],
      'back_ground_depth_m':D,'back_top_depth_m':dt,'back_top_height_m':zt},
      post_anchors_4k_px=[list(world(p)) for p in corners])
def run(a):
    rec={}
    for i in range(1,11):
        for end in ('south','north'):
            ims,meta=make(i,end)
            for kind,im in ims.items():
                B=im.getbbox(); tile=im.crop(B); id=f'goal_{i:02}_{end}_{kind}'
                rec[id]={**meta,'atlas_xywh_4k_px':[B[0],B[1],tile.width,tile.height],'position_bottom_center_4k_px':list(proj(-52.5 if end=='south' else 52.5,0,0))}
                a.add(id,tile,'Goals',family='goal_art',note=f"{meta['title']} ({meta['era']}) / {end.title()} / {kind} | original era-inspired artwork; use exact atlas placement sidecar")
    data={'atlas_size':[3840,2160],'sampling':'point/nearest at cutout placement; no normalized re-scaling','placement_basis':'Original approved 4K world projection; x negative South, x positive North. Cropped tiles use top-left XY plus integral pixel WH. No AA resampling.',
          'layer_order':['shadow','rear_net','actors (depth sorted)','front_frame'],'composite_note':'Alternative preview/convenience only; do not add with component layers.','assets':rec}
    (HERE/'Goals_Placement.json').write_text(json.dumps(data,indent=2)+'\n')
if __name__=='__main__':
    class Capture:
        def __init__(self):self.ims={}
        def add(self,id,im,*args,**kwargs):self.ims[id]=im.copy()
    a=Capture();run(a)
    tiles=Image.new('RGB',(1500,1020),(16,28,29));d=ImageDraw.Draw(tiles)
    for i,r in enumerate(DESIGNS,1):
        dx=(i-1)%5*300;dy=(i-1)//5*510
        d.rounded_rectangle((dx+8,dy+8,dx+291,dy+498),radius=10,fill=(26,48,44),outline=(58,89,69),width=2)
        d.text((dx+21,dy+19),f'{i:02}  {r[0].upper()}',fill=(235,231,203));d.text((dx+21,dy+36),r[1],fill=(162,185,172))
        for k,end in enumerate(('south','north')):
            im=a.ims[f'goal_{i:02}_{end}_composite'];d.text((dx+21,dy+76+204*k),end.upper(),fill=(139,186,150));tiles.paste(im,(dx+150-im.width//2,dy+97+204*k),im)
    tiles.save(HERE/'Goals_Review.png')
    print(len(a.ims),len(set(im.tobytes() for im in a.ims.values())), min(im.width for im in a.ims.values()),max(im.width for im in a.ims.values()))
