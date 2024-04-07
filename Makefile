# this is stupid, but whatever

ifndef EVALFILE
	EVALFILE=default.nnue
endif
ifndef EXE
	EXE=saturn
endif

all:
	mkdir -p build
	cp ./default.nnue build/
	cd build; EVALFILE=$(EVALFILE) && cmake ../ && cmake --build . --config Release --parallel; cp saturn ../${EXE}

clean:
	rm -r ./build
	rm ./${EXE}
