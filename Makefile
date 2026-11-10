# CONTRAATAQUE - homebrew para PS2 (ps2sdk + gsKit)
EE_BIN = contraataque.elf
EE_OBJS = main.o gfx.o input.o map.o models.o game.o

GSKIT ?= $(PS2DEV)/gsKit

EE_INCS += -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -lm
EE_CFLAGS += -O2 -Wall

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
