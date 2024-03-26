# this is stupid, but whatever

all:
	mkdir -p build
	cd build; EVALFILE=$(EVALFILE) && cmake ../ && cmake --build . --config Release --parallel; cp saturn ../${EXE}
