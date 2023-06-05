all:
	gcc -ggdb2 -O0 -Wall finder.c -o finder -lX11 -lXfixes -lXcomposite -lpthread -lcairo -I /usr/include/cairo
