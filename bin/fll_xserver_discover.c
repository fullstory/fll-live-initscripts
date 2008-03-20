/*
 *	fll_xserver_discover -- detect PCI VGA class hardware
 *
 *	Copyright (c) 2007 - 2008 Kel Modderman <kel@otaku42.de>
 *
 *	Can be freely distributed and used under the terms of the GNU GPLv2.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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


/*
 * Only read files with suffix of '.ids'.
 */
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
 * Prioritize the order in which we choose a matching driver in a predictable
 * fashion, giving or taking priority away from select drivers, falling back
 * to versionsort(A, B).
 *
 * Use the ati driver as a last resort, allow devices to be claimed by
 * radeonhd (R500+), r128 and mach64 drivers first.
 * http://www.phoronix.com/scan.php?page=article&item=radeon_vs_radeonhd&num=1
 * http://bgoglin.livejournal.com/15162.html
 */
int driver_prio(const void *A, const void *B)
{
	const char *a = (*(const struct dirent **)A)->d_name;
	const char *b = (*(const struct dirent **)B)->d_name;

	if (strncmp("ati", a, 3) == 0)
		return 1;
	if (strncmp("ati", b, 3) == 0)
		return -1;

	return versionsort(A, B);
}


/*
 * These device id plaintext lists will be exported for a limited time only
 * by the debian xserver-xorg-driver packages.
 *
 * Eventually the Xorg drivers will export symbols correlating to supported
 * devices for automatic configuration.
 */
char *lookup_xorg_dvr_for(const char *string, int debug)
{
	struct dirent **ids;
	int num, n;

	char *driver = "";
	char *ptr;

	num = scandir(XSERVER_PCIIDS_DIR, &ids, ids_file, driver_prio);
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
			if (debug > 1)
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

				if (!debug)
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


/*
 * Print out busid, vendor_id, device_id, board description and a driver
 * for each device of VGA class.
 */
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
 * and export some information about each device that a shell script can use.
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
