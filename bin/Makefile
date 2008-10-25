ALL    := fll_xserver_discover
COPTS  := -W -Wall -Werror
CFLAGS := -g -O2

fll_xserver_discover_LIBS := $(shell pkg-config --libs pciaccess)

all: $(ALL:%=all-%)
all-%:
	gcc $(COPTS) $(CFLAGS) $($*_LIBS) -o $* $*.c

clean:
	$(RM) $(ALL)

distclean: clean
