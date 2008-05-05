CFLAGS := -g -O2 -Wall -Werror
LIBS_PCIACCESS := $(shell pkg-config --libs pciaccess)
ALL := fll_xserver_discover

all: $(ALL)

fll_xserver_discover:
	gcc -o $@ $@.c $(CFLAGS) $(LIBS_PCIACCESS)

clean:
	$(RM) $(ALL)
