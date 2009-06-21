datadir = /usr/share
srcdir = .

pcidatadir = $(datadir)/xserver-xorg/pci
pcidata_DATA = intel.ids

intel.ids : common.h
	awk '/^#define PCI_CHIP/ {print $$3 }' $(srcdir)/common.h | sed 's/^0x/8086/' > intel.ids

all: intel.ids

clean:
	$(RM) *.ids

distclean: clean
