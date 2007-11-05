/*
 *	fll_xserver_discover -- detect PCI VGA class hardware
 *
 *	Copyright (c) 2007 Kel Modderman <kel@otaku42,de>
 *
 *	Can be freely distributed and used under the terms of the GNU GPLv2.
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "pci/pci.h"

int printxdriver(char *string)
{
	int found;
	DIR *dir;
	struct dirent *entry;

	dir = opendir("/usr/share/xserver-xorg/pci/");
	if (!dir)
		return 0;
		
	while ((entry = readdir(dir))) {
		if (entry->d_name[0] == '.')
			continue;
		
		char filename[128];
		char line[10];
		FILE *file;
		char driver[32];

		snprintf(filename, sizeof(filename),
			"/usr/share/xserver-xorg/pci/%s", entry->d_name);
		
		file = fopen(filename, "r");
		if (!file)
			continue;
		while(fgets(line, 10, file) != NULL) {
			if (strncasecmp(line, string, 8) == 0) {
				if (sscanf(entry->d_name, "%31[^.].ids", driver) == 1)
					printf("XDRIVER='%s'\n", driver);
				found++;
				goto end;
			}
		}
		fclose(file);
	}

	end:
	
	if (!found)
		printf("XDRIVER='vesa'\n");

	return 0;
}

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
			snprintf(str, sizeof(str), "%04x%04x", dev->vendor_id, dev->device_id);
			printxdriver(str);
			break;
		}
	}
	
	pci_cleanup(pacc);
	
	return 0;
}
