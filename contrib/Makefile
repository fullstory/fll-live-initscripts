ALL := ati nv
DIRS := ati intel radeonhd

all: $(addsuffix .ids,$(ALL)) $(DIRS:%=all-%)
all-%:
	$(MAKE) -C $* all

%.ids:
	gawk -f $*/$*.awk $*/*.[ch] > $@

clean: $(DIRS:%=clean-%)
	$(RM) *.ids
clean-%:
	$(MAKE) -C $* clean

distclean: clean
