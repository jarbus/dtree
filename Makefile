dtree: main.o
	gcc main.o -o dtree -lSDL2

main.o: main.c
	gcc main.c -c
