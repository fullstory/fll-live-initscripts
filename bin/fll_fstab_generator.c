/*
 * fll_fstab_generator
 * -------------------
 * Output an fstab configuration file based on block device detection
 * mechanisms.
 *
 * Copyright: (c) 2008 Kel Modderman <kel@otaku42.de>
 * License: GPLv2
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

#include <libvolume_id.h>

#define SYS_CLASS_BLOCK "/sys/class/block/"


int main(int argc, char *argv[])
{
	int opt;
	int autom = 0;
	int debug = 0;
	int mkmnt = 0;
	int noswp = 0;
	int simul = 0;
	int uuids = 0;
	int write = 0;

	while ((opt = getopt(argc, argv, "admnsuw")) != -1) {
		switch(opt) {
		case 'a':
			autom++;
			break;
		case 'd':
			debug++;
			break;
		case 'm':
			mkmnt++;
			break;
		case 'n':
			noswp++;
			break;
		case 's':
			simul++;
			break;
		case 'u':
			uuids++;
			break;
		case 'w':
			write++;
			break;
		default:
			break;
		}
	}

	return 0;
}
