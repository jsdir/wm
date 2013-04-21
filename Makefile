default: all

all:
	make mini-clutter-wm

mini-clutter-wm:
	gcc mini-clutter-wm.c -o mini-clutter-wm `pkg-config clutter-1.0 gtk+-2.0 --cflags --libs`

clean:
	-rm -f mini-clutter-wm
