/*
 * fll_xserver_discover
 * --------------------
 * Detect PCI VGA class hardware and output information a shell script can use
 * to generate an xorg.conf configuration file.
 *
 * Copyright: (c) 2007 - 2008 Kel Modderman <kel@otaku42.de>
 * License: GPLv2.
 *
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

#include <pciaccess.h>

#define XSERVER_PCIIDS_DIR "/usr/share/xserver-xorg/pci/"


/*
 * Only read files with suffix of '.ids'.
 */
static int ids_file(const struct dirent *entry)
{
	char *ptr;

	if (entry->d_type != DT_REG)
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
static int driver_prio(const void *A, const void *B)
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
static const char *fll_get_x_driver(uint16_t v_id, uint16_t d_id, int debug)
{
	struct dirent **ids;
	int n, dirnum;
	char *ptr, *driver;
	char string[10];

	driver = NULL;
	snprintf(string, sizeof(string), "%04x%04x", v_id, d_id);

	dirnum = scandir(XSERVER_PCIIDS_DIR, &ids, ids_file, driver_prio);
	if (dirnum <= 0)
		return driver;

	/* read each pciid list */
	for (n = 0; n < dirnum; n++) {
		FILE *file;
		char filename[256], line[10];

		snprintf(filename, sizeof(filename),
			 "%s%s", XSERVER_PCIIDS_DIR, ids[n]->d_name);

		if (debug)
			printf("D: looking for %s in %s\n", string, filename);

		file = fopen(filename, "r");
		if (!file)
			continue;

		while (fgets(line, sizeof(line), file) != NULL) {
			if (debug > 1)
				printf("%s: %s", filename, line);

			if (strncasecmp(line, string, strlen(string)) == 0) {
				/* found string in $driver.ids */
				driver = ids[n]->d_name;

				if (debug)
					printf("D: found in %s\n", driver);

				/* strip .ids extenstion */
				ptr = strstr(driver, ".ids");
				*ptr = '\0';

				if (debug < 2)
					break;
			}
		}

		fclose(file);
		free(ids[n]);

		if (driver && debug < 2)
			break;
	}

	free(ids);

	return driver;
}


/*
 * Print out busid, vendor_id, device_id, board description and a driver
 * for each device of VGA class.
 */
static int fll_get_vga_device(struct pci_device *dev, int debug)
{
	const char *ven_name, *dev_name, *x_driver;

	ven_name = pci_device_get_vendor_name(dev);
	if (!ven_name)
		ven_name = "Unknown vendor";

	dev_name = pci_device_get_device_name(dev);
	if (!dev_name)
		dev_name = "Unknown device";

	if (debug)
		printf("D: %s %s\n", ven_name, dev_name);

	if (((dev->device_class >> 16) & 0x0ff) == 0x03 &&
	    ((dev->device_class >>  8) & 0x0ff) == 0x00 &&
	    ((dev->device_class >>  0) & 0x0ff) == 0x00) {
		printf("XVENDOR='%04x'\nXDEVICE='%04x'\n",
		       dev->vendor_id,
		       dev->device_id);

		printf("XBUSID='PCI:%d:%d:%d'\n",
		       dev->bus,
		       dev->dev,
		       dev->func);

		printf("XBOARDNAME='%s %s'\n",
		       ven_name,
		       dev_name);

		x_driver = fll_get_x_driver(dev->vendor_id,
					    dev->device_id,
					    debug);

		if (x_driver)
			printf("XMODULE='%s'\n", x_driver);

		return 1;
	}

	return 0;
}

/*
 * Simple adaptation of example.c from pciutils. Find all VGA class devices
 * and export some information about each device that a shell script can use.
 */
int main(int argc, char *argv[])
{
	struct pci_device_iterator *iter;
	struct pci_device *dev;
	int ret, opt, debug;
	
	debug = 0;

	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
		case 'd':
			debug++;
			break;
		default:
			break;
		}
	}

	ret = pci_system_init();
	if (ret != 0) {
		printf("Couldn't initialize PCI system\n");
		exit(1);
	}

	iter = pci_slot_match_iterator_create(NULL);

	while ((dev = pci_device_next(iter)) != NULL) {
		if (fll_get_vga_device(dev, debug) && !debug)
			break;
	}

	pci_system_cleanup();

	return 0;
}
