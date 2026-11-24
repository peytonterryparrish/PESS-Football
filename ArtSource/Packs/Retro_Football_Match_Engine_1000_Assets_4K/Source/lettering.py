"""Original rasterized typography matching approved matchup effects."""
from functools import lru_cache
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import re, string

F='/usr/share/fonts/truetype/dejavu/DejaVuSans'
FONT={
    'roster':F+'Mono.ttf',
    'number':F+'Condensed-BoldOblique.ttf',
    'vs':F+'Condensed-BoldOblique.ttf',
    'menu':F+'Condensed-Bold.ttf',
    'teal':F+'Condensed-Bold.ttf',
    'rose':F+'Condensed-Bold.ttf',
    'metal':F+'Condensed-BoldOblique.ttf',
    'steel':F+'Condensed-Bold.ttf',
    'volt':F+'Condensed-Bold.ttf',
    'green':F+'Condensed-Bold.ttf',
}
PALETTE={
    'roster':('#fffde5',),
    'number':('#fff598','#f4c842','#fd7332','#e42645'),
    'vs':('#491728','#702139','#b93132','#f86635','#ffb53c','#f06531','#135bcf','#103889'),
    'menu':('#fffde5',),
    'teal':('#fbffed','#e9ffeb','#c9f1da','#85d6bb'),
    'rose':('#fff2e0','#ffe0dc','#f1a8b4','#ed7897'),
    'metal':('#fff1b1','#e9cc73','#e1a13e','#b8662b'),
    'steel':('#f7ffff','#dfecf2','#bad4ea','#96bde0'),
    'volt':('#f7ffcf','#dbf254','#b9ce31','#88b63e'),
    'green':('#e7ffe7','#a7f3a4','#79da83','#55ae71'),
}
@lru_cache(maxsize=30)
def face(style,size):
    return ImageFont.truetype(FONT[style],size)

def text(s, size=25, style='roster', maxw=None, outline=None):
    """Transparent, thresholded text in native logical pixels.
    Styles: roster, number, vs, menu, teal, rose, metal, steel, volt, green.
    All text in one row uses a common top/baseline. No subpixel RGB fringe.
    """
    if not isinstance(s,str) or not s or '\n' in s: raise ValueError('Single nonempty line required')
    size=max(8,int(size)); f=face(style,size)
    x0,y0,x1,y1=f.getbbox(s); asc,des=f.getmetrics()
    cap=f.getbbox('A')[1]
    base=cap if style=='roster' else min(y0,cap)
    y1=max(y1,f.getbbox('Ag')[3])
    pad=5 if style=='vs' else 3
    w=max(1,x1-min(0,x0))+2*pad; h=max(1,y1-base)+2*pad
    a=Image.new('L',(w,h)); ImageDraw.Draw(a).text((pad-min(0,x0),pad-base),s,font=f,fill=255)
    a=a.point(lambda v:255 if v>=108 else 0)
    if maxw and w>maxw: a=a.resize((maxw,h),Image.Resampling.NEAREST);w=maxw
    im=Image.new('RGBA',a.size)
    if outline is None: outline=2 if style=='vs' else 1
    if outline:
        def layer(c,mask,dest=(0,0)):
            under=Image.new('RGBA',a.size,c);under.putalpha(mask);im.alpha_composite(under,dest)
        m=a.filter(ImageFilter.MaxFilter(outline*2+1))
        layer('#070921',m,(2,3) if style=='vs' else (1,1))
        if style=='vs':
            layer('#152650',m,(2,2));layer('#fff4dc',m)
    fg=Image.new('RGBA',a.size)
    d=ImageDraw.Draw(fg)
    colors=PALETTE[style]
    _,ink_top,_,ink_bottom=a.getbbox() or (0,pad,w,h-pad)
    for y in range(h):
        d.line((0,y,w-1,y),fill=colors[min(len(colors)-1,max(0,y-ink_top)*len(colors)//max(1,ink_bottom-ink_top))])
    fg.putalpha(a); im.alpha_composite(fg)
    return im

def _vs(style='classic'):
    word=text('VS',85 if style=='classic' else 58,'vs',maxw=112 if style=='classic' else 86)
    if style=='classic': return word
    if style=='split':
        im=Image.new('RGBA',(146,69)); d=ImageDraw.Draw(im)
        A=[(9,23),(58,19),(74,7),(96,19),(135,23),(127,49),(89,49),(73,61),(58,50),(13,50)]
        d.polygon([(x+2,y+3) for x,y in A],fill='#080719');d.polygon(A,fill='#27358a',outline='#f5e0bc')
        d.polygon([(10,25),(59,21),(62,47),(15,47)],fill='#1337a8')
        d.polygon([(87,20),(133,24),(126,47),(86,48)],fill='#932942')
        d.line((15,26,45,25),fill='#729bff',width=2);d.line((103,25,128,27),fill='#ff977b',width=2)
        im.alpha_composite(word,((im.width-word.width)//2, (im.height-word.height)//2))
        return im
    # Compact head-to-head tag for team strips and replay summaries.
    word=text('VS',30,'vs',maxw=56)
    im=Image.new('RGBA',(88,42));d=ImageDraw.Draw(im)
    d.polygon([(11,3),(75,3),(84,16),(77,36),(9,36),(2,23)],fill='#24143f',outline='#f5dfa8',width=2)
    d.line((9,33,76,33),fill='#1c58bd')
    im.alpha_composite(word,((im.width-word.width)//2,2))
    return im

def _prompt(k):
    im=Image.new('RGBA',(26,30));d=ImageDraw.Draw(im)
    c={'A':'#204c97','B':'#9c2840','X':'#2a6a76','Y':'#80663a','L':'#493879','R':'#493879'}[k]
    d.ellipse((1,3,24,26),fill='#070922',outline='#2a376c',width=2)
    d.ellipse((2,2,23,23),fill=c,outline='#a0acee')
    d.arc((4,4,21,21),195,343,fill='#cdd9fa')
    t=text(k,14,'menu',outline=0)
    t=t.crop(t.getbbox())
    im.alpha_composite(t,((26-t.width)//2,(26-t.height)//2-2))
    return im

def run(a):
    # Two complete Unicode bitmap character sets (visible printable UI glyphs).
    letters=string.ascii_letters+string.digits+"!?.,:;…'+-=<>/\\|()[]&%#*@_£€$–—×↑↓←→"
    for f,style in [('squad','roster'),('score','number')]:
        for s in letters:
            a.add(f'type_{f}_glyph_u{ord(s):04x}',text(s,26 if f=='squad' else 30,style),'Type Glyphs','core',
                  note=f'U+{ord(s):04X} {s}; original bitmap; preserve transparent padding for baseline.')
        # Full digit pairs with mathematically equal-width canvas.
        raw=[text(f'{n:02}',30 if f=='score' else 24,style) for n in range(100)]
        w,h=max(r.width for r in raw),max(r.height for r in raw)
        for n,image in enumerate(raw):
            im=Image.new('RGBA',(w,h)); im.alpha_composite(image,((w-image.width)//2,0))
            a.add(f'type_{f}_pair_{n:02}',im,'Number Pairs','core',note=f'Zero-padded fixed width {f} pair {n:02}.')
    for v in ('classic','split','compact'):
        a.add(f'type_vs_{v}',_vs(v),'VS Titles','core',note='Original warm/cobalt matchup lettering.')
    for k in 'ABXYLR':
        a.add(f'type_prompt_{k.lower()}',_prompt(k),'Input Symbols','core',note=f'Unbranded {k} prompt button.')
    labels=[
        ('match','menu',26,
         'Continue|Squad|Controls|Formation|Camera|Tactics|Shirt No.|Defence|Edit Team|CONFIRM|BACK|1P|2P|CPU|READY|Match Day|Kick Off|Select Clubs|Club Selection|Match Options|Home Team|Away Team|Play Match|Quick Match|Local Derby|Press Start|Load Game|Settings|Edit Kit|Pause|Resume Match|Reset|Save Changes|Exit to Menu'),
        ('state','teal',22,
         'Goal|Replay|Half Time|Full Time|Extra Time|Penalties|Penalties Won|Offside|Throw-In|Goal Kick|Free Kick|Corner Kick|Yellow Card|Red Card|Added Time|Advantage|Stoppage Time|Game Time|Substitution|Own Goal|Starting XI|Bench|Captain|Next Match'),
        ('table','steel',22,
         'League Table|Fixtures|Results|Season Schedule|League|Cup|Quarter-Final|Semi-Final|Final|Champions|Title Race|Promoted|Relegated|Group Stage|Knockout Round|Team Form|Played|Won|Drawn|Lost|Goals For|Goals Against|Goal Difference|Points|Position|Top Scorers|P|W|D|L|GF|GA|GD|PTS'),
        ('roster','roster',23,
         'Goalkeeper|Defender|Midfielder|Forward|Centre Back|Right Back|Left Back|Wing Back|Defensive Midfielder|Attacking Midfielder|Winger|Striker|GK|DF|MF|FW|CB|RB|LB|WB|DM|AM|RW|LW|ST|Sub|Reserves|Overall|Age|Fit|Form|Foot|Height|Weight|Pace|Shooting|Passing|Dribbling|Defending|Physical|Stamina|Control|Handling|Reflexes|Strength'),
        ('transfer','teal',23,
         'Transfer Market|Transfer List|Shortlist|Search Players|Scout Player|Scout Report|Player Profile|Negotiations|Make Offer|Accept Offer|Counter Offer|Reject Offer|Confirm Signing|Free Agent|Loan Deal|Loan List|Transfer Fee|Contract Length|Weekly Wages|Transfer Budget|Wage Budget|Incoming|Outgoing|Transfer Window|Deadline Day|Transfer Complete|Club Balance|Player Value|Player Comparison'),
        ('warning','rose',22,
         'Transfer Listed|Injury|Injured|Unavailable|Suspended|Sent Off|Offer Expired|Offer Rejected|Window Closed|No Funds|Out of Position|Full Squad|Select Player|Cannot Continue'),
        ('success','green',22,
         'Offer Accepted|Deal Agreed|Loan Approved|Ready to Play|Full Fitness|New Record|Unlocked|Completed|Saved|Updated|Qualified'),
    ]
    for group,style,size,words in labels:
        for s in words.split('|'):
            slug=re.sub('[^a-z0-9]+','_',s.lower()).strip('_')
            a.add(f'type_{group}_label_{slug}',text(s,size,style),'Game Labels','core',note=f'Reusable original word sprite: {s}.')
    for n,v in enumerate(('4-4-2','4-3-3','4-2-3-1','4-1-4-1','4-5-1','3-5-2','3-4-3','5-3-2','5-4-1','4-3-1-2','4-4-1-1','3-4-1-2')):
        a.add(f'type_formation_{v.replace("-","_")}',text(v,26,'metal'),'Game Labels','core',note=f'Formation layout: {v}.')
