INC_DIR         +=      $(PROJDIR)/app
INC_DIR         +=      $(PROJDIR)/base/util
INC_DIR         +=      $(PROJDIR)/base

INC_DIR 	+= 	$(PROJDIR)/http
INC_DIR 	+= 	$(PROJDIR)/lib/include
INC_DIR 	+= 	$(PROJDIR)/lib/crypto

DEPEND_OBJS     +=      $(PROJDIR)/base/libbase.a
DEPEND_OBJS     +=      $(PROJDIR)/app/libapp.a

DIR_BIN         +=      .
APP_BIN		+= 	lks5

CMP_FLAGS 	+= -fomit-frame-pointer -O -W -Wall -Wpointer-arith -Wconversion -Winline -Wmissing-prototypes -Wno-unused -Werror -g
LD_FLAGS 	+= -lpthread -lresolv -lm -lrt -ldl -lcrypto -lssl

include $(PROJDIR)/build.mk
