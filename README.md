## About
This is a uci chess engine, currently estimated to be around 3000 elo w/ the latest NNUE version at fast time control. The version with a simple PSQT eval is 2706 elo at CCRL at the time of writing. NNNUE weights and selfplay data can be found [here](https://huggingface.co/hrtdind).

## TODO
- Refactor this utter mess, add comments and such.
- Fix an extremely rare crash which happens once a billion games or something. 
Currently have absolutely zero clue why it happens.
- Find out the reason of accidental selfplay data corruption due to an illegal move in a chain. 
Possibly related to the previous issue.
- Investigate TT aging and its effect on the playing strength. A very brief test so far resulted in 0 elo difference.
- Better NNUE architecture
- Better NNUE eval range (currently it's soft capped to [-1500; 1500] or something like that)
- Optional BMI2 movegen
- Make sure SSSE3 version werks on an old cpu
- Lazy SMP
- Revisit move ordering stuff
- Singular extension
