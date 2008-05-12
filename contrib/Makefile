ALL := xrandr/nv xrandr/ati

all: $(ALL)

xrandr/ati:
	@mkdir -p xrandr
	gawk -f nv.awk src/ati_pciids_gen.h > $@

xrandr/nv:
	@mkdir -p xrandr
	gawk -f nv.awk src/nv_driver.c > $@

clean:
	$(RM) $(ALL)
	if [ -d xrandr ]; then rmdir xrandr; fi

distclean: clean
