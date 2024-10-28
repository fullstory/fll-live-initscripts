CFLAGS ?= -O2 -g -Wall
LIBS_UDEV = $(shell pkg-config --libs libudev)
LIBS_BLKID = $(shell pkg-config --libs blkid)
PROGS = fll_fstab_generator

all: $(PROGS)

fll_fstab_cmdline.c: fll_fstab_generator.ggo
	gengetopt --input=$< --file-name=fll_fstab_cmdline \
		  --unamed-opts=DEVICES

fll_fstab_cmdline.o: fll_fstab_cmdline.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -c -o $@ $<

fll_fstab_generator: fll_fstab_cmdline.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $< -o $@ $@.c $(LIBS_UDEV) $(LIBS_BLKID)

clean:
	$(RM) $(PROGS) *_cmdline.*
