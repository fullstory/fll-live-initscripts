CFLAGS ?= -O2 -g -Wall
LIBS =  $(shell pkg-config --libs blkid)
LIBS += $(shell pkg-config --libs libudev)

all: fll_fstab_generator

fll_fstab_cmdline.c: fll_fstab_generator.ggo
	gengetopt --input=$< --file-name=fll_fstab_cmdline \
		  --unamed-opts=DEVICES

fll_fstab_cmdline.o: fll_fstab_cmdline.c
	$(CC) $(CFLAGS) -c -o $@ $<

fll_fstab_generator: fll_fstab_cmdline.o
	$(CC) $(CFLAGS) $< -o $@ $@.c $(LIBS)

clean:
	$(RM) fll_fstab_generator fll_fstab_cmdline.*
