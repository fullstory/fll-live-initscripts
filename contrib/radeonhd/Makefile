datadir = /usr/share
srcdir = .

pcidatadir = $(datadir)/xserver-xorg/pci
pcidata_DATA = radeonhd.ids

radeonhd.ids : rhd_id.c
	awk '/RHD_DEVICE_MATCH\(.*0x/ { print $$2}' $(srcdir)/rhd_id.c | sed -e 's/,//' -e 's/0x/1002/' > radeonhd.ids

all: radeonhd.ids

clean:
	$(RM) *.ids

distclean: clean
