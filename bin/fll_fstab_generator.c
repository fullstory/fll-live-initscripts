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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

#include <libvolume_id.h>

#define SYS_BLK		"/sys/block"
#define DEV_DIR		"/dev"

#define DEV_PATH_MAX	VOLUME_ID_PATH_MAX
#define SYS_PATH_MAX	VOLUME_ID_PATH_MAX


static int disk_filter(const struct dirent *d)
{
	if (d->d_name[0] == 'h' && d->d_name[1] == 'd')
		return 1;
	if (d->d_name[0] == 's' && d->d_name[1] == 'd')
		return 1;

	return 0;
}

static int cdrom_filter(const struct dirent *d)
{
	if (strncmp(d->d_name, "cdrom", 5) == 0)
		return 1;

	return 0;
}

static int floppy_filter(const struct dirent *d)
{
	if (strncmp(d->d_name, "fd", 2) == 0 && strlen(d->d_name) >= 3)
		return 1;

	return 0;
}

static void set_str(char *to, const char *from, size_t count)
{
	size_t i, j, len;

	/* strip trailing whitespace */
	len = strnlen(from, count);
	while (len && isspace(from[len-1]))
		len--;

	/* strip leading whitespace */
	i = 0;
	while (isspace(from[i]) && (i < len))
		i++;

	j = 0;
	while (i < len) {
		/* substitute multiple whitespace */
		if (isspace(from[i])) {
			while (isspace(from[i]))
				i++;
			to[j++] = '_';
		}
		/* skip chars */
		if (from[i] == '/') {
			i++;
			continue;
		}
		to[j++] = from[i++];
	}
	to[j] = '\0';
}

static int ata_id(char *buf, int bufsiz, const char* node)
{
	struct hd_driveid id;
	int fd;

	fd = open(node, O_RDONLY|O_NONBLOCK);
	if (fd) {
		if (!ioctl(fd, HDIO_GET_IDENTITY, &id))
			set_str(buf, (char *) id.model, 40);

		close(fd);
		return 1;
	}

	return 0;
}

static void disk_entry(const char* disk, int debug)
{
	struct dirent **diskdir;
	int dirnum;
	int n;
	char sysd[SYS_PATH_MAX];
	char node[DEV_PATH_MAX];
	char model[41];
	int bufsiz = sizeof(model) -1;
	int disklen = strlen(disk);

	snprintf(node, sizeof(node), "%s/%s", DEV_DIR, disk);
	snprintf(sysd, sizeof(sysd), "%s/%s", SYS_BLK, disk);

	ata_id(model, bufsiz, node);

	dirnum = scandir(sysd,
			 &diskdir,
			 disk_filter,
			 versionsort);

	if (dirnum > 0) {
		for (n = 0; n < dirnum; n++) {
			if (strncmp(diskdir[n]->d_name, disk, disklen) != 0)
				continue;

			if (debug)
				fprintf(stderr, "---> %s (partition)\n",
					diskdir[n]->d_name);

			//partition_entry(diskdir[n]->d_name, debug);
		}
	}

}

static void cdrom_entry(const char* cdrom, int debug)
{
	char node[DEV_PATH_MAX];
	char model[41];
	int bufsiz = sizeof(model) -1;

	snprintf(node, sizeof(node), "%s/%s", DEV_DIR, cdrom);

	if (ata_id(model, bufsiz, node))
		fprintf(stdout, "\n# %s\n", model);
	else
		fprintf(stdout, "\n");
	
	fprintf(stdout, "%s\t/media/%s\tudf,iso9660\tuser,noauto\t0\t0\n",
		node, cdrom);
}

static void floppy_entry(const char* floppy, int debug)
{
	char node[DEV_PATH_MAX];

	snprintf(node, sizeof(node), "%s/%s", DEV_DIR, floppy);

	fprintf(stdout, "\n%s\t/media/%s\tauto\trw,user,noauto\t0\t0\n",
		node, floppy);
}

int main(int argc, char *argv[])
{
	struct dirent **blkdir;
	int dirnum;
	int n;

	int opt;
	int autom = 0;
	int debug = 1;
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

	/* scan for hard disk devices in sysfs dirheir */
	dirnum = scandir(SYS_BLK,
			 &blkdir,
			 disk_filter,
			 versionsort);

	if (dirnum > 0) {
		for (n = 0; n < dirnum; n++) {
			if (debug)
				fprintf(stderr, "---> %s (disk)\n",
					blkdir[n]->d_name);

			disk_entry(blkdir[n]->d_name, debug);
		}
	}

	/* scan for cdrom device node symlinks */
	dirnum = scandir(DEV_DIR,
			 &blkdir,
			 cdrom_filter,
			 versionsort);
	
	if (dirnum > 0) {
		for (n = 0; n < dirnum; n++) {
			if (debug)
				fprintf(stderr, "---> %s (cdrom)\n",
					blkdir[n]->d_name);

			cdrom_entry(blkdir[n]->d_name, debug);
		}
	}

	/* scan for floppy device nodes */
	dirnum = scandir(DEV_DIR,
			 &blkdir,
			 floppy_filter,
			 versionsort);
	
	if (dirnum > 0) {
		for (n = 0; n < dirnum; n++) {
			if (debug)
				fprintf(stderr, "---> %s (floppy)\n",
					blkdir[n]->d_name);

			floppy_entry(blkdir[n]->d_name, debug);
		}
	}

	return 0;
}
