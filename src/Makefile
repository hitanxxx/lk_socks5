PROJDIR += $(shell pwd)
export PROJDIR


all:
	@make -C app		TARGET=app 	lib
	@make -C base		TARGET=base	lib
	@make -C bin		bin

.PHONY: test clean
test:
	@make -C app		TARGET=app 	lib
	@make -C base		TARGET=base	lib
	@make -C test		bin

clean:
	@make -C test clean
	@make -C base TARGET=base clean
	@make -C app  TARGET=app clean
	@make -C bin  clean
