TARGET = PokemonArena2D
OBJS = src/main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lm -lpspdebug -lpspkernel

BUILD_PRX = 1
PSP_FW_VERSION = 660
PSP_LARGE_MEMORY = 0
PSP_EBOOT_TITLE = Pokemon Arena 2D

EXTRA_TARGETS = EBOOT.PBP
PSPSDK ?= $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
