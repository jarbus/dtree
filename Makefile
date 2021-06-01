dtree: main.o
	gcc main.o -o dtree -lSDL2 -g -fno-inline -fno-omit-frame-pointer

main.o: main.c
	gcc -g -fno-inline -fno-omit-frame-pointer main.c -c
