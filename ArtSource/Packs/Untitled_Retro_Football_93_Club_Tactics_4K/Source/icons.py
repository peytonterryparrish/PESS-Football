from PIL import Image,ImageDraw
import math
PAL={
'blue':('#070921','#122564','#183da9','#788ff8','#e9f3ff','#ffbb3a'),
'gold':('#100d1b','#534014','#8b651a','#d0aa4d','#fff598','#f19f36'),
'red':('#100b20','#660e27','#a11d28','#de503c','#ffe7db','#ff829c'),
'teal':('#081a31','#0c3346','#086e72','#119e97','#d1f0e4','#73e8d2'),
'core':('#080920','#1c0d44','#372185','#6354bd','#fffde5','#ffdb43')
}
def rgb(c):
 return tuple(int(c[i:i+2],16) for i in (1,3,5))
def mix(a,b,t):
 return tuple(round(x+(y-x)*t) for x,y in zip(rgb(a),rgb(b)))
GLYPHS={
 'home':("0000000000000","0000001000000","0000011100000","0000111110000","0001111111000","0011111111100","0111111111110","0001111111000","0001110111000","0001110111000","0001110111000","0001110111000"),
 'away':("0000000000000","0111111111000","1101110111100","1101110110110","1101110110011","1111111111111","1111111111111","1111111111111","0011000001100","0011000001100"),
 'vol_up':("0000000000000","0000011000100","0000111000010","0111111001010","0111111000110","0111111000110","0111111001010","0000111000010","0000011000100"),
 'settings':("0000011100000","0010011100100","0011111111100","0001111111000","0011000001100","1111000001111","1111000001111","0011000001100","0001111111000","0011111111100","0010011100100","0000011100000"),
 'squad':("0000011100000","0000111110000","1100111110011","1100111110011","1100011100011","0010000000100","1110111110111","1111111111111","1111111111111","1101111111011"),
 'tactics':("0000000000000","1111111111111","1000010100001","1010010000101","1001010010001","1000111001001","1000111000001","1001010010101","1010010100001","1000010000001","1111111111111"),
 'trophy':("0000000000000","0011111111100","1111111111111","1011111111101","1011111111101","1111111111111","0011111111100","0001111111000","0000111110000","0000011100000","0000011100000","0001111111000"),
 'whistle':("0000000000000","0000001111110","0000111111110","0001111100000","0011101111100","0111000111110","1110000011110","1110000011110","0111000111100","0011111111000","0001111100000"),
 'calendar':("0011000001100","0011000001100","1111111111111","1111111111111","1000000000001","1001010101001","1000000000001","1001010101001","1000000000001","1001010101001","1111111111111"),
 'clock':("0000111110000","0011100011100","0110001000110","0100001000010","1100001000011","1000001000001","1000001111001","1100000000011","0100000000010","0110000000110","0011100011100","0000111110000"),
 'lock':("0000111110000","0001100011000","0001000001000","0001000001000","0011111111100","0011111111100","0011110111100","0011100011100","0011110111100","0011110111100","0011111111100"),
 'medic':("0000000000000","0000111110000","0000111110000","0000111110000","0111111111110","0111111111110","0111111111110","0000111110000","0000111110000","0000111110000"),
 'stats':("0000000000000","1000000000011","1000000000011","1000000000011","1000000110011","1011000110011","1011000110011","1011000110011","1011000110011","1111111111111"),
 'stamina':("0000000000000","1111111111100","1000000000110","1010101010111","1010101010111","1010101010111","1000000000110","1111111111100"),
 'swap':("0000000000000","0000000001000","0000000001100","0111111111110","0111111111111","0000000001100","0010000001000","0110000000000","1111111111110","0111111111110","0010000000000"),
 'save':("0011111111100","0111111111110","0101100010010","0101100010010","0111111111110","0111111111110","0111000001110","0111011101110","0111000001110","0111000001110","0111111111110"),
 'boots':("0000110000000","0000111000000","0000111000000","0000111100000","0001111100000","0011101110000","0111110111000","1111111011110","1111111111111","1111111111111","0100100100100"),
 'glove':("0001010101000","0001010101000","0001111111000","0001111111000","0001111111000","0101111111000","0111111110000","0011111110000","0001111100000","0000111100000","0000111100000"),
 'exit':("1111100010000","1000000011000","1000000011100","1000000011110","1000111111111","1000111111111","1000000011110","1000000011100","1000000011000","1111100010000"),
 'info':("0000011100000","0000011100000","0000000000000","0000111100000","0000011100000","0000011100000","0000011100000","0000011100000","0000111110000","0000111110000"),
 'confirm':("0000000000000","0000000000011","0000000000111","0000000011110","1100000111100","1110001111000","0111011110000","0011111100000","0001111000000","0000110000000"),
 'cancel':("1100000000011","1110000000111","0111000001110","0011100011100","0001110111000","0000111110000","0000011100000","0000111110000","0001110111000","0011100011100","0111000001110","1110000000111","1100000000011"),
}
# Hand-built narrow font for button caps, 5 x 7 alphabet subset, deliberate hard grid.
FONT={
'A':('01110','11011','11011','11111','11011','11011','11011'),
'B':('11110','11011','11011','11110','11011','11011','11110'),
'X':('11011','11011','11011','01110','11011','11011','11011'),
'Y':('11011','11011','11011','01110','00100','00100','00100'),
'L':('11000','11000','11000','11000','11000','11000','11111'),
'R':('11110','11011','11011','11110','11100','11010','11011'),
'S':('01111','11000','11000','01110','00011','00011','11110'),
'T':('11111','00100','00100','00100','00100','00100','00100'),
'0':('01110','11011','11011','11011','11011','11011','01110'),
'1':('00100','01100','00100','00100','00100','00100','01110'),
'2':('01110','11011','00011','00110','01100','11000','11111'),
'3':('11110','00011','00011','01110','00011','00011','11110'),
}
def im(s): return Image.new('RGBA',s)
def bitmap(dr,lines,x,y,scale,col):
 for j,row in enumerate(lines):
  for i,p in enumerate(row):
   if p=='1':dr.rectangle((x+i*scale,y+j*scale,x+i*scale+scale-1,y+j*scale+scale-1),fill=col)
def label(dr,word,x,y,scale,color):
 for i,ch in enumerate(word):bitmap(dr,FONT[ch],x+i*6*scale,y,scale,color)
D=((0,8,2,10),(12,4,14,6),(3,11,1,9),(15,7,13,5))
def bevel(size,family,hue=None):
 p=PAL[family]; w,h=size; out=im(size); d=ImageDraw.Draw(out)
 base=[(4,2),(w-5,2),(w-3,4),(w-3,h-5),(w-5,h-3),(4,h-3),(2,h-5),(2,4)]
 d.polygon([(x,y+2) for x,y in base],fill='#080b17')
 d.polygon(base,fill=p[0],outline=p[2])
 d.line([(3,h-10),(3,8),(8,3),(w-9,3),(w-5,7)],fill=p[3],width=1)
 d.line([(w-4,8),(w-4,h-9),(w-9,h-4),(8,h-4)],fill=p[1],width=2)
 mask=Image.new('1',size)
 ImageDraw.Draw(mask).polygon([(8,5),(w-9,5),(w-6,8),(w-6,h-9),(w-9,h-6),(8,h-6),(5,h-9),(5,8)],fill=1)
 px=out.load(); mm=mask.load()
 for y in range(5,h-5):
  for x in range(5,w-5):
   if mm[x,y]:
    t=max(0,min(1,(int(4*(.7-.53*(y-5)/(h-10))+(D[y%4][x%4]/16-.5)*.95)+.3)/3.8))
    px[x,y]=(*mix(p[0],hue or p[2],t),255)
 d.point((5,7),fill=p[4]);d.point((w-6,h-8),fill=p[1]); return out
def emblem(name,pal):
 out=bevel((40,40),pal); d=ImageDraw.Draw(out); p=PAL[pal]
 g=GLYPHS[name]; x=(40-2*len(g[0]))//2;y=(40-2*len(g))//2
 bitmap(d,g,x+1,y+1,2,'#070c1c'); bitmap(d,g,x,y,2,p[4])
 s=Image.new('1',out.size); bitmap(ImageDraw.Draw(s),g,x,y,2,1)
 for j in range(y+len(g),y+len(g)*2):
  for i in range(x,x+len(g[0])*2):
   if s.getpixel((i,j)):out.putpixel((i,j),(*mix(p[4],p[3],(j-y-len(g))/max(1,len(g)-1)*.9),255))
 return out
def controller(tok,fam):
 shoulder=tok in ('L1','L2','R1','R2','ST','SEL')
 w,h=(45,23) if shoulder else (32,32)
 if shoulder:
  out=bevel((w,h),fam);d=ImageDraw.Draw(out)
 else:
  out=im((w,h));d=ImageDraw.Draw(out); p=PAL[fam]
  d.ellipse((2,3,29,30),fill='#0a1123');d.ellipse((2,1,29,28),fill=p[0],outline=p[3],width=1)
  d.ellipse((4,3,27,26),fill=p[2]);d.ellipse((6,5,25,24),fill=p[1])
  for y in range(5,24):
   for x in range(6,26):
    if (x-15.5)**2+(y-14.5)**2<91:
     out.putpixel((x,y),(*mix(p[1],p[3],(.6*(23-y)/19)+(.14 if (x+y)%2==0 else 0)),255))
  d.line((9,4,21,4),fill=p[4])
 p=PAL[fam]; scale=2
 if tok!='SEL':
  x=(w-((len(tok)*6-1)*scale))//2;y=(h-7*scale)//2-1
  label(d,tok,x+1,y+1,scale,p[0]);label(d,tok,x,y,scale,p[4])
 else:
  for i in range(3):
   d.line((13,7+3*i,31,7+3*i),fill=p[4],width=2)
 return out
def moods(kind,col,light):
 out=im((32,32)); d=ImageDraw.Draw(out)
 d.ellipse((2,4,29,31),fill='#090f23');d.ellipse((2,2,29,29),fill='#221927')
 d.ellipse((4,3,27,26),fill=col)
 for y in range(4,26):
  for x in range(4,28):
   if (x-15.5)**2+(y-14.5)**2<116:
    t=max(0,1-((x-11)**2+(y-9)**2)**.5/25)*.80+(.09 if (x+y)%2 else 0)
    out.putpixel((x,y),(*mix(col,light,t),255))
 d.line((9,6,14,6),fill=light); d.point((8,7),fill=light)
 e='#201924'
 if kind in ('high','good'):
  d.rectangle((10,11,12,14),fill=e);d.rectangle((19,11,21,14),fill=e)
  d.polygon([(8,17),(22,17),(20,22),(17,24),(14,24),(10,21)],fill=e)
  if kind=='high':d.rectangle((11,18,20,19),fill='#ffedbb')
 elif kind in ('low','terrible'):
  d.line([(9,12),(13,14)],fill=e,width=2); d.line([(19,14),(23,12)],fill=e,width=2)
  d.line([(9,23),(12,20),(19,20),(22,23)],fill=e,width=2)
  if kind=='terrible':d.point((24,16),fill='#e2faff');d.line((24,17,24,20),fill='#5fdbff',width=2)
 elif kind=='injured':
  d.line((8,11,13,16),fill=e,width=2); d.line((8,16,13,11),fill=e,width=2)
  d.line((18,11,23,16),fill=e,width=2); d.line((18,16,23,11),fill=e,width=2); d.line((12,21,20,21),fill=e,width=2)
 elif kind=='fatigued':
  d.line((9,14,13,14),fill=e,width=2);d.line((18,14,22,14),fill=e,width=2); d.rectangle((13,19,19,23),fill=e)
 else:
  d.rectangle((10,11,12,14),fill=e);d.rectangle((19,11,21,14),fill=e);d.line((11,20,21,20),fill=e,width=2)
 return out
def badge(kind,color):
 out=im((32,32));d=ImageDraw.Draw(out)
 if kind=='selection':
  for x,y,a,b in [(4,4,1,1),(27,4,-1,1),(4,27,1,-1),(27,27,-1,-1)]:
   d.line([(x+a*7,y),(x,y),(x,y+b*7)],fill='#091729',width=5)
   d.line([(x+a*7,y),(x,y),(x,y+b*7)],fill=color,width=2)
 elif kind=='captain':
  d.polygon([(6,7),(25,7),(25,23),(6,23)],fill=color,outline='#17141f',width=2)
  d.line((9,8,22,8),fill='#fff6c2')
  d.line([(19,12),(12,12),(12,19),(19,19)],fill='#242039',width=2)
 elif kind=='injury':
  d.rectangle((5,5,26,26),fill='#fff0d8',outline='#161b2b',width=2)
  d.rectangle((13,8,18,23),fill=color);d.rectangle((8,13,23,18),fill=color)
 elif kind=='yellow_card' or kind=='red_card':
  d.polygon([(8,28),(4,10),(22,4),(26,22)],fill='#151527')
  d.polygon([(9,26),(5,9),(22,3),(26,20)],fill=color,outline='#402323')
  d.line((7,9,20,5),fill='#ffe3c7')
 elif kind=='ball':
  d.ellipse((4,5,27,28),fill='#151a32');d.ellipse((4,3,27,26),fill='#ecf5f1',outline='#384568')
  d.polygon([(15,9),(20,13),(18,18),(12,18),(10,13)],fill='#283457')
  d.line((8,7,12,10),fill='#293656',width=3);d.line((23,9,20,12),fill='#303e5e',width=3)
  d.line((24,21,19,19),fill='#4a5773',width=3);d.line((13,24,14,19),fill='#44536d',width=2)
  d.line((6,17,10,14),fill='#617b91',width=2)
 elif kind=='goalkeeper':
  d.polygon([(16,2),(29,15),(16,28),(3,15)],fill='#121b31',outline=color,width=2)
  d.rectangle((11,11,20,20),fill=color);d.line((12,12,18,12),fill='#fffbde')
 elif kind=='bullet':
  d.ellipse((7,8,25,26),fill='#09182c')
  d.ellipse((5,5,24,24),fill=color,outline='#121f3a',width=2); d.ellipse((9,8,14,11),fill='#ebfbff')
 else:
  d.polygon([(16,3),(28,14),(16,27),(3,14)],fill='#101e32',outline=color,width=2)
  d.polygon([(16,8),(22,14),(16,20),(10,14)],fill=color)
 return out
def arrow(angle,col):
 out=im((32,32));d=ImageDraw.Draw(out)
 xy=[(5,11),(17,11),(17,5),(27,15),(17,25),(17,19),(5,19)]
 a=angle*math.pi/180
 xy=[(round(16+(x-16)*math.cos(a)-(y-16)*math.sin(a)),round(16+(x-16)*math.sin(a)+(y-16)*math.cos(a))) for x,y in xy]
 d.polygon([(x,y+2) for x,y in xy],fill='#0b1325')
 d.polygon(xy,fill=col,outline='#15223d');d.line(xy[:3],fill='#f7fbda',width=1)
 return out
def kit(kind,a,b):
 out=im((44,43));d=ImageDraw.Draw(out)
 xy=[(11,5),(17,3),(26,3),(31,5),(40,12),(34,21),(31,18),(31,38),(11,38),(11,18),(8,21),(2,12)]
 d.polygon([(x+1,y+2) for x,y in xy],fill='#151728')
 d.polygon(xy,fill=a,outline='#24213b',width=2)
 mask=Image.new('1',out.size);ImageDraw.Draw(mask).polygon(xy,fill=1);p=out.load();m=mask.load()
 for y in range(6,37):
  for x in range(4,39):
   if m[x,y] and m[x-1,y] and m[x+1,y] and m[x,y-1] and m[x,y+1]:
    use=(kind=='stripes' and 11<x<31 and x%8<4) or (kind=='hoops' and 12<x<32 and y%9<5) or (kind=='sash' and 12<x<31 and 9<x-y<15) or (kind=='sleeves' and (x<11 or x>30)) or (kind=='quarters' and 11<x<31 and ((x<21)==(y<22))) or (kind=='keeper' and y<16)
    c=b if use else a
    t=.25+(.18 if y<20 else 0)+(.15 if (x+y)%2 else 0)- .35*abs(x-19)/22
    p[x,y]=(*mix(c,'#eaf7ff',max(0,t)),255)
 d.line([(16,4),(17,8),(21,10),(25,8),(27,4)],fill='#251a38',width=2)
 d.line((13,35,28,35),fill=b)
 d.line((12,36,29,36),fill='#231c31')
 return out
W='#eff5ea';R='#d73347';B='#275dc5';N='#273c7d';Y='#ffcf4d';G='#2b9b56';K='#202333';O='#f29445'; L='#62bef0'
FLAGS={
 'england':('cross',W,R,0),'scotland':('saltire',B,W,0),'ireland':('v',G,W,O),
 'france':('v',N,W,R),'italy':('v',G,W,R),'germany':('h',K,R,Y),
 'netherlands':('h',R,W,N),'belgium':('v',K,Y,R),'switzerland':('smallcross',R,W,0),
 'austria':('h',R,W,R),'spain':('boldh',R,Y,R),'poland':('h2',W,R,0),
 'romania':('v',N,Y,R),'ukraine':('h2',B,Y,0),'finland':('nordic',W,N,0),
 'sweden':('nordic',B,Y,0),'denmark':('nordic',R,W,0),
 'norway':('doublecross',R,W,N),'iceland':('doublecross',N,W,R),
 'argentina':('h',L,W,L),'japan':('disc',W,R,0),
 'colombia':('boldtop',Y,N,R),'peru':('v',R,W,R),'nigeria':('v',G,W,G),
 'brazil':('diamond',G,Y,B),'portugal':('portugal',R,G,Y),
 'wales':('dragon',W,G,R),'czechia':('wedge',W,R,N),'croatia':('checkers',R,W,N),
 'turkey':('crescent',R,W,0),'greece':('stripes',B,W,0),
 'usa':('usa',R,W,N),'mexico':('mexico',G,W,R)
}
def flag(name):
 out=im((48,34));d=ImageDraw.Draw(out)
 d.rectangle((2,4,46,32),fill='#101a2d');d.rectangle((1,1,45,30),fill='#0d1529',outline='#a1b9c7')
 mode,a,b,c=FLAGS[name]; x,y,xx,yy=3,3,43,28
 d.rectangle((x,y,xx,yy),fill=a)
 if mode=='v':
  d.rectangle((17,y,29,yy),fill=b);d.rectangle((30,y,xx,yy),fill=c)
 elif mode in ('h','boldh','h2','boldtop'):
  if mode=='h2':d.rectangle((x,16,xx,yy),fill=b)
  else:
   n,z={'h':(11,20),'boldh':(9,23),'boldtop':(16,23)}[mode]
   d.rectangle((x,n,xx,z-1),fill=b);d.rectangle((x,z,xx,yy),fill=c)
  if name=='argentina': d.ellipse((21,13,26,18),fill=Y)
 elif mode in ('cross','nordic','doublecross','smallcross'):
  p=23 if mode in ('cross','smallcross') else 16
  lo,hi,hy,hyy=(3,28,3,43) if mode!='smallcross' else (7,24,12,34)
  s=3 if mode=='doublecross' else 2
  d.rectangle((p-s,lo,p+s,hi),fill=b);d.rectangle((hy,15-s,hyy,15+s),fill=b)
  if mode=='doublecross':d.rectangle((p-1,3,p+1,28),fill=c);d.rectangle((3,14,43,16),fill=c)
 elif mode=='saltire':
  d.line((3,3,43,28),fill=b,width=5);d.line((3,28,43,3),fill=b,width=5)
 elif mode=='disc':d.ellipse((16,8,30,22),fill=b)
 elif mode=='diamond':
  d.polygon([(23,5),(40,15),(23,26),(6,15)],fill=b); d.ellipse((16,9,30,22),fill=c);d.line((17,14,28,16),fill=W,width=1)
 elif mode=='portugal':
  d.rectangle((3,3,18,28),fill=b);d.ellipse((14,11,23,20),fill=c)
  d.rectangle((17,13,20,17),fill=R);d.point((18,14),fill=W)
 elif mode=='dragon':
  d.rectangle((3,16,43,28),fill=b)
  d.polygon([(10,18),(8,14),(9,11),(14,15),(18,16),(18,11),(15,6),(22,9),(25,5),(26,11),(29,14),(32,13),(32,9),(34,6),(35,8),(39,8),(39,11),(35,11),(35,17),(31,19),(28,19),(32,23),(34,24),(34,26),(30,26),(24,21),(21,21),(18,25),(14,25),(14,23),(17,22),(17,18),(14,20),(10,20)],fill=c)
  d.point((35,9),fill=K)
 elif mode=='wedge':
  d.rectangle((3,16,43,28),fill=b);d.polygon([(3,3),(23,15),(3,28)],fill=c)
 elif mode=='checkers':
  d.rectangle((3,11,43,19),fill=b);d.rectangle((3,20,43,28),fill=c)
  for j in range(5):
   for i in range(5):d.rectangle((18+i*2,11+j*2,19+i*2,12+j*2),fill=a if (i+j)%2==0 else b)
 elif mode=='crescent':
  d.ellipse((10,7,28,24),fill=b);d.ellipse((15,8,30,23),fill=a)
  d.polygon([(32,10),(33,13),(37,13),(34,16),(35,19),(32,17),(29,19),(30,16),(27,13),(31,13)],fill=b)
 elif mode=='stripes':
  for j in range(1,8,2): d.rectangle((3,3+j*3,43,min(28,5+j*3)),fill=b)
  d.rectangle((3,3,19,17),fill=a);d.rectangle((9,3,12,17),fill=b);d.rectangle((3,9,19,11),fill=b)
 elif mode=='usa':
  for j in range(1,13,2):d.rectangle((3,3+j*2,43,4+j*2),fill=b)
  d.rectangle((3,3,21,16),fill=c)
  for j in range(4):
   for i in range(5):d.point((5+i*3+(j%2),5+j*3),fill=b)
 elif mode=='mexico':
  d.rectangle((17,3,29,28),fill=b);d.rectangle((30,3,43,28),fill=c)
  d.arc((19,13,27,20),0,180,fill=G,width=1);d.polygon([(20,13),(21,11),(24,13),(26,13),(24,16),(22,17)],fill='#90552e')
 # Pixel shading stays subtle so flag colours remain recognizable.
 pix=out.load()
 for j in range(y,yy+1):
  for i in range(x,xx+1):
   r,g,bl,_=pix[i,j]; f=1-((j-y)/(yy-y))*.10 -(.07 if ((i+j)%2==0 and j>18) else 0)
   pix[i,j]=(round(r*f),round(g*f),round(bl*f),255)
 return out

# Additional 13px outlines, sharing the same logical grid as the menu symbols.
GLYPHS.update({
'sort':("0000000000000","1000011111111","1100000000000","1110011111000","1000000000000","1000011100000","1000000000000","1000010000000","1000000000000","1000000000000"),
'filter':("1111111111111","1111111111111","0111111111110","0011111111100","0001111111000","0000111110000","0000011100000","0000011100000","0000011100000","0000011000000"),
'search':("0011111000000","0110001100000","1100000110000","1000000010000","1000000010000","1100000110000","0110001110000","0011111011000","0000000001100","0000000000110","0000000000011"),
'heart':("0011100011100","0111110111110","1111111111111","1111111111111","0111111111110","0011111111100","0001111111000","0000111110000","0000011100000","0000001000000"),
'price':("0000111111111","0001111110011","0011111110011","0111111111111","1111111111110","1111111111100","0111111111000","0011111110000","0001111100000","0000111000000"),
'uptrend':("0000000011111","0000000011111","0000000001111","0000000011011","0000000110011","0000001100000","0001011000000","0011110000000","0110100000000","1100000000000"),
'downtrend':("1100000000000","0110100000000","0011110000000","0001011000000","0000001100000","0000000110011","0000000011011","0000000001111","0000000011111","0000000011111"),
'flat':("0000000000000","0000000000000","0000000000100","0000000000110","1111111111111","1111111111111","0000000000110","0000000000100"),
'clock_small':("0000111110000","0011000001100","0100001000010","0100001000010","1000001000001","1000001111001","0100000000010","0100000000010","0011000001100","0000111110000"),
'sale':("0011100000110","0110110001100","1100011011000","1100011110000","0110111100000","0011111001110","0001100011011","0011000110001","0110000011011","1100000001110"),
'coins':("0001111100000","0111111111000","1000000000100","1111111111100","1000000000110","1111111111110","0110000000001","0011111111111","0010000000001","0001111111110"),
'grid':("1111001111000","1001001001000","1001001001000","1111001111000","0000000000000","1111001111000","1001001001000","1001001001000","1111001111000"),
'list':("1100111111111","1100111111111","0000000000000","1100111111111","1100111111111","0000000000000","1100111111111","1100111111111"),
'shield':("0001111100000","0111111111000","1111111111100","1111111111100","1111111111100","0111111111000","0011111110000","0001111100000","0000111000000","0000010000000"),
'location':("0000111110000","0011111111100","0111000001110","1110000000111","1110000000111","0111000001110","0011111111100","0001111111000","0000111110000","0000011100000","0000001000000"),
})
def pin(kind, fill, hi):
 out=im((29,33)); d=ImageDraw.Draw(out)
 p=[(9,3),(18,3),(24,8),(24,16),(21,20),(14,30),(6,20),(3,15),(3,9)]
 d.polygon([(x+1,y+1) for x,y in p],fill='#05061b')
 d.polygon(p,fill=fill,outline='#090f2e',width=2)
 d.line([(5,12),(5,10),(10,5),(17,5),(21,8)],fill=hi)
 d.ellipse((9,8,18,17),fill='#090d27',outline=hi)
 if kind=='selected':d.rectangle((12,11,15,14),fill='#fff7c6')
 elif kind=='capital':d.line((12,13,15,13),fill=hi,width=2);d.line((14,11,14,15),fill=hi,width=2)
 elif kind=='visited':d.line([(11,12),(13,15),(17,11)],fill=hi,width=1)
 else:d.point((14,13),fill=hi)
 return out
def run(a):
 def add(k,o,c,f='core',n=''):
  a.add('ico_'+k,o,c,f,n)
 for fam,choices in {
  'blue':('home','away','squad','tactics','swap','boots','save','stats','calendar','settings','whistle','trophy','glove','info','exit','confirm','cancel','location'),
  'teal':('tactics','swap','sort','filter','search','list','grid','location','confirm','stats','price','clock_small','stamina','squad','calendar','shield'),
  'gold':('trophy','heart','coins','price','sale','uptrend','glove','lock','info','away'),
  'red':('medic','cancel','downtrend','heart','sale','exit','lock','vol_up','shield','location','whistle','confirm')
 }.items():
  for name in choices:add(f'{fam}_{name}',emblem(name,fam),'Menu icons',fam)
 for fam,choices in {'blue':('A','B','X','Y','L1','L2','R1','R2','ST','SEL'),'red':('A','B','X','Y'),'teal':('A','B','L1','R1')}.items():
  for tok in choices:add(f'{fam}_button_{tok.lower()}',controller(tok,fam),'Controller prompts',fam)
 for n,c,s in [('excellent','#b92b34','#ffcf68'),('good','#d9852f','#fff0a2'),('neutral','#bd9934','#fff49a'),('low','#3564c1','#9dc8ff'),('very_low','#5141a0','#bdafff'),('injured','#747481','#d1d8df'),('fatigued','#236574','#8ddfcc')]:
  add('morale_'+n,moods('high' if n=='excellent' else 'terrible' if n=='very_low' else n,c,s),'Player status','core')
 for n,c in [('selection','#ffd568'),('captain','#ffe172'),('injury','#d13551'),('yellow_card','#ffda48'),('red_card','#ed4e45'),('ball','#ffffff')]:
  add('status_'+n,badge(n,c),'Match symbols','core')
 for i,n in enumerate(('east','south_east','south','south_west','west','north_west','north','north_east')):
  add('cursor_'+n,arrow(i*45,'#f19f36'),'Cursor arrows','gold')
 for n,base,accent in [('home','#161de1','#abcaff'),('away','#bc392f','#faeabb')]:
  for style in ('solid','stripes','hoops','sash','sleeves','quarters','keeper'):
   add(f'kit_{n}_{style}',kit(style,base,accent),'Kit tokens','core')
 for name in FLAGS:
  add('flag_'+name,flag(name),'Country flags','core','Original miniature of public national flag design; some insignia simplified.')
 for state,fam in [('available','blue'),('selected','gold'),('locked','red'),('visited','teal'),('capital','gold')]:
  p=PAL[fam];add('map_pin_'+state,pin(state,p[2],p[4]),'England map pins',fam)
 for i,(name,fam,color,light) in enumerate([
  ('north_east','red','#a11d28','#ff829c'),('north_west','blue','#183da9','#9db6ff'),('yorkshire_humber','blue','#2e51b9','#c0d5ff'),
  ('east_midlands','gold','#8b651a','#ffd76b'),('west_midlands','gold','#bd7d25','#fff1a0'),('east_england','teal','#097d83','#8ef2dc'),
  ('london','red','#bf303e','#ffc5bf'),('south_east','teal','#159a86','#c3ffdc'),('south_west','core','#6354bd','#d9c9ff')
 ]):
  add('map_region_'+name,pin('capital' if i==6 else 'plain',color,light),'England region legend',fam,'Color swatch and locator token; names provided by the game.'),
