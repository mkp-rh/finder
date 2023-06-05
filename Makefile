all:
	gcc -ggdb2 -Wall finder.c -o finder -lX11 -lpthread $(shell pkg-config --libs --cflags cairo)
