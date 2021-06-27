FLAGS=-g -O3 -Wall $(shell sdl2-config --cflags)
all: CFLAGS=$(FLAGS)
debug: CFLAGS=$(FLAGS) -DDEBUG

LDLIBS=$(shell sdl2-config --libs) -lSDL2_ttf

TARGET=$(basename $(wildcard *.c))

all: $(TARGET)

debug: $(TARGET)

clean: 
	$(RM) $(TARGET)
