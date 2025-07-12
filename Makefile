DIRS := bin man
INIT := $(notdir $(wildcard share/fll-live-initscripts/fll_*))

all: $(DIRS:%=all-%)
all-%:
	$(MAKE) -C $* all

clean: $(DIRS:%=clean-%)
clean-%:
	$(MAKE) -C $* clean

distclean: clean

test: $(INIT:%=test-%)
test-%:
	$(info checking $* ...)
	@dash -n share/fll-live-initscripts/$*
	@checkbashisms -p share/fll-live-initscripts/$*
