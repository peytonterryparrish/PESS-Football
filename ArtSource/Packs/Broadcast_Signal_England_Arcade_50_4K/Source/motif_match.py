"""Original Broadcast Signal stadium and play illustrations, bbox-local pixel layers."""
from PIL import Image,ImageDraw
import math,random
STYLE={'dark':(14,7,48),'mid':(49,18,119),'hi':(104,40,195),'shine':(230,184,255),'red':(224,13,76),'ice':(78,167,255)}
ORDER=((0,8,2,10),(12,4,14,6),(3,11,1,9),(15,7,13,5))
def draw(layer,kind,x,y,w,h,style,seed):
 if w<2 or h<2:return
 out=Image.new('RGBA',(w,h)); d=ImageDraw.Draw(out); rng=random.Random(seed)
 colors={k:tuple(style.get(k,v))[:3] for k,v in STYLE.items()}
 def ink(key,a=255):return colors[key]+(a,)
 def p(X,Y):return (round(X*(w-1)),round(Y*(h-1)))
 def pol(points,k,a=255):d.polygon([p(*z) for z in points],fill=ink(k,a))
 def ln(points,k,a=255,sw=1):d.line([p(*z) for z in points],fill=ink(k,a),width=max(1,round(sw*min(w,h)/195)))
 def ell(X,Y,rx,ry,k,a=255):d.ellipse((p(X-rx,Y-ry),p(X+rx,Y+ry)),fill=ink(k,a))
 def dust(X,Y,rx,ry,k,alpha):
  x0=max(0,int((X-rx)*w));x1=min(w,int((X+rx)*w)+1)
  y0=max(0,int((Y-ry)*h));y1=min(h,int((Y+ry)*h)+1)
  for yy in range(y0,y1):
   for xx in range(x0,x1):
    u=(xx/w-X)/rx;v=(yy/h-Y)/ry;f=(1-u*u-v*v)
    if f>0 and ORDER[yy%4][xx%4]/16<(f**2)*.87:r=ink(k,max(1,int(alpha*f**.8)));d.point((xx,yy),fill=r)
 if kind=='floodlights':
  # Tilted array seen from below, tapering double cross-braced truss.
  flip=seed%2
  dust(.46,.25,.52,.30,'hi',107);dust(.48,.21,.43,.22,'ice',122);dust(.46,.17,.30,.14,'shine',80)
  pol([(.47,.36),(.61,.31),(.76,.92),(.51,.98)],'dark',244)
  for j in range(6):
   a=.36+j*.095;b=a+.095
   L=lambda yy:.479+(yy-.36)*.075
   R=lambda yy:.601+(yy-.36)*.247
   ln([(L(a),a),(R(b),b)],'hi',205,1.4)
   ln([(R(a),a),(L(b),b)],'mid',236,2)
   ln([(L(b),b),(R(b),b)],'hi',175,1.4)
  ln([(.479,.36),(.525,.98)],'hi',240,3);ln([(.60,.32),(.747,.92)],'ice',156,2)
  ln([(.54,.40),(.635,.96)],'mid',228,2)
  pol([(.18,.24),(.72,.075),(.84,.14),(.87,.29),(.32,.45),(.21,.39)],'dark')
  pol([(.19,.24),(.72,.075),(.83,.14),(.85,.27),(.31,.41),(.23,.37)],'mid')
  pol([(.23,.245),(.72,.107),(.79,.151),(.825,.254),(.32,.382),(.256,.339)],'hi')
  for row in range(3):
   for j in range(6):
    X=.275+j*.084+row*.023;Y=.254-j*.024+row*.047
    ell(X,Y,.038,.032,'ice',135);ell(X,Y,.025,.023,'shine',255);ell(X-.006,Y-.005,.016,.016,'shine',255)
    if (row+j)%3==1:d.point(p(X,Y),fill=(255,248,255,255))
  ln([(.21,.373),(.32,.423),(.849,.275)],'ice',217,2)
  if flip:out=out.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
 elif kind=='advertising':
  # Angle-matched rails, terraces and perimeter signage; no letters or emblems.
  pol([(.0,.77),(.38,.57),(.98,.35),(.98,.79),(.02,.97)],'dark',195)
  for row in range(3):
   Y=.48+row*.069
   pol([(.0,Y+.36),(.98,Y-.13),(.98,Y-.103),(.0,Y+.404)],'mid',155+22*row)
   ln([(.018,Y+.349),(.98,Y-.144)],'hi',172+19*row,1.5)
   for j in range(24):
    X=.015+j*.041+((row%2)*.013);phase=rng.random()
    if phase<.35:continue
    yy=Y+.332-.48*X
    pol([(X,yy),(X+.02,yy-.009),(X+.02,yy+.007),(X,yy+.016)],'shine' if phase>.93 else 'ice',64+int(phase*128))
  edges=[.018,.25,.45,.63,.79,.9,.987]
  for j,(a,b) in enumerate(zip(edges,edges[1:])):
   ya=.785-.31*a;yb=.785-.31*b;dy=.125-.05*a
   pol([(a,ya),(b,yb),(b,yb+dy),(a,ya+dy)],'dark',230)
   ln([(a,ya),(b,yb)],'ice',210,2);ln([(a,ya+dy),(b,yb+dy)],'hi',216,2)
   ln([(a,ya),(a,ya+dy)],'hi',204,2)
   left=a+.02;right=b-.014
   if right<=left:continue
   ya1=.785-.31*left;yb1=.785-.31*right
   pol([(left,ya1+.021),(right,yb1+.021),(right,yb1+dy-.018),(left,ya1+dy-.018)],'hi' if j%3 else 'mid',220)
   for k in range(4):
    dx=(right-left)/4;u=left+dx*k; yy=.785-.31*u+.054
    if (k+j)%4==3:continue
    pol([(u,yy),(u+dx*.48,yy-.011),(u+dx*.31,yy+.012),(u,yy+.021)],'red' if j%2 else 'ice',183)
  ln([(.01,.968),(.987,.63)],'ice',167,2)
 else:raise ValueError(kind)
 layer.alpha_composite(out,(x,y))
