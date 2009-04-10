ALL := ati nv

all: $(addsuffix .ids,$(ALL))

%.ids:
	gawk -f $*/$*.awk $*/*.[ch] > $@

clean:
	$(RM) *.ids

distclean: clean
