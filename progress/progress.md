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
No significat improvements in puzzles due to the absence 
of iterative deepening. For each depth the TT was cleared. 
Below are the node counts for each depth.
| depth |    no TT    |  TT no ttm |  TT + ttm  |
| ----- | ----------- | ---------- | ---------- |
|     1 |          20 |         20 |         20 |
|     2 |         428 |        428 |        428 |
|     3 |       4'438 |      4'438 |      4'438 |
|     4 |      44'802 |     42'863 |     42'869 |
|     5 |     334'973 |    282'702 |    280'478 |
|     6 |   3'073'695 |  2'074'516 |  2'025'382 |
|     7 |  19'273'822 | 10'528'642 | 10'246'249 |
|     8 | 178'770'818 | 71'989'817 | 68'744'120 |

