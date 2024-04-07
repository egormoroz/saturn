## About

[Challenge me on Lichess!](https://lichess.org/@/xxSaturnxx)

A UCI chess engine. NNUE weights and selfplay data can be found [here](https://huggingface.co/hrtdind).

## Rating

CCRL 40/15 ELO changelog. Questionmark means the elo is my rough estimate at STC.

| Version           | ELO  | Most notable change                        |
| ----------------- | ---- | ------------------------------------------ |
| 1.2               | 3275 | horizontally mirrored NNUE, much more data | 
| 1.1               | 3087 | HalfKP NNUE                                | 
| 1.0               | 2706 | First version with PSQT eval               | 

## TODO
- Optional BMI2 movegen
- Search parameters tuning
- Smarter time management
- Lazy SMP
- EGTBs
- Revisit move ordering stuff

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
