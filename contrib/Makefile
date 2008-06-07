ALL := xrandr/nv.ids xrandr/ati.ids

all: $(ALL)

xrandr/ati.ids:
	@mkdir -p xrandr
	gawk -f ati.awk src/ati_pciids_gen.h > $@

xrandr/nv.ids:
	@mkdir -p xrandr
	gawk -f nv.awk src/nv_driver.c > $@

clean:
	$(RM) $(ALL)
	if [ -d xrandr ]; then rmdir xrandr; fi

distclean: clean
