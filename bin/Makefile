CFLAGS ?= -O2 -g -Wall
LIBS =  $(shell pkg-config --libs blkid)
LIBS += $(shell pkg-config --libs libudev)

all: fll_fstab_generator

cmdline.c: fll_fstab_generator.ggo
	gengetopt --input=$< --unamed-opts=DEVICES

cmdline.o: cmdline.c
	$(CC) $(CFLAGS) -c -o $@ $<

fll_fstab_generator: cmdline.o
	$(CC) $(CFLAGS) $< -o $@ $@.c $(LIBS)

clean:
	$(RM) fll_fstab_generator cmdline.*
