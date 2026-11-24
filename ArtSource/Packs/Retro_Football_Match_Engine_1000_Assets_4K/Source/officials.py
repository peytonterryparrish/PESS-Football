"""Sixty native 64px SOUTH idle referee sprites; source bases from the approved R08 pack."""
from pathlib import Path
from PIL import Image,ImageDraw
B=Path(__file__).resolve().parent/'official_inputs'
# Distinct classic and contemporary-looking officiating uniforms.
K=[
 ('black','24272d','cbd2d7','flat'),('charcoal','343841','e6daa6','seam'),('frost_black','282831','7fabcb','panel'),
 ('slate','495261','e8e9e8','double'),('graphite','313039','d2765c','cuffs'),
 ('yellow','e8ca42','282c34','panel'),('gold','d5a739','292d37','offset'),('light_yellow','f2e267','55585e','cuffs'),
 ('lime','a9c644','293923','seam'),('old_yellow','cec25b','e4e1ba','double'),
 ('sky','73b1db','253b59','panel'),('royal_blue','3c76ad','d7e0ee','offset'),('pale_sky','a9cde0','3c5368','seam'),
 ('ultramarine','4a6299','d3d6dd','cuffs'),('navy','324779','88a7c7','double'),
 ('scarlet','c63f47','34313a','panel'),('brick_red','a7464d','d1c9c4','double'),('ruby','b94c62','2e263a','cuffs'),
 ('vermilion','d66548','562d34','seam'),('red_clay','a74639','b9b4be','offset'),
 ('teal','46a396','283941','panel'),('forest','477459','cad6b8','seam'),('aqua','5ebfc2','313c59','cuffs'),
 ('jade','4b9673','a5d3be','offset'),('pine','416861','b1c6b8','double'),
 ('rose','c16686','393147','panel'),('violet','765d9f','d3cee2','offset'),('orange','d9874c','3f3345','seam'),
 ('pearl','dbd9c7','282a3a','cuffs'),('dusty_lilac','a79dbd','4a365f','double')]
def rgb(s):return tuple(int(s[i:i+2],16) for i in (0,2,4))
def blend(a,b,k):return tuple(round(x*(1-k)+y*k) for x,y in zip(a,b))
def tone(a,k):return tuple(max(0,min(255,round(x*k))) for x in a)+(255,)
def luma(c):return .27*c[0]+.62*c[1]+.11*c[2]
def old_red(c):
 r,g,b=c[:3];return r>1.9*g and b>g*.98 and r>23
def texture(im,i):
 _,col,piping,cut=K[i]; c=rgb(col); tr=rgb(piping)
 oa=im.copy(); p=im.load(); b=oa.load()
 for y in range(64):
  xs=[x for x in range(64) if b[x,y][3]]
  for x in xs:
   r,g,z,alpha=b[x,y]
   sleeve=11<=y<=20 and min(abs(x-xs[0]),abs(xs[-1]-x))<=2 and not (r>1.35*g and g>1.14*z)
   chest=(11<=y<=25 and 26<=x<=36 and (old_red(b[x,y]) or 14<=y<=24 and 28<=x<=35))
   if chest or sleeve:
    if old_red(b[x,y]): shade=max(.45,min(1.31,(r+7)/181))
    elif sleeve: shade=max(.32,min(.87,(luma(b[x,y])+18)/68))
    else:
     n=[b[xx,y] for xx in range(26,38) if old_red(b[xx,y])]
     shade=max(.55,min(1.16,(sorted(k[0] for k in n)[len(n)//2]+5)/182)) if n else .87
    edge=min(x-xs[0],xs[-1]-x)
    marked=((cut in ('panel','offset') and 13<=y<=24 and x in ((26,36) if cut=='panel' else (27,))) or
       (cut in ('cuffs','double') and sleeve and 18<=y<=19) or (cut=='seam' and (y in (12,13) and abs(x-31)>=4)) or
       (cut=='double' and y in (14,16) and x in (28,34)))
    p[x,y]=tone(tr if marked else c,shade*(.90 if edge==0 else 1))
   # Remove old team socks and cloth hue without touching calves, hands or shoes.
   if 47<=y<=59 and old_red(b[x,y]):
    base=(40,44,51) if i%3!=1 else blend(c,(25,30,36),.61)
    p[x,y]=tone(tr if y==49 and i%4!=3 else base,max(.44,min(1.32,r/130)))
   if 26<=y<=37 and 26<=x<=37 and old_red(b[x,y]):p[x,y]=tone((39,43,51),.68+.45*(r/255))
 d=ImageDraw.Draw(im)
 # Two unbranded pockets, zip and whistle/light tape in custom shapes.
 for x in ((28,33) if i%4==0 else ((33,) if i%4==1 else (28,))):
  for dx in (0,1,2):p[x+dx,16]=tone(c,.63)
  p[x,17]=tone(c,.76); p[x+2,17]=tone(c,.69); p[x+1,18]=tone(c,.82)
 if i%3==0:d.line([(31,11),(31,15)],fill=tone(tr,.62))
 if i%6==2:p[31,14]=tone(tr,.89)
 if i%2:
  y=28+i%5
  xa=min(x for x in range(20,27) if p[x,y][3])
  p[xa+1,y]=tone((36,41,49),.90);p[xa+1,y+1]=tone((166,184,202),.80)
 return im
def flag(im,i):
 d=ImageDraw.Draw(im); flip=i%2
 # Paint pole behind grip, direct pixel flag geometries down and away from body.
 conv=lambda x:63-x if flip else x
 y=33+i%3
 hand=min((x for x in range(18,29) if im.getpixel((conv(x),y))[3]),default=22)
 xmap=lambda x:conv(x+(hand-21))
 cols=[((255,198,42),(224,87,37)),((244,221,62),(219,65,46)),((231,163,50),(247,214,103)),((249,204,46),(225,103,38)),((238,215,69),(205,78,43))]
 hi,lo=cols[i%len(cols)]
 o=im.copy()
 def xy(x,yy):return (xmap(x),yy)
 d.line([xy(21,y-2),xy(15,y+21)],fill=(27,28,36,255),width=2)
 d.line([xy(21,y-2),xy(15,y+21)],fill=(150,149,143,255))
 end=6+i%3; yp=y+4+(i//3%2); yb=yp+10+i%3
 poly=[xy(19,yp),xy(end,yp+3+i%2),xy(end-1,yb+2),xy(16,yb)]
 d.polygon(poly,fill=hi+(255,),outline=(120,61,38,255))
 for yy in range(yp+1,yb+2):
  for xx in range(end,19):
   xx1=xmap(xx)
   if im.getpixel((xx1,yy))!=hi+(255,):continue
   if ((xx-end)//3+(yy-yp)//3+i)%2:
    if i%3!=0 or (xx+yy)%5<3:d.point((xx1,yy),fill=lo+(255,))
   elif (xx+2*yy)%5==0:d.point((xx1,yy),fill=tone(hi,.91))
 d.line([xy(19,yp),xy(15,yb+4)],fill=(173,158,118,255))
 # Original hand over shaft, without modifying grip or skin silhouette.
 for xx in range(18,25):
  for yy in range(y-3,y+4):
   pix=o.getpixel(xy(xx,yy))
   if pix[3]:im.putpixel(xy(xx,yy),pix)
 return im
def run(a):
 for role in ('center','assistant'):
  for i,(name,_,__,cut) in enumerate(K):
   j=(i*7+(i//10)*2+(3 if role=='assistant' else 0))%10
   src=Image.open(B/f'F{j+1:03}.png').convert('RGBA')
   im=texture(src,i)
   if role=='assistant': im=flag(im,i)
   im.putdata([p if p[3] else (0,0,0,0) for p in im.getdata()])
   id=f'ref_{role}_{i+1:02}_{name}_south_idle'
   a.add(id,im,'Officials','core',f'R08-derived native 64×64. {role.title()} referee, south idle only. {name.replace("_"," ").title()} kit, {cut} finish. '+('Flag integrated; alternates hand and flag textile.' if role=='assistant' else 'Unbranded shirt pockets and officiating details.'))
