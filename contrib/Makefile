ALL := ati nv

all: $(addsuffix .ids,$(ALL))
%.ids:
	gawk -f $*/$*.awk $*/*.[ch] > $@
	$(MAKE) -C vmmouse_detect

clean:
	$(RM) *.ids

distclean: clean
