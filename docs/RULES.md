# Laws of the Game in Proper Football (v0.7)

The owner asked for the game to follow the real rules of football. Sources:
- the overview at soccerblade.com/rules-of-soccer (the 17 Laws: keeper handling, back-pass, fouls and cards, restarts, ball in and out of play);
- the IFAB Laws of the Game 2025/26, for the numbers that page leaves out (9.15 m, the penalty area, the 8-second keeper rule, offside).

Code: `Source/ProperFootball/Private/PFMatch.cpp`, the "laws of the game" section and the places noted below.

## Goalkeepers (Law 12)
| Law | In the game |
|---|---|
| A keeper may handle the ball only inside his own penalty area (16.5 m deep, 40.32 m wide). | `InOwnBox` and `CanHandle`. Outside the area he plays the ball with his feet like an outfielder: he passes or clears it. Shots outside the area are only blocked; he can't catch them. |
| A keeper holding the ball may not be challenged. | `KeeperHolding()`. Nobody presses or tackles him, and a slide tackle that misses the ball can't knock it loose. A slide into him while he holds it is a foul, given as an indirect free kick. Opponents drop back out of his area. |
| A keeper who holds the ball for more than 8 seconds concedes a corner (2025/26 law). | `HoldT`. The AI keeper holds it for 1.4 to 2.6 s, so the limit only catches long holds. |
| Back-pass: he may not handle a ball deliberately kicked to him by a team-mate, or thrown to him from a throw-in. | `CanHandle` checks how the ball last moved. Headers, chest passes, deflections and blocks are allowed. He also can't pick the ball up again after releasing it. |
| Gathering the ball. | Inside his area he takes a ground ball into his arms with a scoop (`gk_pickup`), catches balls at chest height, and claims crosses. The ball stays in his hands until he throws it or kicks it out of his hands. |

## Offside (Law 11)
- **When it's judged:** at the moment a team-mate plays the ball (`MarkOffside`).
- **Offside position:** in the opponents' half, and nearer their goal line than both the ball and the second-last opponent. Level counts as onside.
- **When it's an offence:** only when that player is the next to play the ball before an opponent does. An opponent having the ball resets it.
- **Exceptions:** there is no offside from a throw-in, corner kick or goal kick.
- **Decision:** an indirect free kick to the defending team. The assistant referee on that half raises his flag.
- **AI behaviour:** attackers time their runs to stay level with the line, and forwards gamble on it a little. Passers avoid team-mates who are offside, the better passers more reliably.
- **In AI-only matches nobody is ever caught offside.** The AI back line always sits at least 4 m behind the ball, so no attacker gets beyond it. A human-controlled attacker can still be flagged.

## Fouls and cards (Law 12)
| Situation | Decision |
|---|---|
| Tackle from behind, or the man before the ball | Free kick, or a penalty inside the area. |
| Caution | More likely for a tackle from behind. |
| Second caution | Sending-off ("SECOND YELLOW - RED CARD"). |
| Denying an obvious goal-scoring opportunity outside the area (the fouled player was within 25 m of goal and no defender but the keeper was nearer it) | Red card. |
| Impeding or challenging the keeper | Indirect free kick. |
| Offside | Indirect free kick. Taken from the goal-area line if the offence was inside the goal area. |

## Restarts (Laws 13 to 17)
- **Free kick and corner:** opponents at least 9.15 m from the ball (`EnforceRestartDistances`).
- **Defensive wall:** for a free kick within 32 m of goal, 2 to 4 defenders line up 9.15 m out, on the line between the ball and the goal.
- **Throw-in:** opponents at least 2 m away. A goal can't be scored directly from one; if the ball goes straight in, it's a goal kick (or a corner if it went into the thrower's own goal).
- **Goal kick:** kicked off the ground (no longer a punt from the hands), with opponents outside the penalty area until it's taken.
- **Penalty:** everyone except the taker and the defending keeper stays outside the area and the arc (9.15 m from the spot).
- **Indirect free kick:** a goal scored straight from it doesn't count.

## Match balance after the rules (headless AI matches, `-pfsimtest`)
| | v0.6 | v0.7 |
|---|---|---|
| Goals per match | 3.40 | 2.53 |
| Shots per match | 28.0 | 18.1 |
| Goals scored after taking the ball off a keeper | about 12 per 20 matches (the bug the owner saw) | 0 |

v0.7 also changed how shots are blocked:
- A fast shot is no longer controlled by a defender standing up to 0.85 m away. A body has to be right in its path, and the shot cannons off him.
- AI shooters wait for a clear sight of goal and aim for the corner away from the keeper.
