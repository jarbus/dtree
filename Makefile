CFLAGS=-g -O3 -Wall $(shell sdl2-config --cflags)
LDLIBS=$(shell sdl2-config --libs) -lSDL2_ttf

TARGET=$(basename $(wildcard *.c))

all: $(TARGET)

clean: 
	$(RM) $(TARGET)
