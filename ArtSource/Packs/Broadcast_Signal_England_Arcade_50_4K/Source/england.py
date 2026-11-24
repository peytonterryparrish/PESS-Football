from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from concurrent.futures import ProcessPoolExecutor
import json,csv,hashlib,zipfile,random
import numpy as np
from render import foundation,ribbon,grids,PASSES,W,H,N
import motif_club,motif_stadium,motif_match,motif_signal
P=Path(__file__).resolve().parents[1]; OUT=P/'New_Pack'
DATA="""Saturday Terrace|stand|floodlights|waveform|L|0
Corrugated Kop|roof|terrace|bars|R|2
Cup End|stand|net|prism|L|1
High Road Corner|brick|ball|bars|R|3
The Covered End|terrace|advertising|waveform|L|0
Tunnel to the Pitch|stand|net|prism|R|1
End Stand Steel|roof|floodlights|bars|R|0
Turnstile Blue|brick|stand|waveform|L|1
Railway Terrace|terrace|brick|prism|R|2
Old Ground Signal|brick|net|bars|L|3
Under the Lamps|stand|floodlights|prism|R|3
Blue Hour Away|floodlights|terrace|waveform|L|0
The Floodlit Kop|roof|floodlights|bars|L|2
Midweek Mist|floodlights|net|prism|R|1
Lights Over Brick|brick|floodlights|waveform|L|3
Late Turnstiles|floodlights|advertising|bars|R|0
Terrace After Dark|terrace|floodlights|prism|L|1
Back of the Net|roof|net|waveform|R|2
Home End at Dusk|roof|scarf|bars|L|1
Red Cross Raster|stand|waveform|prism|R|3
Saturday Kickoff|terrace|ball|bars|L|0
Away End Teletext|advertising|terrace|waveform|R|2
CRT Touchline|stand|net|bars|L|3
Arcade Replay|roof|ball|prism|R|1
Last Train Home|brick|floodlights|waveform|L|0"""
ROWS=[s.split('|') for s in DATA.splitlines()]
BANK={k:m for m,keys in [(motif_club,'st_george scarf ball'),(motif_stadium,'net stand terrace'),(motif_match,'floodlights advertising'),(motif_signal,'prism bars waveform')] for k in keys.split()}
def architecture(im,k,x,y,w,h,s,seed):
 a=Image.new('RGBA',(w,h)); d=ImageDraw.Draw(a)
 def points(z):return [(round(X*(w-1)),round(Y*(h-1))) for X,Y in z]
 def po(z,c,v):d.polygon(points(z),fill=(*s[c],v))
 def ln(z,c,v,n=1):d.line(points(z),fill=(*s[c],v),width=n)
 if k=='roof':
  po([(0,.43),(.23,.18),(1,.04),(1,.31),(.17,.56)],'mid',225)
  for i in range(30):
   u=i/29
   ln([(.25+.75*u,.18-.139*u),(.16+.84*u,.54-.232*u)],'ice' if i%6==0 else 'hi',100+i%5*21,2)
  po([(0,.43),(.17,.55),(1,.31),(1,.38),(.17,.61),(0,.50)],'dark',249)
  ln([(0,.43),(.17,.55),(1,.31)],'ice',168,2)
  for row in range(7):
   Y=.686+row*.049; po([(0,Y),(1,Y-.26),(1,Y-.22),(0,Y+.04)],'mid',194)
   for i in range(43):
    u=(i+.5*(row%2))/43
    ln([(u,Y-.26*u),(u+.013,Y-.26*u-.004)],'ice' if (i+row)%8==0 else 'hi',66+(i+row)%7*12,2)
  for j in range(7):
   u=j/7; v=.61-.227*u;ln([(u,v),(u+.025,v+.32)],'hi',190,2)
 else:
  po([(0,.34),(1,.09),(1,.96),(0,.99)],'mid',221)
  for row in range(20):
   Y=.34+row*.036
   ln([(0,Y),(1,Y-.25)],'dark',223)
   for i in range(29):
    u=(i+.5*(row%2))/29; v=Y-.25*u
    ln([(u,v),(u,v+.034)],'dark',216)
    if (i*5+row*3+seed)%13<5:ln([(u+.005,v+.014),(u+.024,v+.009)],'red',82)
  for i in range(4):
   u=.13+.23*i; v=.87-.065*i
   q=[(u-.085,v),(u-.085,v-.21),(u-.059,v-.29),(u,v-.32),(u+.059,v-.29),(u+.085,v-.21),(u+.085,v)]
   po(q,'dark',246);ln(q,'ice',129,2)
  ln([(0,.34),(1,.09)],'ice',156,2)
 if seed%2:a=a.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
 im.alpha_composite(a,(x,y))
def place(im,k,x,y,w,h,s,seed,alpha):
 l=Image.new('RGBA',im.size)
 if k in BANK:BANK[k].draw(l,k,x,y,w,h,s,seed)
 else:architecture(l,k,x,y,w,h,s,seed)
 l.putalpha(l.getchannel('A').point(lambda a:round(a*alpha))); im.alpha_composite(l)
def art(i):
 j,t=divmod(i,2);name,stad,detail,fx,side,form=ROWS[j];form=int(form)
 p=(j*7+j//5+(1 if t else 4))%10; seed=0x8920+149*j+79*t; rng=random.Random(seed)
 b,s=foundation(p,31+j,seed); ink=Image.new('RGBA',(W,H)); ribbon(ink,j,p,s,seed)
 left=side=='L'
 def X(left,w,bleed):return -bleed if left else W-w+bleed
 fw=(397,319,362,304)[form]+t*43;fh=(236,250,273,216)[form]+t*13
 fy=((-18,123,-18,397) if not t else (-12,392,108,-12))[form]+rng.randrange(-9,9)
 sw=(440,392,459,376)[form]+t*24;sh=(287,305,274,296)[form]+t*11
 sy=(443,468,410,434)[form]+rng.randrange(-11,11)
 if stad=='floodlights':sw,sh,sy=205,278,142+(j%5)*31
 place(ink,stad,X(not left,sw,27),sy,sw,sh,s,seed+9,.95)
 loc=left if (form+t)%2==0 else not left
 if detail=='floodlights': w,h,y,bleed=191,246,14 if fy>120 or loc!=left else 223,3
 elif detail=='ball': w,h,y,bleed=211,211,545 if fy<310 or loc!=left else 246,19
 elif detail in ['brick','stand','terrace','advertising']:w,h,y,bleed=342,211,542,40
 elif detail=='net':w,h,y,bleed=268,224,524,32
 elif detail=='scarf':w,h,y,bleed=332,166,566,29
 else:w,h,y,bleed=335,135,570,28
 place(ink,detail,X(loc,w,bleed),y+rng.randrange(-10,14),w,h,s,seed+15,.80)
 w,h=(321,66) if fx=='bars' else (354,123)
 place(ink,fx,X(not left,w,26),9 if fy>220 else 625-(j%4)*14,w,h,s,seed+29,.71)
 flag=dict(s,ice=(228,229,244),shine=(252,242,249),red=(226,29,68))
 flag_im=Image.new('RGBA',(fw,fh))
 motif_club.draw(flag_im,'st_george',0,0,fw,fh,flag,seed+21)
 if not left:flag_im=flag_im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
 ink.alpha_composite(flag_im,(X(left,fw,9),fy))
 _,_,xx,yy,_=grids()
 z=np.clip((np.maximum(np.abs(xx-.5)*2,np.abs(yy-.5)*2)-.56)/.37,0,1); z=z*z*(3-2*z)
 a=np.asarray(ink.getchannel('A'),dtype='float32');ink.putalpha(Image.fromarray(np.rint(a*z).astype('uint8')))
 b.alpha_composite(ink)
 im=b.convert('RGB').quantize(colors=224,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE)
 id=f'BS{i+301:03}';group='Terrace_End' if j<10 else 'Floodlit_Grounds' if j<18 else 'Broadcast_Arcade'
 path=OUT/'Backgrounds_4K'/group/(id+'_'+name.replace(' ','_')+'_'+PASSES[p][0]+'.png')
 path.parent.mkdir(parents=True,exist_ok=True);im.resize((3840,2160),N).save(path,compress_level=7)
 rgb=im.convert('RGB');rgb.resize((440,248),N).save(P/'thumbs'/f'{id}.jpg',quality=92,subsampling=0)
 core=np.asarray(rgb,dtype='float32')[191:529,287:993]
 light=float(np.quantile(core@np.array([.2126,.7152,.0722]),.95))
 assert light<62,(id,light)
 return dict(id=id,file=path.relative_to(OUT).as_posix(),layout=name,palette=PASSES[p][0],collection=group,width=3840,height=2160,center_luma_p95=round(light,2),sha256=hashlib.file_digest(path.open('rb'),'sha256').hexdigest())
def board(rows,f,title):
 w=1600;I=Image.new('RGB',(w,109+5*194),'#0b1129');d=ImageDraw.Draw(I)
 def font(n):return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',n)
 d.rectangle((27,15,w-27,19),fill='#6de3d3');d.text((27,26),title,fill='#fff0cc',font=font(34))
 d.text((27,68),'ENGLAND ARCADE  /  ST GEORGE, ENGLISH GROUNDS & CRT DITHER',fill='#c1d7f0',font=font(14))
 for j,r in enumerate(rows):
  x=27+j%5*310;y=99+j//5*194
  I.paste(Image.open(P/'thumbs'/(r['id']+'.jpg')).resize((295,166),Image.Resampling.LANCZOS),(x,y))
  d.rectangle((x-1,y-1,x+295,y+166),outline='#4b537a');d.text((x,y+172),r['id']+'  '+r['layout'],fill='#f0eefb',font=font(13))
 I.save(f,quality=91,subsampling=0,optimize=True)
if __name__=='__main__':
 (P/'thumbs').mkdir(exist_ok=True); (OUT/'Previews').mkdir(parents=True,exist_ok=True)
 with ProcessPoolExecutor(6) as pool:rows=list(pool.map(art,range(50),chunksize=2))
 assert len({r['sha256'] for r in rows})==50
 for r in rows:
  with Image.open(OUT/r['file']) as z: assert z.size==(3840,2160) and z.format=='PNG';z.verify()
 (OUT/'Manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
 with (OUT/'Manifest.csv').open('w',newline='') as f:
  cw=csv.DictWriter(f,fieldnames=list(rows[0]));cw.writeheader();cw.writerows(rows)
 for j in range(2):board(rows[25*j:25*(j+1)],OUT/'Previews'/f'England_Arcade_{j+1:02}.jpg','BROADCAST SIGNAL / ENGLAND ARCADE '+str(j+1))
 (OUT/'CLAUDE_START_HERE.md').write_text("""# Broadcast Signal: England Arcade
50 new 3840×2160 PNGs, BS301–BS350. Companion to the clean 300-background set, not a replacement. Twenty-five layouts, each with two framing and palette treatments. Every plate has a St George's flag. No players, team logos, titles or UI are baked into the backgrounds.

## Unreal
Use as full-screen 16:9 menu backgrounds, not playable stadium art. Place existing interactive widgets above them. Keep text in the dark central area. The 1280×720 logical pixel grid is enlarged 3× with nearest-neighbor sampling. Use point sampling where practical and check the CRT texture at target display sizes. Bind by the id field in Manifest.csv or Manifest.json. Contact-sheet captions are not baked into the assets. No images have been imported into Unreal yet.
""")
 bundle=P/'Broadcast_Signal_England_Arcade_50_4K.zip'
 with zipfile.ZipFile(bundle,'w',zipfile.ZIP_DEFLATED,compresslevel=5) as z:
  for f in sorted(OUT.rglob('*')):
   if f.is_file():z.write(f,f.relative_to(OUT))
  for f in sorted(Path(__file__).parent.glob('*.py')):z.write(f,Path('Source')/f.name)
 with zipfile.ZipFile(bundle) as z:
  assert len([f for f in z.namelist() if f.startswith('Backgrounds_4K/')])==50 and not z.testzip()
 print('Verified',len(rows),'4K PNGs;',bundle.stat().st_size,'bytes')
