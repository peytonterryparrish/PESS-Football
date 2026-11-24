# Club prestige tiers

These are subjective game-design assignments, not official league rankings, current club ratings, a precise 2005–06 power rating, or legal clearance of fictional names. They balance stature at the 2005–06 snapshot, sustained visibility, and historical club profile. Division influences tier but does not determine it. The bonus Wimbledon slot is a historical reference rather than a 2005–06 league member and is counted once.

| Rank | ID | In-game descriptor |
|---|---|---|
| 5 | T5 | Elite |
| 4 | T4 | Major Heritage |
| 3 | T3 | High Profile |
| 2 | T2 | Established |
| 1 | T1 | Grassroots |

The prestige tier is stable by club ID and source slug. Fictional names are the previously established candidate game crosswalk. Keep internal source names/slugs separate from shipped fictional content if needed.

Use `home_primary_hex` for home seat tint at runtime, not the prestige tier. It is the literal primary Home shirt value from the existing R08 historical kit palette metadata.
