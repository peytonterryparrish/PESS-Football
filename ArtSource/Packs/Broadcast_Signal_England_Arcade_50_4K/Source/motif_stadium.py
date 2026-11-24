"""Three hand-drawn pixel-native broadcast motifs. Transparent, no fonts or assets."""
from PIL import Image, ImageDraw
from random import Random
from math import sin, pi

def draw(layer,kind,x,y,w,h,style,seed):
    if w<8 or h<8:return
    x,y,w,h=map(int,(x,y,w,h))
    patch=Image.new('RGBA',(w,h)); d=ImageDraw.Draw(patch); rng=Random(str(seed)+kind)
    C={k:tuple(v) for k,v in style.items() if k in ('dark','mid','hi','shine','red','ice')}
    def mix(a,b,t):return tuple(round(k+(l-k)*t) for k,l in zip(C[a],C[b]))
    def c(k,a=255):return C[k]+(a,)
    def xy(p):return (round((w-1)*p[0]),round((h-1)*p[1]))
    def pol(points,cl):d.polygon([xy(p) for p in points],fill=cl)
    def line(points,cl,width=1):d.line([xy(p) for p in points],fill=cl,width=max(1,round(width)))
    def point(p,cl):d.point(xy(p),fill=cl)
    def quad(a,b,c,d,u,v):
        return tuple((1-v)*((1-u)*a[i]+u*b[i])+v*((1-u)*d[i]+u*c[i]) for i in (0,1))
    # Texture pops concentrate on bright edges and bands, rather than painting noise across a tile.
    def pixels(n,bounds,palette,inset=0):
        xl,yl,xr,yr=bounds
        for i in range(n):
            t=rng.random(); yy=yl+(yr-yl)*rng.random()
            xx=xl+(xr-xl)*t
            point((xx,yy),palette[i%len(palette)])
    if kind=='net':
        # Converging top ribbon and two distinct surfaces. The foreground upright
        # enters at 73% width; outer right remains available for colour fringes.
        a,b,p,q=(.001,.10),(.73,.49),(.73,.996),(.001,.996)
        e,f=(.91,.39),(.91,.995)
        pol([(0,.115),b,p,q],c('dark',175))
        pol([b,e,f,p],c('dark',140))
        def surface(verts,n,m,back=False):
            for j in range(n+1):
                u=j/n
                pts=[quad(*verts,u+.027*sin(pi*k/16)*sin(pi*u),k/16) for k in range(17)]
                line(pts,c('mid' if back else 'hi',218 if j%3==0 else 140),w/370)
                if not back and j%3==0:
                    line([p for k,p in enumerate(pts) if k>2],c('shine',80))
            for j in range(m+1):
                v=j/m
                pts=[quad(*verts,k/20,v+.048*sin(pi*k/20)*sin(pi*v)) for k in range(21)]
                line(pts,c('mid' if back else 'hi',210 if j%3 else 241),h/300)
            for j in range(1,n):
                for k in range(1,m):
                    if (j+k)%3==0:
                        u=j/n; v=k/m
                        p=quad(*verts,u+.018*sin(pi*v)*sin(pi*u),v+.028*sin(pi*u)*sin(pi*v))
                        point(p,c('shine',210 if not back else 101))
        surface([b,e,f,p],4,8,True);surface([a,b,p,q],11,8)
        line([(0,.14),(.735,.52),p],c('dark'),w/50)
        line([(0,.117),(.738,.499),(.738,.997)],c('red',177),w/72)
        line([(0,.095),(.731,.483),(.731,.997)],c('hi',252),w/60)
        line([(0,.086),(.725,.473),(.725,.997)],c('shine',244),w/140)
        line([(.747,.482),(.912,.390),(.912,.996)],c('ice',117),w/190)
        for j in range(int(w*.77)):
            u=j/max(1,int(w*.77))
            yy=.088+.389*u+(rng.randrange(-3,5)/h)
            if j%3!=0:point((u*.73,yy),c('ice' if j%5==0 else 'shine',138))
    elif kind=='stand':
        # Skewed English steel cantilever roof, terrace bands and high lattice tower.
        # Small lights and a few acrylic blue/red deltas evoke early broadcast bleed.
        for r in range(1,26):
            u=r/25
            cl=c('ice',round((1-u)**3*15))
            for s in range(0,16):
                if (s+r)%4==0:continue
                a=(.816+(u*.211)*sin(2*pi*s/16),.15+(u*.28)*sin(2*pi*(s+4)/16))
                b=(a[0]+.032*u,a[1])
                line([a,b],cl)
        # Floodlight tower behind roof and trusses.
        left=[(.788,.225),(.761,.676)];right=[(.854,.207),(.903,.637)]
        pol([left[0],right[0],right[1],left[1]],c('dark',240))
        for rail in (left,right):line(rail,c('mid'),w/165)
        def interp(a,b,t):return tuple(k+(l-k)*t for k,l in zip(a,b))
        for t in range(7):
            l0=interp(*left,t/7);l1=interp(*left,(t+1)/7)
            r0=interp(*right,t/7);r1=interp(*right,(t+1)/7)
            line([l0,r1],c('hi',169),w/360);line([r0,l1],c('mid',224),w/390);line([l0,r0],c('mid',215))
        lamp=[(.705,.108),(.894,.025),(.951,.154),(.755,.255)]
        pol([(.701,.132),(.897,.033),(.974,.164),(.759,.279)],c('dark',254))
        pol(lamp,c('mid',240));line(lamp[:3],c('hi',208),w/180)
        side=max(1,min(w,h)//130)
        for row in range(3):
            for col in range(5):
                p=quad(*lamp,(col+.6)/5.6,(row+.62)/3.6)
                xx,yy=xy(p)
                for r,colr in ((side*3,c('ice',55)),(side*2,c('ice',179)),(side,c('shine',255))):
                    d.ellipse((xx-r,yy-r,xx+r,yy+r),fill=colr)
                d.line((xx-side,yy-side-1,xx+side,yy-side-1),fill=c('shine',191))
        # Stepped seating below a rising dash edge, dark support bays.
        terr=[(.078,.934),(.998,.622),(.998,.967),(.28,.996)]
        pol(terr,c('dark',249))
        for rr in range(10):
            y0=.984-rr*.012; y1=.864-rr*.023
            line([(.16+rr*.008,y0),(.997,y1)],c('mid',191+rr*3),h/170)
            for j in range(26):
                u=(j+rng.random()*.45)/27
                if (rr+j)%5==0:continue
                x0=.21+.785*u
                y0p=(y0+(y1-y0)*u)-.005
                point((x0,y0p),c('hi' if j%3 else 'shine',114+rr*8))
        roof=[(.112,.755),(.876,.444),(.998,.443),(.998,.589),(.19,.814)]
        pol(roof,c('dark',252))
        pol([(.109,.738),(.876,.431),(.998,.428),(.998,.475),(.12,.782)],mix('dark','mid',.72)+(253,))
        line([(.109,.738),(.876,.431),(.998,.428)],c('hi',224),h/140)
        line([(.118,.753),(.877,.448),(.998,.446)],c('red',191),h/270)
        line([(.125,.769),(.878,.466),(.998,.462)],c('mid',218))
        for j in range(7):
            u=j/7
            x0=.25+.675*u; yy=.751-.259*u
            line([(x0,yy+.005),(x0+.050,yy-.017)],c('hi',124),h/116)
            line([(x0+.004,yy+.007),(x0+.046,yy-.008)],c('shine',211))
            if j%2==0:line([(x0-.01,yy+.032),(x0-.01,.90-.201*u)],c('mid',203),w/220)
        for j in range(90):
            u=rng.random()
            xx=.12+.878*u; yy=.736-.335*u+(rng.random()*.023)
            point((xx,yy),c('hi' if j%6 else 'shine',171))
        line([(.03,.985),(.998,.864)],c('hi',202),h/135)
        line([(.073,.995),(.998,.881)],c('ice',151),h/240)
    elif kind=='terrace':
        # Grandstand roof teeth and narrow terraces at the horizon, with roof
        # vents and floor rails rather than repeated box silhouettes.
        top=[(.0,.96),(.06,.82),(.20,.80),(.20,.70),(.33,.61),(.37,.61),(.37,.52),(.48,.45),(.51,.45),(.51,.30),(.66,.22),(.69,.24),(.76,.13),(.80,.15),(.83,.23),(.87,.22),(.87,.08),(.91,.04),(.96,.06),(.96,.19),(.998,.17),(.998,.997),(.0,.997)]
        pol(top,c('dark',246))
        line(top[:-2],c('mid',225),h/80)
        for pairs in ((.08,.77,.26,.72),(.20,.69,.37,.60),(.34,.59,.51,.49),(.39,.52,.52,.45),(.51,.30,.67,.23),(.67,.29,.75,.142),(.77,.138,.83,.245),(.865,.085,.914,.040)):
            xa,ya,xb,yb=pairs
            line([(xa,ya),(xb,yb)],c('hi',197),h/105)
            line([(xa,ya+.019),(xb,yb+.019)],c('red',113),h/145)
        for i in range(7):
            xo=.10+.043*i; yy=.966-.025*i
            line([(xo,yy),(.999,yy-.31-i*.019)],c('hi' if i in (3,6) else 'mid',183 if i==6 else 159),h/125)
            for j in range(20):
                u=(j+.3*rng.random())/20
                if (i+j)%4==0:continue
                px=xo+(1-xo)*u; py=yy-(.31+i*.019)*u-.012
                line([(px,py),(px+.013,py-.006)],c('ice' if j%6==0 else 'hi',90+(i*12)),1)
        for j in range(9):
            u=j/8
            xx=.41+.572*u
            yy=.57-.37*u
            line([(xx,yy),(xx,yy+.15)],c('mid',232),w/235)
            if (j+1)%3==0:line([(xx+.004,yy+.038),(xx+.044,yy+.015)],c('shine',145),1)
        for j in range(280):
            u=rng.random();v=rng.random()
            point((.25+.747*u,.915-.43*u-.13*v),c('mid' if j%5 else 'hi',76+int(80*u)))
    else: raise ValueError(f'unknown motif {kind!r}')
    box=(max(0,-x),max(0,-y),min(w,layer.width-x),min(h,layer.height-y))
    if box[0]<box[2] and box[1]<box[3]:layer.alpha_composite(patch.crop(box),(x+box[0],y+box[1]))
    return layer

if __name__=='__main__':
    im=Image.new('RGBA',(1280,720),(31,7,66,255))
    style=dict(dark=(19,8,47),mid=(75,27,142),hi=(155,95,251),shine=(237,220,255),red=(225,26,62),ice=(34,149,248))
    draw(im,'net',0,462,273,258,style,13)
    draw(im,'stand',834,60,446,461,style,12)
    draw(im,'terrace',890,514,390,205,style,2)
    im.convert('RGB').save('/tmp/pix_broadcast_stadium078.png')
