# Brick Breaker Hit — Makefile (uses w64devkit bundled with raylib)

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -mwindows
TARGET  = BrickBreakerHit
SRC     = src/main.c

ifeq ($(OS),Windows_NT)
    # Use w64devkit gcc + bundled raylib (avoids UCRT DLL issues)
    W64DEV      = C:/raylib/w64devkit
    RAYLIB_DIR  = C:/raylib/w64devkit/x86_64-w64-mingw32
    CC          = $(W64DEV)/bin/gcc
    RAYLIB_INC  = $(RAYLIB_DIR)/include
    RAYLIB_LIB  = $(RAYLIB_DIR)/lib/libraylib.a
    LIBS        = $(RAYLIB_LIB) -lwinmm -lgdi32 -lopengl32
    EXT         = .exe
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Darwin)
        RAYLIB_INC = /usr/local/include
        LIBS       = -lraylib -framework IOKit -framework Cocoa -framework OpenGL
    else
        RAYLIB_INC = /usr/local/include
        LIBS       = -lraylib -lm -ldl -lpthread
    endif
    EXT =
endif

all: $(TARGET)$(EXT)

$(TARGET)$(EXT): $(SRC)
	$(CC) $(CFLAGS) -I$(RAYLIB_INC) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean
