"""250 original functional labels and tokens; no game names or branding."""
from pathlib import Path
import re
import sys
from PIL import Image, ImageDraw
from lettering import text

# Nine unique screen vocabularies. Each string targets a real interface state,
# field, or button, rather than filling the pack with recolours.
GROUPS = {
 'negotiation': ('teal', """
Opening Bid
Minimum Bid
Asking Price
Preferred Fee
Upfront Payment
Fee in Instalments
Sell-on Clause
Buy-back Clause
Player Exchange
Bid Submitted
Bid Withdrawn
Awaiting Club Reply
Awaiting Player Reply
Renew Offer
Submit Proposal
Withdraw Bid
Terms Accepted
Talks Stalled
Fee Agreed
Move Collapsed
Medical Pending
Register Signing
Loan Extension
Recall from Loan
Reject All Bids"""),
 'contracts': ('steel', """
Current Deal
Proposed Deal
Contract Starts
Contract Ends
Years Remaining
Renew Contract
Release Clause
Signing Bonus
Appearance Bonus
Goal Bonus
Clean Sheet Bonus
Promotion Bonus
Wage Rise
Squad Role
Negotiation Status
First Team Regular
Rotation Option
Breakthrough Prospect
Release Player
Release Scheduled
Deal Terminated
Contract Renewed
Set Wage Limit
Register Squad
Registration Confirmed"""),
 'league': ('steel', """
Division Overview
Division Leaders
League Record
Recent Results
Games in Hand
Home Record
Away Record
Unbeaten Run
Current Streak
Highest Finish
Best Defence
Best Attack
Title Clinched
Title Decider
Promotion Places
Promotion Secured
Play-off Places
Play-off Final
Relegation Zone
Relegation Confirmed
Points Deduction
Season Review
Head-to-Head
League Split
Match Postponed"""),
 'calendar': ('menu', """
Calendar
Today
Tomorrow
This Week
Next Week
This Month
Last Month
Previous Day
Next Day
Previous Month
Next Month
Jump to Date
Advance Day
Advance to Fixture
Simulate Week
Training Day
Recovery Day
Travel Day
Rest Day
Match Week
Kick-off Time
International Break
Winter Break
Pre-season Starts
Season Ends"""),
 'medical': ('rose', """
Medical Centre
Injury Report
Injury Risk
Injury History
Medical Assessment
Expected Return
Full Clearance
Light Training Only
Non-contact Training
Out for Season
Day to Day
In Rehabilitation
Treatment Started
Treatment Complete
Fitness Test
Fitness Test Passed
Fitness Test Failed
Match Fitness
Fatigue Warning
Knock
Muscle Strain
Ankle Sprain
Concussion Protocol
Cannot Be Selected
Return to Squad"""),
 'training': ('green', """
Training Ground
Plan Session
Session Focus
Session Intensity
Training Load
Training Attendance
Individual Plan
Group Session
Tactical Session
Set-piece Practice
Finishing Drill
Pressing Drill
Marking Drill
Ball Control Drill
Goalkeeping Drill
Recovery Session
Conditioning
Session Complete
Training Progress
Development Report
Development Target
Attribute Gain
Attribute Loss
Potential Growth
Adjust Workload"""),
 'dashboard': ('menu', """
Club Overview
Board Objectives
Objective Progress
Season Targets
Manager Rating
Supporter Mood
Financial Summary
Monthly Income
Monthly Expenses
Net Spend
Ticket Revenue
Gate Receipts
Sponsor Revenue
Operating Costs
Projected Balance
Recent Activity
Unread Messages
Club News
Inbox
Squad Overview
Form Guide
Stadium Capacity
Average Attendance
Supporter Confidence
Club Milestones"""),
 'scouting': ('teal', """
Scouting Network
New Assignment
Staff Overview
Scout Assignment
Assignment Complete
Preferred Position
Preferred Foot
Scouting Coverage
Scouting Range
Current Ability
Potential Ability
Recommendation
Report Pending
Report Complete
Report Expired
Areas of Strength
Areas to Improve
Player Development
Contract Situation
Estimated Wage
Estimated Fee
Availability
Watch Next Match
End Assignment
Add to Watchlist"""),
 'tactics': ('metal', """
Tactical Review
In Possession
Out of Possession
In Transition
Defensive Line
Pressing Intensity
Line of Engagement
Attacking Width
Defensive Width
Passing Directness
Build-up Play
Final Third
Counter Attack
Counter Press
Hold Shape
Drop Back
Overlapping Run
Underlapping Run
Play Wider
Play Narrower
Set-Piece Takers
Corner Taker
Penalty Taker
Free Kick Taker
Save Tactic""")
}

# Vector primitives deliberately drawn on a 20px grid, no sampled pixels.
# Blue/teal/amber match existing highlights; red denotes destructive states.
def icon(name,tone='teal'):
    im=Image.new('RGBA',(28,28)); d=ImageDraw.Draw(im)
    fill,light,shade={
      'teal':('#176f73','#d4f3d6','#0a3c54'),
      'ice':('#223c92','#c9ecff','#0b2355'),
      'amber':('#965525','#ffe2a1','#4b263c'),
      'red':('#872540','#ffe0bd','#4d173e'),
      'green':('#296a42','#ddf6c9','#103c45'),
    }[tone]
    def L(*coords,color=light,width=2): d.line(coords,fill=color,width=width)
    def P(points,color=fill): d.polygon(points,fill=color,outline=light)
    def R(box,color=fill,w=1): d.rectangle(box,fill=color,outline=light,width=w)
    def E(box,color=fill,w=1): d.ellipse(box,fill=color,outline=light,width=w)
    if name in {'weekday_calendar','calendar_plus','calendar_rewind','calendar_lock'}:
        R((4,6,23,23));d.rectangle((5,7,22,10),fill=shade)
        L(8,4,8,8);L(19,4,19,8)
        if name=='weekday_calendar':
            for y in [13,18]:
                for x in [8,13,18]: d.rectangle((x,y,x+2,y+2),fill=light)
        elif name=='calendar_plus':
            L(9,16,19,16);L(14,12,14,20)
        elif name=='calendar_rewind':
            L(8,17,19,17);L(12,13,8,17,12,21)
        else:
            R((10,15,18,20),shade);d.arc((11,10,17,17),180,359,fill=light,width=1)
    elif name in {'transfer_return','transfer_exchange','transfer_hold'}:
        L(5,8,22,8);L(17,3,22,8,17,13)
        if name=='transfer_return':
            L(5,14,5,22,22,22);L(17,17,22,22,17,25)
        elif name=='transfer_exchange':
            L(22,20,5,20);L(10,15,5,20,10,25)
        else:
            R((10,15,12,23));R((16,15,18,23))
    elif name in {'contract_unsigned','contract_renewal','contract_expires'}:
        P([(6,4),(17,4),(22,9),(22,24),(6,24)])
        L(17,4,17,9,22,9,width=1); L(9,12,18,12,width=1); L(9,15,17,15,width=1)
        if name=='contract_unsigned': L(9,20,19,20,width=1)
        elif name=='contract_renewal': L(9,20,12,23,20,17)
        else: L(13,17,13,20);d.point((13,22),fill=light)
    elif name in {'medical_clearance','medical_recovery','medical_watch'}:
        if name=='medical_clearance':
            P([(5,7),(13,4),(22,7),(22,16),(18,22),(13,25),(8,22),(5,16)])
            L(9,15,12,18,18,11)
        elif name=='medical_recovery':
            R((4,5,23,22)); L(5,15,9,15,12,9,15,18,18,12,22,12,width=1)
        else:
            E((5,5,22,22)); L(13,8,13,13,18,13)
    elif name in {'training_load','training_cones','training_target'}:
        if name=='training_load':
            L(3,14,24,14);R((6,9,9,19));R((18,9,21,19));R((3,11,5,17));R((22,11,24,17))
        elif name=='training_cones':
            P([(3,21),(8,8),(13,21)]);P([(15,21),(19,10),(24,21)])
            L(6,17,10,17,width=1);L(17,17,21,17,width=1)
        else:
            E((4,4,23,23));E((8,8,19,19),shade);E((12,12,15,15))
            L(22,6,14,13,width=1)
    elif name in {'league_playoff','league_gap','league_form'}:
        if name=='league_playoff':
            L(5,5,11,5,11,12,5,12,width=1);L(5,16,11,16,11,23,5,23,width=1)
            L(11,8,16,8,16,20,11,20,width=1);L(16,14,23,14,width=1)
        elif name=='league_gap':
            L(6,5,6,22);L(4,8,6,5,9,8);L(4,19,6,22,9,19)
            R((12,7,23,10));R((12,18,19,21))
        else:
            R((4,5,23,22));L(6,16,9,12,13,16,17,9,21,12,width=1)
    elif name in {'scout_route','scout_coverage','scout_estimate'}:
        if name=='scout_route':
            L(7,21,12,15,19,16,21,6,width=1);E((4,18,9,23));E((18,3,23,8))
        elif name=='scout_coverage':
            E((3,4,23,24),shade);d.arc((7,8,19,20),0,359,fill=light)
            L(13,14,21,7,width=1);d.point((10,17),fill=light);d.point((16,12),fill=light)
        else:
            E((4,4,18,18));L(15,16,23,24,width=3);L(9,10,13,10,width=1);d.point((11,14),fill=light)
    elif name in {'congestion','add_objective','attendance'}:
        if name=='congestion':
            for x,y in [(5,5),(12,5),(19,5),(5,13),(12,13),(19,13),(5,21),(12,21),(19,21)]:
                R((x,y,x+4,y+3))
        elif name=='add_objective':
            R((5,5,20,24));R((9,3,16,7),shade);L(9,15,16,15,width=1);L(12,12,12,18,width=1)
            E((17,16,25,24),shade);L(19,20,23,20,width=1);L(21,18,21,22,width=1)
        else:
            for x in (6,13,20):
                E((x-1,9,x+2,12));R((x-2,14,x+3,18))
            L(4,20,24,20,width=1)
    else: raise ValueError(name)
    return im

ICONS = {'weekday_calendar':'ice','calendar_plus':'green','calendar_rewind':'ice','calendar_lock':'amber',
         'transfer_return':'teal','transfer_exchange':'teal','transfer_hold':'amber',
         'contract_unsigned':'ice','contract_renewal':'green','contract_expires':'red',
         'medical_clearance':'green','medical_recovery':'teal','medical_watch':'amber',
         'training_load':'ice','training_cones':'amber','training_target':'teal',
         'league_playoff':'ice','league_gap':'amber','league_form':'green',
         'scout_route':'teal','scout_coverage':'ice','scout_estimate':'amber',
         'congestion':'red','add_objective':'green','attendance':'ice'}

def run(a):
    seen=set()
    for group,(style,rows) in GROUPS.items():
        entries=rows.strip().splitlines()
        assert len(entries)==25,(group,len(entries))
        for word in entries:
            assert word.casefold() not in seen, word
            seen.add(word.casefold())
            key=re.sub('[^a-z0-9]+','_',word.casefold()).strip('_')
            a.add(f'xtype_{group}_{key}',text(word,size=22,style=style),
                  'Functional Labels','core',note=f'{group.title()} label: {word}.')
    for name,tone in ICONS.items():
        a.add(f'xtype_token_{name}',icon(name,tone),'Functional Symbols','core',
              note=f'Unbranded UI token: {name.replace("_"," ")}.')
    assert len(seen)+len(ICONS)==250
