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
got from ~20 to ~10 (828 -> 8210 -> 82780 -> 595447 -> 5444539 
-> 33194743 @ ~8mnps).
This is huge, especially considering the fact
that we now do some extra work (quiescence search).
Quiescence search handles checks, now it is possible to find
forced checkmate in N in fewer than N depth, and overall thanks
to the qsearch the score is somewhat sane.

