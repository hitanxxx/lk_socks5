PROJDIR += $(shell pwd)
export PROJDIR

all:
	make -C test  TARGET=test   lib
	make -C base  TARGET=base   lib
	make -C http TARGET=http   lib
	make -C app TARGET=app   lib
	make -C bin bin

clean:
	make -C test TARGET=test   clean
	make -C base TARGET=base   clean
	make -C http TARGET=http   clean
	make -C app  TARGET=app    clean
	make -C bin  clean

.PHONY: test clean base http app bin
