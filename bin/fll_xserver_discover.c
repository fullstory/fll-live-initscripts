/*
 *	fll_xserver_discover -- detect PCI VGA class hardware
 *
 *	Copyright (c) 2007 - 2008 Kel Modderman <kel@otaku42.de>
 *
 *	Can be freely distributed and used under the terms of the GNU GPLv2.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>

#include "pci/pci.h"

#define VGA_CLASS 0x0300
#define XSERVER_PCIIDS_DIR "/usr/share/xserver-xorg/pci/"

struct pci_access *pacc;

int ids_file(const struct dirent *entry)
{
	char *ptr;

	if (entry->d_name[0] == '.')
		return 0;

	ptr = rindex(entry->d_name, '.');
	if (ptr && strcmp(ptr, ".ids") == 0)
		return 1;		

	return 0;
}

/*
 * These device id textual lists will be exported for a limited time only.
 * Eventually the Xorg drivers will export symbols correlating to supported
 * devices for automatic configuration.
 */
char *lookup_xorg_dvr_for(const char *string, int debug)
{
	struct dirent **ids;
	int num, n;

	char *driver = "";
	char *ptr;

	num = scandir(XSERVER_PCIIDS_DIR, &ids, ids_file, alphasort);
	if (num <= 0)
		return driver;

	/* read each pciid list */
	for (n = 0; n < num; n++) {
		char filename[256];
		char line[10];

		snprintf(filename, sizeof(filename),
		         "%s%s", XSERVER_PCIIDS_DIR, ids[n]->d_name);

		if (debug)
			printf("I: looking for %s in %s\n", string, filename);

		FILE *file;
		file = fopen(filename, "r");
		if (!file)
			continue;
		while (fgets(line, sizeof(line), file) != NULL) {
			if (debug && debug > 1)
				printf("%s: %s", filename, line);
			if (strncasecmp(line, string, strlen(string)) == 0) {
				/* found string in $driver.ids */
				driver = ids[n]->d_name;
				if(debug)
					printf("I: found %s in %s\n", string, driver);

				/* strip .ids extenstion */
				ptr = strrchr(driver, '.');
				*ptr = '\0';
				ptr++;

				break;
			}
		}
		fclose(file);
		free(ids[n]);

		if (strlen(driver) > 0 && !debug)
			break;
	}

	free(ids);
	return driver;
}

void xdisplay(struct pci_dev *dev, int debug)
{
	char devbuf[128];
	char str[10];

	if (debug)
		printf("%02x:%02x.%d vendor=%04x device=%04x class=%04x %s\n",
		       dev->bus, dev->dev, dev->func, dev->vendor_id,
		       dev->device_id, dev->device_class,
		       pci_lookup_name(pacc, devbuf, sizeof(devbuf),
				       PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
				       dev->vendor_id, dev->device_id));

	if (dev->device_class == VGA_CLASS) {
		/* convert bus:dev.func into BusID */
		printf("XBUSID='PCI:%d:%d:%d'\n",
		       dev->bus, dev->dev, dev->func);

		/*  print vendor + device ids */
		printf("XVENDOR='%04x'\nXDEVICE='%04x'\n",
		       dev->vendor_id, dev->device_id);

		/* look up board description */
		printf("XBOARDNAME='%s'\n",
		       pci_lookup_name(pacc, devbuf, sizeof(devbuf),
				       PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
				       dev->vendor_id, dev->device_id));

		/* concatenate vendor:device into string */
		snprintf(str, sizeof(str), "%04x%04x",
			 dev->vendor_id, dev->device_id);

		/* search for string in xserver pciids lists */
		printf("XMODULE='%s'\n", lookup_xorg_dvr_for(str, debug));
	}
}

/*
 * Simple adaptation of example.c from pciutils. Find all VGA class devices
 * and export some information about each device that a shell script can source.
 */
int main(int argc, char *argv[])
{
	struct pci_dev *p;

	int opt;
	int debug = 0;

	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch(opt) {
		case 'd':
			debug++;
			break;
		default:
			break;
		}
	}

	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);

	for (p = pacc->devices; p; p = p->next)
		xdisplay(p, debug);

	pci_cleanup(pacc);

	return 0;
}
