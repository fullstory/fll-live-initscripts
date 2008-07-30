COPTS  := -W -Wall -Werror
CFLAGS := -g -O2
LIBS_PCIACCESS := $(shell pkg-config --libs pciaccess)
ALL := fll_xserver_discover fll_is80211

all: $(ALL)

fll_xserver_discover:
	gcc $(COPTS) $(CFLAGS) $(LIBS_PCIACCESS) -o $@ $@.c

fll_is80211:
	gcc $(COPTS) $(CFLAGS) -o $@ $@.c

clean:
	$(RM) $(ALL)

distclean: clean
