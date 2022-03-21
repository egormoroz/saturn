## About
This is a simple uci chess engine. 
Here I will try measure perfomance after every single update.

## Current goal
Get the best possible perfomance **exact** search.

### Checklist
0.  [ ] Puzzle framework!
1.  [ ] Barebones negamax, basic draw detection
2.  [ ] Alpha-beta (staged movegen) + Quiescence (SEE >=0 moves)
3.  [ ] Add TT to alpha-beta (try w/ and w/out searching TT move first)
4.  [ ] Iterative deepening
5.  [ ] Internal iterative deepening
6.  [ ] Aspiration window (try different strategies)
7.  [ ] Order root moves based on (prev score, nodes, etc...)
8.  [ ] Most valueable victim / Least valuable attacker
9.  [ ] TT move -> Good tactical (MVV/LVA) -> Bad tactical (MVV/LVA) -> Non-tactical
10. [ ] Killer move heuristic: TT -> Good -> Killers -> Bad -> Non
11. [ ] PSQT non-tactical ordering
12. [ ] History heuristic for non-tactical

100. [ ] Tune aspiration window
