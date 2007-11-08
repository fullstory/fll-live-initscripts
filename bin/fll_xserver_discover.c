/*
 *	fll_xserver_discover -- detect PCI VGA class hardware
 *
 *	Copyright (c) 2007 Kel Modderman <kel@otaku42.de>
 *
 *	Can be freely distributed and used under the terms of the GNU GPLv2.
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "pci/pci.h"

#define XSERVER_PCIIDS_DIR "/usr/share/xserver-xorg/pci/"

/*
 * These device id textual lists will be exported for a limited time only.
 * Eventually the Xorg drivers will export symbols correlating to supported
 * devices for automatic configuration.
 */
char *lookup_xorg_dvr_for(const char *string)
{
	char *driver = "vesa";
	char *ptr;
	struct dirent *entry;

	DIR *dir = opendir(XSERVER_PCIIDS_DIR);
	if (!dir)
		return 0;
		
	while ((entry = readdir(dir))) {
		if (entry->d_name[0] == '.')
			continue;
		
		char filename[256];
		char line[10];
				
		snprintf(filename, sizeof(filename),
			"%s/%s", XSERVER_PCIIDS_DIR, entry->d_name);
		
		FILE *file = fopen(filename, "r");
		if (!file)
			continue;
		while (fgets(line, sizeof(line), file) != NULL) {
			if (strncasecmp(line, string, strlen(string)) == 0) {
				fclose(file);
				driver = entry->d_name;
				/* strip .ids extenstion */
				ptr = strrchr(driver, '.');
				*ptr = '\0';
				ptr++;
				goto end;
			}
		}
		fclose(file);
	}

	end:
	return driver;
}

/*
 * Simple adaptation of example.c from pciutils. Find first VGA class device
 * and export some information about the device that a shell script can source.
 */
int main(void)
{
	struct pci_access *pacc;
	struct pci_dev *dev;
	char devbuf[128];
	char str[9];

	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	
	for (dev = pacc->devices; dev; dev = dev->next) {
		if (dev->device_class == 0x0300) {
			printf("XBUSID='PCI:%d:%d:%d'\nXVENDOR='%04x'\nXDEVICE='%04x'\n",
				dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id);
			printf("XBOARDNAME='%s'\n",
				pci_lookup_name(pacc, devbuf, sizeof(devbuf),
					PCI_LOOKUP_VENDOR | PCI_LOOKUP_DEVICE,
					dev->vendor_id, dev->device_id));
			snprintf(str, sizeof(str), "%04x%04x",
				dev->vendor_id, dev->device_id);
			printf("XMODULE='%s'\n", lookup_xorg_dvr_for(str));
			break;
		}
	}
	
	pci_cleanup(pacc);
	
	return 0;
}
