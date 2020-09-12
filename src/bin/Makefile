
INC_DIR     +=      $(PROJDIR)/base/util
INC_DIR     +=      $(PROJDIR)/base
INC_DIR     += 	    $(PROJDIR)/dns
INC_DIR     += 	    $(PROJDIR)/http
INC_DIR     +=      $(PROJDIR)/app
INC_DIR     +=      $(PROJDIR)/test
INC_DIR     +=      $(PROJDIR)/bin

DEPEND_OBJS     +=      $(PROJDIR)/base/libbase.a
DEPEND_OBJS     +=      $(PROJDIR)/app/libapp.a
DEPEND_OBJS     +=      $(PROJDIR)/test/libtest.a

DIR_BIN         +=      .
APP_BIN		+= 	lks5

#CMP_FLAGS 	+= -O0 -Wall -Wpointer-arith -Wconversion -Winline -Wmissing-prototypes -Wno-unused -Werror -ggdb 
CMP_FLAGS += -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -Werror -g
LD_FLAGS 	+= -lpthread -lresolv -lm -lrt -ldl -lcrypto -lssl

include $(PROJDIR)/build.mk
