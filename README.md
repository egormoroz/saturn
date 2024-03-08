## About
This is a uci chess engine, currently estimated to be around 3000 elo w/ the latest NNUE version at fast time control. The version with a simple PSQT eval is 2706 elo at CCRL at the time of writing. NNNUE weights and selfplay data can be found [here](https://huggingface.co/hrtdind).

## TODO
- Refactor this utter mess, add comments and such.
- Merge search_root and search into a single function to benefit from reductions and such (e.g. reducing late moves in root by 1 gains a decent amount of elo).
- Investigate TT aging and its effect on the playing strength. A very brief test so far resulted in 0 elo difference.
- Better NNUE architecture
- Better NNUE eval range (currently it's soft capped to [-1500; 1500] or something like that)
- Optional BMI2 movegen
- Make sure SSSE3 version werks on an old cpu
- Lazy SMP
- Revisit move ordering stuff
- Singular extension

## Building using cmake
Requires C++17 and popcnt intrisincs.
Go into source folder.
```
mkdir build
cd build
cmake ../
```
Then either:
```
cmake --build . --config Release
```
or go to the build folder and open the generated visual studio solution.

## Running
It is recommended to use a gui that supports the uci protocol (e.g. Arena, Cute Chess).
Alternatively, you can run it in console and type uci commands yourself.
