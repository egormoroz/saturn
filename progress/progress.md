# Progress records
## Format

The following info must be specified for each version:
1. Test positions results (% correct, nodes, time, avg nps, avg depth)
2. Self-play elo gain (w/ reasonable error)
3. Previous opponents elo difference
4. New opponents (if suitable) elo difference
5. New elo = min(self play, other engines)

## 1. Negamax

Nothing special. Sucks because of big branch factor.
Passes all mate2 tests (except the one with KNkbp, because
after capturing the pawn the position is considered drawn material-wise
(even though it's mate in two plies)). 
Absence of quiescence search hurts a lot as well...
