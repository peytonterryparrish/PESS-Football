"""300 original HUD event, timing, and player-name assets."""
from PIL import Image, ImageDraw, ImageFilter
from lettering import text, face
import unicodedata

# 35 uppercase characters and their corresponding lowercase partners plus
# player-name apostrophe and interpunct. Core ASCII remains in the master pack.
NAME_CAPS='ÀÁÂÃÄÅÆÇÈÉÊËÍÎÏÑÓÔÕÖØŒŠÚÛÜÝŸŽĆČĞĐŁŘ'
NAME_SPLICE='’·'

# Event labels are deliberately separate from the large existing broadcast
# lower-thirds. These small flags are intended for playable decision flows.
EVENT_FLAGS={
 'goal':('GOAL','success','ball'),
 'own_goal':('OWN GOAL','danger','ball'),
 'foul':('FOUL','warn','whistle'),
 'yellow_card':('YELLOW CARD','warn','yellow'),
 'red_card':('RED CARD','danger','red'),
 'second_yellow':('SECOND YELLOW','danger','second_yellow'),
 'corner_kick':('CORNER KICK','ice','corner'),
 'goal_kick':('GOAL KICK','cobalt','goal'),
 'free_kick_direct':('DIRECT FREE KICK','warn','whistle'),
 'free_kick_indirect':('INDIRECT FREE KICK','ice','whistle'),
 'penalty_kick':('PENALTY KICK','warn','spot'),
 'offside':('OFFSIDE','warn','flag'),
 'throw_in':('THROW-IN','ice','up'),
 'advantage':('ADVANTAGE','success','right'),
 'kick_off':('KICK OFF','cobalt','ball'),
 'drop_ball':('DROP BALL','cobalt','down'),
 'substitution':('SUBSTITUTION','ice','swap'),
 'injury_stop':('INJURY STOP','danger','cross'),
 'penalty_scored':('PENALTY SCORED','success','spot'),
 'penalty_missed':('PENALTY MISSED','danger','spot'),
 'decision_overturned':('DECISION OVERTURNED','danger','turn'),
 'decision_confirmed':('DECISION CONFIRMED','success','tick'),
}

PALETTES={
 'success': ('#071e26','#104b43','#2d7460','#8ce7a3','#d9f7c7'),
 'danger':  ('#1a102e','#5e1c3b','#962e4c','#fc7a74','#ffe0bc'),
 'warn':    ('#281522','#684225','#976231','#f4ae42','#fff2b1'),
 'ice':     ('#09182e','#184069','#2f64a2','#82b5ed','#d2edff'),
 'cobalt':  ('#100d37','#26326d','#494c9c','#9ea9ff','#e6eaff'),
 'neutral': ('#171531','#323354','#53587c','#a0a8c5','#ecedff'),
}

SEGMENTS={
 '0':'abcfed','1':'bc','2':'abged','3':'abgcd','4':'fgbc',
 '5':'afgcd','6':'afgecd','7':'abc','8':'abcdefg','9':'abfgcd',
}

def _digit(im,x,y,n,mode):
    # Seven-segment display authored on a physical grid. Drop and highlight
    # share a fixed advance so the live score has no visual jitter.
    d=ImageDraw.Draw(im)
    if mode=='clock':
        cols=('#0b2037','#52bfa6','#c1f3a4','#e8ffd0')
    else:
        cols=('#10163c','#618bda','#aedaff','#e4fbff')
    s={
      'a':[(3,0),(12,0),(14,2),(12,4),(3,4),(1,2)],
      'g':[(3,13),(12,13),(14,15),(12,17),(3,17),(1,15)],
      'd':[(3,26),(12,26),(14,28),(12,30),(3,30),(1,28)],
      'f':[(0,3),(2,5),(2,11),(0,13),(-2,11),(-2,5)],
      'b':[(15,3),(17,5),(17,11),(15,13),(13,11),(13,5)],
      'e':[(0,16),(2,18),(2,24),(0,26),(-2,24),(-2,18)],
      'c':[(15,16),(17,18),(17,24),(15,26),(13,24),(13,18)],
    }
    for k in SEGMENTS[n]:
        p=s[k];d.polygon([(x+a+1,y+b+2) for a,b in p],fill=cols[0])
    for k in SEGMENTS[n]:
        p=s[k]; q=[(x+a,y+b) for a,b in p]
        d.polygon(q,fill=cols[1])
        if k in 'agd':
            d.line((x+3,y+p[0][1]+1,x+12,y+p[0][1]+1),fill=cols[3],width=1)
            d.line((x+2,y+p[0][1]+2,x+13,y+p[0][1]+2),fill=cols[2],width=1)
        else:
            d.line((x+p[0][0]-1,y+p[1][1],x+p[0][0]-1,y+p[2][1]),fill=cols[3],width=1)
            d.line((x+p[0][0],y+p[1][1]-1,x+p[0][0],y+p[2][1]+1),fill=cols[2],width=1)
    if mode=='score':
        for i,p in enumerate((x+3,x+11)):
            d.point((p,y+1),fill=cols[3] if 'a' in SEGMENTS[n] else (0,0,0,0))

def pair(value,mode='clock'):
    """Fixed 46x38 transparent clock HUD sprites, authored at 1x."""
    n=f'{value:02d}'
    img=Image.new('RGBA',(46,38))
    _digit(img,4,3,n[0],mode); _digit(img,25,3,n[1],mode)
    return img

def name_glyph(c):
    """Same logical height, advance, and common baseline for all extended glyphs."""
    c=unicodedata.normalize('NFC',c)
    f=face('roster',26); im=Image.new('RGBA',(21,37)); a=Image.new('L',im.size)
    ImageDraw.Draw(a).text((2,26),c,font=f,anchor='ls',fill=255)
    a=a.point(lambda x:255 if x>=108 else 0)
    mask=a.filter(ImageFilter.MaxFilter(3))
    shadow=Image.new('RGBA',im.size,'#070921');shadow.putalpha(mask);im.alpha_composite(shadow,(1,1))
    liquid=Image.new('RGBA',im.size,'#fffde5');liquid.putalpha(a);im.alpha_composite(liquid)
    return im

def _symbol(d,code,p):
    o,a,m,c,h=p
    def L(coords,fill=h,width=2): d.line(coords,fill=fill,width=width)
    if code=='ball':
        d.ellipse((14,9,28,23),fill='#c8e0d0',outline=h)
        d.polygon([(20,12),(24,12),(26,16),(23,19),(19,17)],fill=o)
        for points in [[(19,15),(15,14)],[(23,18),(23,22)],[(24,13),(27,12)]]:L(points,o,1)
    elif code in ('yellow','red','second_yellow'):
        if code=='second_yellow':
            d.polygon([(15,9),(24,8),(25,21),(16,22)],fill='#e6a52c',outline='#ffe5a4')
        d.polygon([(18,10),(26,9),(28,22),(19,23)],
                  fill='#d73d58' if code!='yellow' else '#f1c546',
                  outline='#ffd6b1' if code!='yellow' else '#fff3be')
    elif code=='whistle':
        d.polygon([(13,13),(30,13),(30,17),(24,17),(22,22),(16,22),(13,19)],fill=c,outline=h)
        d.ellipse((16,16,20,20),fill=o);L([(24,15),(29,15)],o,1)
    elif code in ('corner','flag'):
        L([(15,23),(15,9)]);d.polygon([(16,9),(28,11),(16,16)],fill=c,outline=h)
        if code=='corner': d.arc((15,17,25,27),181,273,fill=h,width=1)
    elif code=='goal':
        d.rectangle((13,11,29,21),outline=h)
        for x in range(16,29,3):L([(x,12),(x,20)],c,1)
        for y in (14,17):L([(14,y),(28,y)],m,1)
    elif code=='spot':
        L([(13,20),(29,20)],h,1); d.arc((13,8,28,25),189,353,fill=c,width=1)
        d.ellipse((19,12,23,16),fill=h)
    elif code=='swap':
        L([(14,12),(27,12)]); L([(24,9),(27,12),(24,15)])
        L([(27,20),(14,20)],c); L([(17,17),(14,20),(17,23)],c)
    elif code=='cross':
        d.rectangle((13,9,28,24),fill=m,outline=c)
        d.rectangle((19,12,22,21),fill=h);d.rectangle((16,15,25,18),fill=h)
    elif code=='tick':
        L([(14,15),(19,20),(28,10)],h,3)
    elif code=='turn':
        L([(14,11),(26,11),(26,21),(17,21)]);L([(20,17),(16,21),(20,25)])
    elif code in ('up','down','right'):
        arr={'up':[(16,17),(21,11),(26,17)],'down':[(16,15),(21,21),(26,15)],'right':[(18,11),(24,16),(18,21)]}[code]
        L(arr,h,2)

def flag(label='',kind='neutral',code=None,width=None,layout='decision'):
    """Compact event flag. No names, team logos, or score text baked in."""
    p=PALETTES[kind]; outer,a,m,c,hi=p
    sprite=None
    if label:
        sprite=text(label,19,'menu')
        sprite=sprite.crop(sprite.getbbox())
    w=width or max(128,63+(sprite.width if sprite else 0))
    h=34 if layout!='timeline' else 32
    im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
    top=[(3,4),(w-18,4),(w-3,13),(w-12,30),(1,30),(1,9)]
    if layout=='timeline': top=[(11,4),(w-10,4),(w-3,10),(w-3,23),(w-10,28),(3,28),(3,10)]
    if layout=='broadcast': top=[(1,3),(w-15,3),(w-3,10),(w-3,23),(w-17,30),(1,30)]
    d.polygon([(min(w-1,x+1),min(h-1,y+3)) for x,y in top],fill='#070921')
    mask=Image.new('L',im.size);ImageDraw.Draw(mask).polygon(top,fill=255)
    base=Image.new('RGBA',im.size,outer); bd=ImageDraw.Draw(base)
    for y in range(5,29):
        cur=a if y<9 or y>=26 else m if y<17 else a
        bd.line((1,y,w-1,y),fill=cur)
        for x in range((y%2)*2+3,w-3,4):
            if y%3!=0:bd.point((x,y),fill=a if cur==m else outer)
    base.putalpha(mask); im.alpha_composite(base);d=ImageDraw.Draw(im)
    d.line(top+[top[0]],fill=c,width=1)
    d.line((7 if layout!='timeline' else 13,5,w-23,5),fill=hi,width=1)
    if layout!='broadcast':
        d.polygon([(9,7),(36,7),(41,14),(32,25),(7,25)],fill=outer,outline=m)
        d.line((40,9,34,23),fill=c,width=1)
    if code:_symbol(d,code,p)
    if sprite:
        im.alpha_composite(sprite,(45,(h-sprite.height)//2))
    return im

def run(a):
    count=0
    for mode in ('clock','score'):
        for value in range(100):
            a.add(f'me_{mode}_led_pair_{value:02d}',pair(value,mode),'Live Number Pairs',
                  note=f'Tabular segmented {mode} value {value:02d}. Transparent fixed 46×38 canvas; no panel baked in.')
            count+=1
    for c in NAME_CAPS+NAME_CAPS.lower()+NAME_SPLICE:
        a.add(f'me_roster_extended_u{ord(c):04x}',name_glyph(c),'Player Name Glyphs',
              note=f'Extended U+{ord(c):04X} {c}. NFC. 21×37 canvas; 16 px advance, baseline y=26; draw with clean pixel filtering.')
        count+=1
    for slug,(lab,kind,code) in EVENT_FLAGS.items():
        a.add(f'me_event_flag_{slug}',flag(lab,kind,code),'Decision Flags',
              note=f'Compact independent {lab} flag. Semantic {kind} colour plus separate visual symbol.')
        count+=1
    for name,kind,layout,w in (
        ('restart','ice','decision',164),('sanction','warn','decision',170),
        ('dismissal','danger','decision',170),('match_flow','success','decision',176),
        ('timeline','neutral','timeline',156),('broadcast','cobalt','broadcast',232),
    ):
        a.add(f'me_event_blank_{name}',flag('',kind,width=w,layout=layout),'Decision Flag Blanks',
              note='Modular blend-free plate, ready for live event text; icon, name, and score are not baked in.')
        count+=1
    assert count==300,count
