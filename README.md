## About
This is a simple uci chess engine. 
Here I will try measure perfomance after every single update.

## Current goal
Get the best possible perfomance out of **exact** search.

### Checklist
- [x] Puzzle framework!
- [x] Barebones negamax, basic draw detection
- [x] Alpha-beta (staged movegen) + Quiescence (SEE >=0 moves)
- [ ] Add TT to alpha-beta (try w/ and w/out searching TT move first)
- [ ] Iterative deepening
- [ ] Internal iterative deepening
- [ ] Aspiration window (try different strategies)
- [ ] Order root moves based on (prev score, nodes, etc...)
- [ ] Most valueable victim / Least valuable attacker
- [ ] TT move -> Good tactical (MVV/LVA) -> Bad tactical (MVV/LVA) -> Non-tactical
- [ ] Killer move heuristic: TT -> Good -> Killers -> Bad -> Non
- [ ] PSQT non-tactical ordering
- [ ] History heuristic for non-tactical
- [ ] Tune aspiration window
