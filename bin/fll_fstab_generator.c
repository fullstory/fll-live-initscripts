/*
 * fll_fstab_generator
 * -------------------
 * Detect block devices and output a fstab configuration.
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
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>

#include <libvolume_id.h>

#define BLKGETSIZE64    _IOR(0x12, 114, size_t)

#define SYS_BLK         "/sys/block"
#define DEV_DIR         "/dev"

#define DEV_PATH_MAX    VOLUME_ID_PATH_MAX
#define SYS_PATH_MAX    VOLUME_ID_PATH_MAX

/* ------------------------------------------------------------------------- *
 * global options
 * --------------
 * opt_automnt - automount filesystem volumes
 * opt_debug   - enable debugging
 * opt_mkmntpt - make mountpoints as required
 * opt_noswap  - disable swap configuration
 * opt_uuids   - use filesystem uuids in fstab configuration
 * ------------------------------------------------------------------------- */
static int opt_automnt = 0;
static int opt_debug = 1;
static int opt_mkmntpt = 0;
static int opt_noswap = 0;
static int opt_uuids = 0;

/* ------------------------------------------------------------------------- *
 * filesystem struct
 * -----------------
 * Hold information about a filesystem:
 *   node  - block device node
 *   label - filesystem label
 *   type  - filesystem type
 *   usage - volume usage
 *   uuid  - filesystem uuid
 * ------------------------------------------------------------------------- */
struct filesystem {
	const char *node;
	const char *label;
	const char *type;
	const char *usage;
	const char *uuid;
};

/* ------------------------------------------------------------------------- *
 * <device>_filter functions
 * -------------------------
 * These functions are given to scandir(2) when scanning for a particular
 * type of block device, to filter out the desired output.
 * ------------------------------------------------------------------------- */
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


/* ------------------------------------------------------------------------- *
 * volume_id wrapping functions
 * ----------------------------
 * vol_id_probe() handles the filesystem probing, it temporarily drops privs
 * before reading the block device, then restroes privs thereafter.
 *
 * vol_id() fills the filesystem struct pointer with the probed filesystem
 * information.
 * ------------------------------------------------------------------------- */
static int vol_id_probe(struct volume_id *vid, uint64_t size, const char *node)
{
	int ret, gid, uid, grn;
	int ngroups_max = NGROUPS_MAX - 1;
	gid_t groups[NGROUPS_MAX];

	/* 
	 * store privilege properties for restoration later
	 */
	uid = geteuid();
	gid = getegid();
	grn = getgroups(ngroups_max, groups);

	if (grn < 0) {
		fprintf(stderr, "E: getgroups failed\n");
		return -1;
	}

	groups[grn++] = gid;

	/* 
	 * try to drop all privileges before reading disk content
	 */
	if (uid == 0) {
		struct passwd *pw;
		pw = getpwnam("nobody");

		if (pw != NULL && pw->pw_uid > 0 && pw->pw_gid > 0) {
			if (setgroups(0, NULL) != 0) {
				fprintf(stderr, "E: setgroups failed\n");
				return -1;
			}
			if (setegid(pw->pw_gid) != 0) {
				fprintf(stderr, "E: setegid failed\n");
				return -1;
			}
			if (seteuid(pw->pw_uid) != 0) {
				fprintf(stderr, "E: seteuid failed\n");
				return -1;
			}
		}
	}

	ret = volume_id_probe_all(vid, 0, size);

	/* 
	 * restore original privileges
	 */
	if (uid == 0) {
		if (seteuid(uid) != 0) {
			fprintf(stderr, "E: seteuid failed\n");
			return -1;
		}
		if (setegid(gid) != 0) {
			fprintf(stderr, "E: setegid failed\n");
			return -1;
		}
		if (setgroups(grn, groups) != 0) {
			fprintf(stderr, "E: setgroups failed\n");
			return -1;
		}
	}

	if (ret != 0)
		return -1;

	return 0;
}

static void vol_id(struct filesystem *fs, const char* node)
{
	struct volume_id *vid;
	int fd, ret;
	uint64_t size;
	char label_enc[VOLUME_ID_PATH_MAX];
	char uuid_enc[VOLUME_ID_PATH_MAX];
	const char *label, *uuid, *type, *usage;

	fd = open(node, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "E: failed to get file descriptor for %s\n",
			node);
		return;
	}

	vid = volume_id_open_fd(fd);
	if (vid == NULL) {
		fprintf(stderr, "E: volume_id_open_fd failed to open file descriptor %d\n",
			fd);
		goto end;
	}

	if (ioctl(fd, BLKGETSIZE64, &size) != 0)
		size = 0;

	ret = vol_id_probe(vid, size, node);
	if (ret < 0)
		goto end;

	if (!volume_id_get_label(vid, &label) ||
	    !volume_id_get_usage(vid, &usage) ||
	    !volume_id_get_type(vid, &type) ||
	    !volume_id_get_uuid(vid, &uuid)) {
		fprintf(stderr, "E: volume_id_get_* failed\n");
		goto end;
	}

	volume_id_encode_string(label, label_enc, sizeof(label_enc));
	volume_id_encode_string(uuid, uuid_enc, sizeof(uuid_enc));

	if (strlen(label_enc) > 0)
		fs->label = label_enc;
	if (strlen(uuid_enc) > 0)
		fs->uuid = uuid_enc;
	if (strlen(type) > 0)
		fs->type = type;
	if (strlen(usage) > 0)
		fs->usage = usage;
	
 end:
	if (vid != NULL)
		volume_id_close(vid);

	close(fd);
}

/* ------------------------------------------------------------------------- *
 * <device>_entry functions
 * ------------------------
 * Print out a line of fstab for a particular device type.
 * ------------------------------------------------------------------------- */
static void cdrom_entry(const char* cdrom)
{
	char node[DEV_PATH_MAX];

	snprintf(node, sizeof(node), "%s/%s", DEV_DIR, cdrom);

	fprintf(stdout, "\n%s\t/media/%s\tudf,iso9660\tuser,noauto\t0\t0\n",
		node, cdrom);
}

static void floppy_entry(const char* floppy)
{
	char node[DEV_PATH_MAX];

	snprintf(node, sizeof(node), "%s/%s", DEV_DIR, floppy);

	fprintf(stdout, "\n%s\t/media/%s\tauto\trw,user,noauto\t0\t0\n",
		node, floppy);
}


/* ------------------------------------------------------------------------- *
 * main
 * ----
 * Detect block devices and output a fstab configuration.
 * ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
	struct dirent **dir;
	int dirnum;
	int n;
	int opt;

	while ((opt = getopt(argc, argv, "admnu")) != -1) {
		switch (opt) {
		case 'a':
			opt_automnt++;
			break;
		case 'd':
			opt_debug++;
			break;
		case 'm':
			opt_mkmntpt++;
			break;
		case 'n':
			opt_noswap++;
			break;
		case 'u':
			opt_uuids++;
			break;
		default:
			break;
		}
	}

	/*
	 * scan for hard disk devices in sysfs dirheir
	 */
	dirnum = scandir(SYS_BLK,
			 &dir,
			 disk_filter,
			 versionsort);

	for (n = 0; n < dirnum; n++) {
		struct dirent **dir2;
		int dirnum2;
		int m;
		char *disk = dir[n]->d_name;

		if (opt_debug)
			fprintf(stderr, "---> %s\n", disk);

		/*
		 * scan for partition device node symlinks
		 */
		dirnum2 = scandir(DEV_DIR,
				  &dir2,
				  disk_filter,
				  versionsort);
		
		for (m = 0; m < dirnum2; m++) {
			if (strncmp(dir2[m]->d_name, disk, strlen(disk)) != 0)
				continue;

			struct filesystem f, *fs;
			char node[DEV_PATH_MAX];
			snprintf(node, sizeof(node), "%s/%s", DEV_DIR, dir2[m]->d_name);

			f.node  = node;
			f.label = NULL;
			f.type  = NULL;
			f.usage = NULL;
			f.uuid  = NULL;
			
			fs = &f;
			vol_id(fs, node);

			if (opt_debug) {
				fprintf(stderr, "\t* vol_id(%s)\n", fs->node);
				if (fs->label)
					fprintf(stderr, "\t\t* label: %s\n", fs->label);
				if (fs->type)
					fprintf(stderr, "\t\t* type:  %s\n", fs->type);
				if (fs->usage)
					fprintf(stderr, "\t\t* usage: %s\n", fs->usage);
				if (fs->uuid)
					fprintf(stderr, "\t\t* uuid:  %s\n", fs->uuid);
			}
		}
	}

	/*
	 * scan for cdrom device node symlinks
	 */
	dirnum = scandir(DEV_DIR,
			 &dir,
			 cdrom_filter,
			 versionsort);

	for (n = 0; n < dirnum; n++) {
		if (opt_debug)
			fprintf(stderr, "---> %s\n", dir[n]->d_name);

		cdrom_entry(dir[n]->d_name);
	}

	/*
	 * scan for floppy device nodes
	 */
	dirnum = scandir(DEV_DIR,
			 &dir,
			 floppy_filter,
			 versionsort);

	for (n = 0; n < dirnum; n++) {
		if (opt_debug)
			fprintf(stderr, "---> %s\n", dir[n]->d_name);

		floppy_entry(dir[n]->d_name);
	}

	return 0;
}
