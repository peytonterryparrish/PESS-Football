"""Original pixel motifs for 07 Broadcast Signal: geometric shaded cloth and stitched ball."""
from PIL import Image
import numpy as np
import math
BAYER=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]],'f')/16
def _c(s,k,v):
 return np.array(s.get(k,v),dtype=np.float32)[:3]
def _mix(a,b,t):return a+(b-a)*np.asarray(t)[...,None]
def _shade(base,dark,light,val,noise,levels,grain=0.5):
 vt=np.clip(val,0,1)*levels
 # Ordered 4×4 dithering with minor position-dependent scanline shading.
 k=(np.floor(vt)+(vt%1>noise))/levels
 clipped=np.clip(k,0,1)
 return np.where((clipped<.52)[...,None],_mix(dark,base,clipped/.52),_mix(base,light,np.clip((clipped-.52)/.48,0,1)))
def _cloth(kind,u,v,px,py,s,seed,g):
 r=np.random.default_rng(seed); t=r.uniform(.0,math.tau)
 dark=_c(s,'dark',(16,12,61));mid=_c(s,'mid',(44,27,128)); ice=_c(s,'ice',(222,239,247)); bright=_c(s,'shine',(231,222,248)); red=_c(s,'red',(197,7,48))
 upper=.16+.086*np.sin(u*9+t)+.080*np.sin(u*4+.2)+.12*u
 lower=.80+.125*np.sin(u*10+t+.48)-.21*u+.055*np.sin(u*6)
 if kind=='scarf':
  upper=.31+.14*np.sin(u*7.2+t)-.13*u
  lower=upper+.36+.11*np.sin(u*4+.9)
 dv=(v-upper)/(lower-upper)
 m=(u>.01)&(u<.985)&(dv>=0)&(dv<=1)
 fold=(.44+.235*np.sin(u*17+t+dv*4.9)+.16*np.sin(u*29+dv*5.8-t)+.068*np.cos(dv*10-u*16))
 fold+=.13*(1-dv)+.055*np.cos(u*6+dv*13)+.07*np.sin(2*u+1.2)*np.sin(dv*17)
 if kind=='st_george':
  a=np.abs(u-.39+.035*np.sin(dv*4+t)) < .079
  b=np.abs(dv-.46+.037*np.cos(u*6+t)) < .158
  cross=a|b
  base=np.where(cross[...,None],red,ice)
  light=np.where(cross[...,None],_mix(red,bright,.43),_mix(ice,np.array([255.,247,252]),.71))
  shadow=np.where(cross[...,None],_mix(red,dark,.69),_mix(ice,mid,.75))
  cv=_shade(base,shadow,light,fold,g,6)
 else:
  band=(u<.14)|(u>.87)
  border=(abs(dv-.08)<.027)|(abs(dv-.89)<.040)
  sw=np.floor(u*36)%7==0
  base=np.where((border|(band&sw))[...,None],ice,np.where((band|(dv>.37)&(dv<.64))[...,None],red,_c(s,'hi',(103,66,196))))
  light=_mix(base,bright,.57);shadow=_mix(base,dark,.74)
  knit=(np.mod(px+py%2,3)==0)*(np.mod(py,4)<2)
  cv=_shade(base,shadow,light,fold+.11*knit-.09*(py%4==3),g,5)
  ends=(((u-.006)*11+(dv*81)%2*.12>.0)&(u<.022)|((u>.978)&(u<.996)))&(dv>=0)&(dv<=1)&((py%5<2))
  m=m|ends; cv=np.where(ends[...,None],_mix(ice,red,.19),cv)
 ed=(dv<.018)|(dv>.982)|(u<.025)|(u>.977)
 cv=np.where(ed[...,None],_mix(cv,dark,.49),cv)
 return cv,m

def _ball(u,v,px,py,s,seed,g,a):
 # Great-circle surface seams: six near-side hexagons around each dark pentagonal panel.
 # Generate truncated-icosahedron-like spherical tiling from vertices + face centers;
 # the cells have 5/6 neighbours and the panel borders are evaluated in 3D.
 p=(1+math.sqrt(5))/2
 verts=np.array([(0,b,c*p) for b in (-1,1) for c in (-1,1)]+[(b,c*p,0) for b in (-1,1) for c in (-1,1)]+[(b*p,0,c) for b in (-1,1) for c in (-1,1)],'f')
 verts/=np.linalg.norm(verts[0]); ds=verts@verts.T
 faces=np.array([(i,j,k) for i in range(12) for j in range(i+1,12) for k in range(j+1,12) if ds[i,j]>.4 and ds[i,k]>.4 and ds[j,k]>.4],int)
 cen=verts[faces].sum(axis=1); cen/=np.linalg.norm(cen,axis=1)[:,None]
 points=np.concatenate((verts,cen)); ang=(seed%17)*.041+.48; tilt=.34+(seed%7)*.055
 rz=np.array([[math.cos(ang),-math.sin(ang),0],[math.sin(ang),math.cos(ang),0],[0,0,1.]])
 rx=np.array([[1,0,0],[0,math.cos(tilt),-math.sin(tilt)],[0,math.sin(tilt),math.cos(tilt)]])
 points=points@rz@rx
 # Use an inscribed circle rather than stretching a traditional football.
 x=(u-.50)*2.13*a[0]; y=(v-.50)*2.13*a[1]
 rr=x*x+y*y; z=np.sqrt(np.maximum(0,1-rr)); m=rr<=1
 pos=np.stack([x,y,z],axis=-1); scores=(pos.astype('f')@points.T.astype('f'))
 best=scores.argmax(axis=-1); pair=np.partition(scores,-2,axis=-1)[...,-2:]
 near=np.abs(pair[...,1]-pair[...,0])
 ice=_c(s,'ice',(212,226,245)); dark=_c(s,'dark',(13,9,51)); hi=_c(s,'hi',(99,60,174)); mid=_c(s,'mid',(41,19,108)); bright=_c(s,'shine',(242,239,251))
 color=np.where((best<12)[...,None],_mix(dark,hi,.28),_mix(ice,hi,.19))
 light=np.where((best<12)[...,None],_mix(dark,hi,.65),_mix(ice,bright,.72))
 shadow=np.where((best<12)[...,None],_mix(dark,mid,.21),_mix(mid,dark,.33))
 lam=np.clip(.43+.61*(z*.83-x*.47-y*.42),.06,.99)
 cv=_shade(color,shadow,light,lam+.048*(px%5==0)*(py%4==2),g,7)
 seam=near<(.012+.010*rr)
 rim=(rr>.938)&((x<.6)&(y<.72))
 cv=np.where(seam[...,None],_mix(cv,dark,.73),cv)
 cv=np.where((seam&((px*3+py*2)%9<2))[...,None],_mix(cv,ice,.42),cv)
 cv=np.where(rim[...,None],_mix(cv,bright,np.clip(.52-.54*(x+y),0,.85)),cv)
 cv=np.where((rr>.972)[...,None],_mix(cv,dark,.60),cv)
 return cv,m
def draw(layer,kind,x,y,w,h,style,seed):
 w,h=int(w),int(h); x,y=int(x),int(y);seed=int(seed)
 if w<=0 or h<=0:return layer
 if kind not in ('st_george','scarf','ball'):raise ValueError(kind)
 # Evaluate local bounds; do not draw anything outside the assigned rectangle.
 # Horizontal CRT texture remains native-resolution with staggered dither.
 px,py=np.meshgrid(np.arange(w),np.arange(h))
 u=(px+.5)/w;v=(py+.5)/h
 d=BAYER[(py+seed)%4,(px+seed//3)%4]
 if kind=='ball': cv,m=_ball(u,v,px,py,style,seed,d,(w/min(w,h),h/min(w,h)))
 else:cv,m=_cloth(kind,u,v,px,py,style,seed,d)
 cv=cv*(1.0-.11*(py%3==2)[...,None])
 arr=np.empty((h,w,4),np.uint8); arr[...,:3]=np.clip(cv,0,255).astype(np.uint8); arr[...,3]=m*255
 im=Image.fromarray(arr)
 layer.alpha_composite(im,(x,y))
 return layer
