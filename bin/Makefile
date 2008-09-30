COPTS  := -W -Wall -Werror
CFLAGS := -g -O2
LIBS_PCIACCESS := $(shell pkg-config --libs pciaccess)
ALL := fll_xserver_discover

all: $(ALL)

fll_xserver_discover:
	gcc $(COPTS) $(CFLAGS) $(LIBS_PCIACCESS) -o $@ $@.c

clean:
	$(RM) $(ALL)

distclean: clean
