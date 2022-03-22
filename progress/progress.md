# Progress records
## Format

The following info must be specified for each version:
1. Test positions results (% correct, nodes, time, avg nps, avg depth)
2. Self-play elo gain (w/ reasonable error)
3. Previous opponents elo difference
4. New opponents (if suitable) elo difference
5. New elo = min(self play, other engines)

## TODO
Add tables w/ info!

## 1. Negamax
Nothing special. Sucks because of big branching factor.
Passes all mate2 tests. Absence of quiescence search hurts a lot as well...

## 2. Alpha-beta, quiescience + tacticals -> nontacticals
In the starting position the effective branching factor (EBF)
got from ~20 to ~10 (20 -> 428 -> 4438 -> 44802 -> 
334973 -> 3073695 -> 19273822 @ ~6mnps).
This is huge, especially considering the fact
that we now do some extra work (quiescence search).
Quiescence search handles checks, now it is possible to find
forced checkmate in N in fewer than N depth, and overall thanks
to the qsearch the score is somewhat sane.

## 3. Alpha-beta with TT
No significant improvements in puzzles due to the absence 
of iterative deepening and relatively low depth. 

## 4. Iterative deepening
Huge gains thanks to TT and such. 
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
|     7 |  19'273'822 | 10'528'642 | 10'246'249 |  1'175'187 |
|     8 | 178'770'818 | 71'989'817 | 68'744'120 |  5'518'337 |


