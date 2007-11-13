CFLAGS = -g -O2 -Wall

fll_xserver_discover:
	gcc -o $@ $(CFLAGS) $@.c -lpci -lz

clean:
	rm -f fll_xserver_discover

.PHONY: clean
