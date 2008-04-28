/* -------------------------------------------------------------------------
   fll_fstab_generator
   -------------------
   Detect block devices and output a fstab configuration.

   Copyright: (c) 2008 Kel Modderman <kel@otaku42.de>
   License: GPLv2
   ------------------------------------------------------------------------- */

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

#ifndef BLKGETSIZE64
#define BLKGETSIZE64    _IOR(0x12, 114, size_t)
#endif

#define SYS_BLK         "/sys/block"
#define DEV_DIR         "/dev"

#define DEV_PATH_MAX    VOLUME_ID_PATH_MAX
#define SYS_PATH_MAX    VOLUME_ID_PATH_MAX

/* -------------------------------------------------------------------------
   global options
   --------------
   opt_automnt - automount filesystem volumes
   opt_debug   - enable debugging
   opt_mkmntpt - make mountpoints as required
   opt_noswap  - disable swap configuration
   opt_uuids   - use filesystem uuids in fstab configuration
   ------------------------------------------------------------------------- */
static int opt_automnt = 0;
static int opt_debug = 1;
static int opt_mkmntpt = 0;
static int opt_noswap = 0;
static int opt_uuids = 0;

/* -------------------------------------------------------------------------
   filesystem struct
   -----------------
   Hold information about a filesystem:
     diskn - disk device index
     node  - block device node
     label - filesystem label
     type  - filesystem type
     usage - volume usage
     uuid  - filesystem uuid
   ------------------------------------------------------------------------- */
struct filesystem {
	int diskn;
	const char *node;
	const char *label;
	const char *type;
	const char *usage;
	const char *uuid;
};

/* -------------------------------------------------------------------------
   <device>_filter functions
   -------------------------
   These functions are given to scandir(2) when scanning for a particular
   type of block device, to filter out the desired output.
   ------------------------------------------------------------------------- */
static int disk_filter(const struct dirent *d)
{
	int ret;
	char sysfs_path[SYS_PATH_MAX];
	char rem[2];
	FILE *fp;

	ret = snprintf(sysfs_path, sizeof(sysfs_path),
		       "%s/%s/removable", SYS_BLK, d->d_name);
	if (ret < 0)
		return 0;
	
	fp = fopen(sysfs_path, "r");
	if (fp) {
		if (fgets(rem, sizeof(rem), fp) != NULL)
			ret = atoi(rem);

		fclose(fp);

		if (opt_debug)
			fprintf(stderr, "%s = %d\n", sysfs_path, ret);

		if (ret == 0)
			return 1;
	}

	return 0;
}

static int cdrom_filter(const struct dirent *d)
{
	/*
	 * Return 1 for any symlink in /dev/ starting with cdrom.
	 */
	if (strncmp(d->d_name, "cdrom", 5) == 0 &&
	    d->d_type == DT_LNK)
		return 1;

	return 0;
}

static int floppy_filter(const struct dirent *d)
{
	/*
	 * Return 1 for any block device in /dev/ starting with fd.
	 */
	if (strncmp(d->d_name, "fd", 2) == 0 &&
	    d->d_type == DT_BLK)
		return 1;

	return 0;
}

/* -------------------------------------------------------------------------
   <device>_entry functions
   ------------------------
   Print out a line of fstab for a particular device type.
   ------------------------------------------------------------------------- */
static void filesystem_entry(struct filesystem *fs)
{
	const char *fs_auto;
	const char *fs_opts;
	int fs_dumpno = 0;
	int fs_passno = 2;

	if (strcmp(fs->usage, "filesystem") != 0 &&
	    strcmp(fs->usage, "other") != 0)
		return;
	
	if (strncmp(fs->type, "ext", 3) == 0 ||
	    strncmp(fs->type, "reiser", 6) == 0 ||
	    strcmp(fs->type, "jfs") == 0 ||
	    strcmp(fs->type, "xfs") == 0) {
		fs_opts = "users,exec,noatime";
	} else if (strcmp(fs->type, "swap") == 0) {
		fs_opts = "sw";
		fs_passno = 0;
	} else if (strcmp(fs->type, "ntfs") == 0) {
		fs_opts = "ro,dmask=0022,fmask=0133,nls=utf8";
		fs_passno = 0;
	} else if (strcmp(fs->type, "msdos") == 0) {
		fs_opts = "quiet,umask=000,iocharset=utf8";
		fs_passno = 0;
	} else if (strcmp(fs->type, "vfat") == 0) {
		fs_opts = "shortname=lower,quiet,umask=000,utf8";
		fs_passno = 0;
	} else
		return;
	
	if (strcmp(fs->type, "swap") == 0)
		fs_auto = "";
	else
		fs_auto = opt_automnt ? "auto," : "noauto,";

	fprintf(stdout, "\n%s\t/media/disk%dpart?\t%s\t%s%s  %d  %d\n",
		fs->node,
		fs->diskn,
		fs->type,
		fs_auto,
		fs_opts,
		fs_dumpno,
		fs_passno);
}

static void cdrom_entry(const char* cdrom)
{
	int ret;
	char node[DEV_PATH_MAX];

	ret = snprintf(node, sizeof(node), "%s/%s", DEV_DIR, cdrom);
	if (ret < 0)
		return;

	fprintf(stdout, "\n%s\t/media/%s\tudf,iso9660\tuser,noauto\t0  0\n",
		node,
		cdrom);
}

static void floppy_entry(const char* floppy)
{
	int ret;
	char node[DEV_PATH_MAX];

	ret = snprintf(node, sizeof(node), "%s/%s", DEV_DIR, floppy);
	if (ret < 0)
		return;

	fprintf(stdout, "\n%s\t/media/%s\tauto\trw,user,noauto\t0  0\n",
		node,
		floppy);
}

/* -------------------------------------------------------------------------
   volume_id wrapping functions
   ----------------------------
   vol_id_probe() handles the filesystem probing, it temporarily drops privs
   before reading the block device, then restroes privs thereafter.

   vol_id() fills the filesystem struct pointer with the probed filesystem
   information.
   ------------------------------------------------------------------------- */
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

	if (grn < 0)
		goto error;

	groups[grn++] = gid;

	/*
	 * try to drop all privileges before reading disk content
	 */
	if (uid == 0) {
		struct passwd *pw;
		pw = getpwnam("nobody");

		if (pw != NULL && pw->pw_uid > 0 && pw->pw_gid > 0) {
			if (setgroups(0, NULL) != 0 ||
			    setegid(pw->pw_gid) != 0 ||
			    seteuid(pw->pw_uid) != 0)
			    	goto error;
		}
	}

	ret = volume_id_probe_all(vid, 0, size);

	/*
	 * restore original privileges
	 */
	if (uid == 0) {
		if (seteuid(uid) != 0 ||
		    setegid(gid) != 0 ||
		    setgroups(grn, groups) != 0)
		    	goto error;
	}

	return ret;

 error:
 	fprintf(stderr, "vol_id_probe() failed\n");
	return -1;
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

/* -------------------------------------------------------------------------
   scandisk
   --------
   Given the base disk device name, scan /dev/ for partitions
   ------------------------------------------------------------------------- */
static void scandisk(const char *disk, int diskn)
{
	struct dirent **dir;
	int dirnum, n, ret;
	int disklen = strlen(disk);
	char sysfs_path[SYS_PATH_MAX];

	ret = snprintf(sysfs_path, sizeof(sysfs_path),
		       "%s/%s", SYS_BLK, disk);
	if (ret < 0)
		return;

	/*
	 * scan for partition device node symlinks
	 */
	dirnum = scandir(sysfs_path,
			 &dir,
			 0,
			 versionsort);

	for (n = 0; n < dirnum; n++) {
		if (strncmp(dir[n]->d_name, disk, disklen) != 0)
			continue;

		struct filesystem f, *fs;
		int ret;
		char node[DEV_PATH_MAX];

		ret = snprintf(node, sizeof(node), "%s/%s", DEV_DIR, dir[n]->d_name);
		if (ret < 0)
			continue;

		f.diskn = diskn + 1;
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

		if (!fs->usage || !fs->type)
			continue;

		filesystem_entry(fs);
	}
}

/* -------------------------------------------------------------------------
   main
   ----
   Detect block devices and output a fstab configuration.
   ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
	struct dirent **dir;
	int dirnum, n, opt;

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
		if (opt_debug)
			fprintf(stderr, "---> %s\n", dir[n]->d_name);

		scandisk(dir[n]->d_name, n);
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
