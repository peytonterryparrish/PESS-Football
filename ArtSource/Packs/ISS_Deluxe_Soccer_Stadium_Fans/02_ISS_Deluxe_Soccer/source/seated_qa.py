from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
import json
P=Path(__file__).resolve().parent.parent; rows=json.loads((P/'Manifest.json').read_text())['assets'][20:]
im=Image.new('RGB',(1340,654),'#e9e5d8');d=ImageDraw.Draw(im)
F=lambda sz,b=False:ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans'+('-Bold' if b else '')+'.ttf',sz)
d.text((30,19),'SEATED SUPPORTERS / ALIGNMENT CHECK',font=F(32,True),fill='#172838')
d.text((31,65),'Preview only: the teal seat and baseline are not in the PNGs.  •  Each pose fits the same field-facing convention.',font=F(15),fill='#465961')
for i,r in enumerate(rows):
 j=i%10;col=x=28+129*j;y=102+267*(i//10);w=120
 d.rounded_rectangle((x,y,x+w,y+246),radius=7,fill='#f6f2e5',outline='#b9bda9')
 d.text((x+7,y+8),f'{i+1:02}',font=F(15,True),fill='#182f3c')
 oy=y+45;xx=x+60
 seat=oy+int(r['seat_mount_px'][1]/4*3); ground=oy+189
 d.rounded_rectangle((xx-35,seat-45,xx+35,seat+3),radius=7,fill='#acc6bd',outline='#739a91',width=2)
 d.line((xx-29,seat+2,xx-29,ground),fill='#8bad9f',width=4);d.line((xx+29,seat+2,xx+29,ground),fill='#8bad9f',width=4)
 d.rounded_rectangle((xx-38,seat+1,xx+38,seat+11),radius=2,fill='#79a095',outline='#6e978e',width=1)
 d.line((x+8,ground,x+111,ground),fill='#aab4a4',width=1)
 a=Image.open(P/r['native_file']).resize((192,192),Image.Resampling.NEAREST);im.paste(a,(xx-96,oy),a)
im.save(P/'Seated_QA_Preview.png',optimize=True)
