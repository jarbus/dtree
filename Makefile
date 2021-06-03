CFLAGS=-g -O2 -Wall $(shell sdl2-config --cflags)
LDFLAGS=-g -O2 -Wall $(shell sdl2-config --libs) -lSDL2_ttf

PROGRAMS=$(basename $(wildcard *.c))

all: $(PROGRAMS)
