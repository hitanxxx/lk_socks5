CC 		:= gcc
AR		:= ar
STRIP		:= strip


INC_FLAGS 	+= $(addprefix -I, $(INC_DIR))

SRC_FILES 	+= $(wildcard *.c)
SRC_FILES 	+= $(foreach dir, $(SRC_DIR), $(wildcard $(dir)/*.c))
SRC_FILES       += $(foreach dir, $(SRC_DIR), $(wildcard $(dir)/*.cpp))
SRC_FILES       += $(foreach dir, $(SRC_DIR), $(wildcard $(dir)/*.cc))
SRC_OBJS        += $(patsubst %.c, %.o, $(patsubst %.cpp, %.o, $(patsubst %.cc, %.o, $(SRC_FILES) ) ) )

STATIC_LIB	+= lib$(TARGET).a
APP_BIN		+= $(TARGET)

CMP_FLAGS   += -Wall -O2 -ffunction-sections -fdata-sections
LD_FLAGS    += 

%.o:%.c
	$(CC) $(CMP_FLAGS) $(DEF_FLAGS) $(INC_FLAGS) -c -o $@ $<


bin:$(SRC_OBJS)
	$(CC) -o $(DIR_BIN)/$(APP_BIN) $^ $(DEPEND_OBJS) $(PREBUILD_OBJS) $(LD_FLAGS)
	$(STRIP) $(DIR_BIN)/$(APP_BIN)


lib:$(SRC_OBJS)
ifneq ($(SRC_OBJS), )
	$(AR) -rcs $(STATIC_LIB) $^ $(PREBUILD_OBJS) 
endif


.PHONY: clean all 
clean:
	-rm -rf $(SRC_OBJS)
	-rm -rf $(STATIC_LIB)
	-rm -rf $(APP_BIN)
