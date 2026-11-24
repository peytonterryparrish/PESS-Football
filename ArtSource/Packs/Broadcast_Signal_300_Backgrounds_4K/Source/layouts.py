# Each row is an explicitly art-directed composition. The ten passes per row
# vary foreground framing, light direction, detail density and hues.
# Coordinates are on the unscaled 1280 × 720 layout.
FAMILIES=[
 ('Terrace_Transmission','Classic Broadcast',[('st_george',-54,-39,345,214),('stand',925,236,407,241),('floodlights',1120,37,166,211),('net',-31,511,245,267),('ball',1052,535,288,249)]),
 ('North_Bank_Live','Classic Broadcast',[('prism',-18,-25,388,174),('floodlights',29,44,165,241),('terrace',948,465,395,239),('ball',1093,568,241,210)]),
 ('Crossbar_Link','Classic Broadcast',[('net',-24,458,268,276),('st_george',970,-46,338,195),('floodlights',1112,122,176,235)]),
 ('Saturday_Signal','Classic Broadcast',[('st_george',-36,-51,363,211),('bars',975,-9,315,57),('stand',973,413,351,207),('ball',1059,556,275,228)]),
 ('Matchday_Circuit','Classic Broadcast',[('stand',-43,420,376,238),('signal',1010,15,302,183),('floodlights',1114,77,151,229),('net',1097,558,237,219)]),
 ('Cobalt_Terrace','Terrace & Floodlights',[('stand',-48,389,379,256),('floodlights',49,90,167,237),('stand',1066,450,230,188)]),
 ('Roofline_Wire','Terrace & Floodlights',[('terrace',-45,473,438,234),('floodlights',1140,47,159,235),('waveform',900,652,386,73)]),
 ('Far_End_Floodlights','Terrace & Floodlights',[('floodlights',-31,16,191,272),('floodlights',1132,16,155,272),('terrace',929,477,371,220)]),
 ('Cantilever_Sweep','Terrace & Floodlights',[('stand',973,300,374,236),('prism',0,530,350,194),('floodlights',1114,62,170,240)]),
 ('Dusk_Concrete','Terrace & Floodlights',[('terrace',-44,480,434,264),('advertising',903,528,414,168),('bars',934,1,350,52)]),
 ('Three_Lions_Signal','English Matchday',[('st_george',-55,-25,380,239),('prism',961,510,379,223),('ball',1041,546,284,232)]),
 ('Supporters_Corner','English Matchday',[('scarf',-41,-34,366,213),('net',1043,531,272,213),('terrace',964,393,365,243)]),
 ('St_George_Diagonal','English Matchday',[('st_george',998,-20,346,218),('waveform',-25,468,301,210),('net',-36,495,258,247)]),
 ('Wool_And_Wire','English Matchday',[('scarf',938,-33,390,212),('floodlights',34,34,157,245),('ball',-32,543,286,233)]),
 ('Away_Day_Foils','English Matchday',[('st_george',-43,-37,338,182),('scarf',935,595,384,183),('bars',1070,22,212,58)]),
 ('Goal_Mouth_Raster','Pitchside Details',[('net',-18,429,309,317),('waveform',968,501,337,196),('floodlights',1133,29,139,226)]),
 ('Full_Time_Leather','Pitchside Details',[('ball',1002,514,346,266),('net',-38,475,228,266),('stand',1024,205,286,233)]),
 ('Seam_And_Touchline','Pitchside Details',[('ball',-23,501,275,265),('advertising',919,519,418,215),('prism',1008,3,316,168)]),
 ('Crossbar_Reflection','Pitchside Details',[('net',1059,465,275,295),('floodlights',9,36,185,217),('waveform',-31,524,393,185)]),
 ('Overhead_Clearance','Pitchside Details',[('removed',983,337,306,338),('net',-28,520,227,248),('bars',36,-4,262,57)]),
 ('Calibration_Plane','Signal Graphics',[('bars',-14,10,341,67),('prism',1010,511,333,229),('floodlights',1135,51,164,205)]),
 ('Waveform_Purple','Signal Graphics',[('waveform',-19,492,375,181),('waveform',965,61,339,186),('terrace',1117,528,158,168)]),
 ('Raster_Stack','Signal Graphics',[('prism',-24,-17,386,264),('prism',1009,496,335,244)]),
 ('Test_Card_Touchline','Signal Graphics',[('bars',967,4,309,79),('waveform',-37,488,387,198),('ball',1094,557,252,198)]),
 ('Cold_Open','Signal Graphics',[('prism',916,6,411,199),('bars',-1,644,335,76),('net',-47,430,281,297)]),
 ('Touchline_Sprint','Quiet Menu Plates',[('removed',-3,392,258,307),('floodlights',1156,45,123,212),('waveform',970,654,311,71)]),
 ('Late_Kickoff','Quiet Menu Plates',[('floodlights',1137,58,151,209),('terrace',938,508,363,218)]),
 ('Subtle_Net','Quiet Menu Plates',[('net',-42,548,250,206),('prism',984,32,317,148)]),
 ('Cobalt_Bands','Quiet Menu Plates',[('bars',1008,6,264,49),('prism',-4,565,397,179)]),
 ('Signal_After_Hours','Quiet Menu Plates',[('waveform',-35,577,356,159),('floodlights',1140,35,127,192)]),
]
# Reposition once an assigned key is known; "signal" means a prism accent.
FAMILIES=[(n,g,[(('prism' if k=='signal' else k),x,y,w,h) for k,x,y,w,h in a]) for n,g,a in FAMILIES]
# Stable color/lighting styles. Retain the purple/blue aesthetic throughout.
PASSES=[
 ('Violet_Live','24114d','33116a','5544df','95baff','d30f45','e5e5ff',.97,.0,0),
 ('Ultramarine','1d124e','272078','354bff','6eccff','d31f53','d9efff',.96,-.21,13),
 ('Blue_Hour','17123c','242268','3f58e0','94c8ff','ae2552','dcf2fb',.82,.19,24),
 ('Violet_Red','271043','391063','5d3de0','aaa7ff','ed1947','f6e7ff',1.02,-.23,34),
 ('Electric_Cyan','181545','271c6f','304bdd','5fcbff','bd2061','e2f8ff',.94,.11,46),
 ('Plum_Broadcast','241046','39105d','6645dd','b1a1ff','ca1b60','ece3ff',.87,-.1,57),
 ('Deep_Indigo','171239','241658','382dbb','86afff','bb1451','d1dfff',.81,.25,68),
 ('Quartz_Violet','201249','332077','5448e5','9eaaff','c92463','eef1ff',.87,-.12,79),
 ('Muted_Cobalt','16143e','282167','385acd','91c2f2','a92558','d7f1ff',.76,.11,91),
 ('Night_Scan','140f33','291454','4742bb','7c96e5','b32157','d6defa',.72,-.22,103),
]
