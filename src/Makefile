CROSS_PREFIX ?= 

CC           = $(CROSS_PREFIX)gcc
CXX          = $(CROSS_PREFIX)g++
AS           = $(CROSS_PREFIX)as
AR           = $(CROSS_PREFIX)ar
LD           = $(CROSS_PREFIX)ld
OBJDUMP      = $(CROSS_PREFIX)objdump
OBJCOPY      = $(CROSS_PREFIX)objcopy
STRIP        = $(CROSS_PREFIX)strip
SIZE         = $(CROSS_PREFIX)size

### DEF
ifeq ($(TEST), y)
DEFINE += -DTEST
endif


### INC DIR
INCLUDE  += -I.
INCLUDE  += -I./common/util
INCLUDE  += -I./common
INCLUDE  += -I./dns
INCLUDE  += -I./http
INCLUDE  += -I./app
INCLUDE  += -I./bin
INCLUDE  += -I./test


### SRC DIR
SRCDIR  += .
SRCDIR  += ./common/util
SRCDIR  += ./common
SRCDIR  += ./dns
SRCDIR  += ./http
SRCDIR  += ./app
SRCDIR  += ./bin
SRCDIR  += ./test

CSRCS += $(foreach d, $(SRCDIR), $(wildcard $d/*.c))
CSRCS += $(foreach d, $(SRCDIR), $(wildcard $d/*.cpp))
COBJS += $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(CSRCS)))

### COMPLE FALGS
LDFLAGS += -lcrypto -lssl -lpthread -lm -ldl

CFLAGS += -Wall -ffunction-sections -fdata-sections $(DEFINE) $(INCLUDE)
CFLAGS += -O2
CFLAGS += -fstrict-aliasing -march=native -mtune=native -pipe


.c.o:
	@echo "> $<"
	@$(CC) -c $(CFLAGS)  -o $@ $<
.cpp.o:
	@echo "> $<"
	@$(CXX) -c $(CFLAGS)  -o $@ $<


all: s5

s5: $(COBJS)
	@echo "[CC]:($(CC))"
	@echo "[DEFINE]:($(DEFINE))"
	$(CC) -o $@ $(COBJS) $(LDFLAGS)
	$(STRIP) $@
	@md5sum $@
	@echo "===== compile finish ====="

clean:
	$(RM) s5
	$(RM) $(COBJS)


