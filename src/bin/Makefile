INC_DIR         +=      $(PROJDIR)/app
INC_DIR         +=      $(PROJDIR)/base/util
INC_DIR         +=      $(PROJDIR)/base
INC_DIR 	+= 	$(PROJDIR)/lib/include
INC_DIR 	+= 	$(PROJDIR)/lib/crypto

DEPEND_OBJS     +=      $(PROJDIR)/base/libbase.a
DEPEND_OBJS     +=      $(PROJDIR)/app/libapp.a

PREBUILD_OBJS 	+= 	$(PROJDIR)/lib/libssl.a
PREBUILD_OBJS	+=	$(PROJDIR)/lib/libcrypto.a

DIR_BIN         +=      .
APP_BIN		+= 	lks5

CMP_FLAGS 	+= -fomit-frame-pointer -O -W -Wall -Wpointer-arith -Wconversion -Winline -Wmissing-prototypes -Wno-unused -Werror -g
LD_FLAGS 	+= -lpthread -lresolv -lm -lrt -lz -ldl

include $(PROJDIR)/build.mk

