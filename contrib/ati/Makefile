datadir = /usr/share
srcdir = .

pcidatadir = $(datadir)/xserver-xorg/pci
pcidata_DATA = radeon.ids

radeon.ids: ati_pciids_gen.h
	cat ${srcdir}/ati_pciids_gen.h \
		| grep -v PCI_CHIP_MACH32 | grep -v PCI_CHIP_MACH64 | grep -v PCI_CHIP_RAGE128 \
		| awk '/^#define.*PCI_CHIP/ {print $$3}' \
		| sed -e 's/0x/1002/' > radeon.ids

all: radeon.ids

clean:
	$(RM) *.ids

distclean: clean
