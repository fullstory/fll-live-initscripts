CFLAGS := -g -O2 -Wall -Werror
LIBS_PCIACCESS := $(shell pkg-config --libs pciaccess)
LIBS_VOLUME_ID := $(shell pkg-config --libs libvolume_id)
ALL := fll_xserver_discover fll_fstab_generator

all: $(ALL)

fll_xserver_discover:
	gcc -o $@ $@.c $(CFLAGS) $(LIBS_PCIACCESS)

fll_fstab_generator:
	gcc -o $@ $@.c $(CFLAGS) $(LIBS_VOLUME_ID)

clean:
	$(RM) $(ALL)

.PHONY: clean
