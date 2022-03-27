# Progress records

## 1. Negamax
Nothing special. Sucks because of big branching factor.
Passes all mate2 tests. Absence of quiescence search hurts a lot as well...

## 2. Alpha-beta, quiescience + tacticals -> nontacticals
In the starting position the effective branching factor (EBF)
got from ~20 to ~10 (20 -> 428 -> 4438 -> 44802 -> 
334973 -> 3073695 -> 19273822 @ ~6mnps).
This is a huge gain, especially considering the fact
that we now do some extra work (quiescence search).
Quiescence search handles checks, now it is possible to find
forced checkmate in N in fewer than N depth, and overall thanks
to the qsearch the score is somewhat sane.

## 3. Alpha-beta with TT
No significant improvements in puzzles due to the absence 
of iterative deepening and relatively low depth. 

## 4. Iterative deepening + internal iterative deepening
Huge gains thanks to TT and such. IID boots move ordering.
It counts one more node for no reason, but I'm too lazy to fix this.
(move the increment into the cycle).


| depth |    no TT    |  TT no ttm |  TT + ttm  |     ID     |
| ----- | ----------- | ---------- | ---------- | ---------- |
|     1 |          20 |         20 |         20 |         21 |
|     2 |         428 |        428 |        428 |        130 |
|     3 |       4'438 |      4'438 |      4'438 |       1091 |
|     4 |      44'802 |     42'863 |     42'869 |      5'453 |
|     5 |     334'973 |    282'702 |    280'478 |     25'972 |
|     6 |   3'073'695 |  2'074'516 |  2'025'382 |    134'662 |
|     7 |  19'273'822 | 10'528'642 | 10'246'249 |    895'180 |
|     8 | 178'770'818 | 71'989'817 | 68'744'120 |  3'877'582 |

## 5.1 Aspiration window
Nothing special. Lowers amount of nodes a little bit.

## 5.2 Root move ordering
Puzzles did not improve significantly; 
nodes in quiet positions did reduce, although not everywhere...
Sometimes it's better to sort by nodes rather than by score,
but this needs to be tested...
Fail-high-firt / fail-high ratio did improve actually.

## 6. MVV/LVA, killers + PSQT for quiet moves
The biggest move ordering boost came from killers (unsuprisingly)
and PSQT for quit moves since there was no quiet move ordering at all.
Startpos: depth 8 - 1'849'984 nodes, depth 10 - 27'370'459;
With history + countermove+ followup:
Startpos: depth 8 - 1'439'975 nodes, depth 10 - 21'792'585, fhf = 0.988
Marginally improved quiet move ordering by adding a 
bonus to quite TT move, that fails high. And yet another tiny improvement
- PVS at root.

## 7. Null move pruning
Can go 1-2 plies deeper. 100 games @ 20+1.5 suggest it's 100+ elo, LOS = 100%.

## 8. PVS + Late move reduction, check extension, IID -> IIR
### 8.1 Late move reduction
2-3 plies deeper, although that's somewhat misleading due to the nature of LMR.
150 games @ 20 + 0.5 suggest it's 152 +/- 49.3 elo with LOS=100%.

### 8.2 Check extension, IID -> IIR
Check extension helps solving puzzles and deals with checking sequences
that won't let us see the quiet position. Replaced IID w/ simple depth reduction
by one ply if there is no TT move.
620 games @ 20+0.5 -- 36.6 +/- 20.8 elo, LOS=100%

## 9. Forward pruning
### 9.1 Delta pruning
1000 games @ 20+0.5 -- 18.8 +/- 16.1 elo, LOS 98.9%
### 9.2 Reverse futility pruning
174 games @ 20+0.5 -- 100.6 +/- 38 elo, LOS 100%
### 9.3 Late move pruning
210 games @ 20+0.5 -- 63.6 +/- 36.1 elo, LOS 100%

### 9.4 Futility pruning and razoring
Seemed to loose elo, although there was no serious testing involved.

## 10. History improvement and LMR fix
### 10.1 History heuristic improvements
Switched [Piece, To] -> [Color, From, To] + added penalty for quiet moves
that failed to cause a cutoff. 1000 games @ 20+0.5 -- 32.4 +/- 16.3 elo, LOS 100%

### 10.2 LMR Fix
Accidentaly removed main LMR reduction, and all this time
only adjustments were applied. Returning it resulted in elo gain.
1000 games @ 20+0.5 -- 73.3 +/- 16.2, LOS: 100.0 %

