CFLAGS = -g -O2 -Wall
LIBS = $(shell pkg-config --libs libpci)

fll_xserver_discover:
	gcc -o $@ $@.c $(CFLAGS) $(LIBS)

clean:
	$(RM) fll_xserver_discover

.PHONY: clean
