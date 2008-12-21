ALL := ati nv

all: $(addsuffix .ids,$(ALL))
	$(MAKE) -C vmmouse_detect

%.ids:
	gawk -f $*/$*.awk $*/*.[ch] > $@

clean:
	$(RM) *.ids
	$(MAKE) -C vmmouse_detect clean

distclean: clean
