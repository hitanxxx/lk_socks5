PROJDIR += $(shell pwd)
export PROJDIR


all:
	@make -C base		TARGET=base	lib
	@make -C app		TARGET=app	lib
	@make -C bin		bin

.PHONY: test clean
test:
	@make -C base		TARGET=base	lib
	@make -C app		TARGET=app	lib
	@make -C test		bin

clean:
	@make -C test clean
	@make -C base TARGET=base clean
	@make -C app  TARGET=app clean
	@make -C bin  clean
