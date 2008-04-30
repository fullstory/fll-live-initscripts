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
#define BLKGETSIZE64     _IOR(0x12, 114, size_t)
#endif

#define SYS_BLK          "/sys/block"
#define DEV_DIR          "/dev"

#define DEV_DISK_BYID    "/dev/disk/by-id"
#define DEV_DISK_BYLABEL "/dev/disk/by-label"
#define DEV_DISK_BYUUID  "/dev/disk/by-uuid"

#define DEV_PATH_MAX     VOLUME_ID_PATH_MAX
#define SYS_PATH_MAX     VOLUME_ID_PATH_MAX

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
static int opt_debug = 0;
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
	int partn;
	const char *label;
	const char *type;
	const char *usage;
	const char *uuid;
	char label_enc[VOLUME_ID_PATH_MAX];
	char uuid_enc[VOLUME_ID_PATH_MAX];
	char by_id[DEV_PATH_MAX];
	char by_label[DEV_PATH_MAX];
	char by_uuid[DEV_PATH_MAX];
};

/* -------------------------------------------------------------------------
   filesystem_debug
   ----------------
   Display the contents of the filesystem struct.
   ------------------------------------------------------------------------- */
static void filesystem_debug(struct filesystem *fs, const char *name,
			     const char *node)
{
	if (!opt_debug)
		return;

	fprintf(stderr, "---> %s (%s)\n", name, node);

	if (fs->diskn)
		fprintf(stderr, "\t* diskn: %d\n", fs->diskn);
	if (fs->diskn)
		fprintf(stderr, "\t* partn: %d\n", fs->partn);
	if (fs->label_enc)
		fprintf(stderr, "\t* label: %s\n", fs->label_enc);
	if (fs->type)
		fprintf(stderr, "\t* type:  %s\n", fs->type);
	if (fs->usage)
		fprintf(stderr, "\t* usage: %s\n", fs->usage);
	if (fs->uuid_enc)
		fprintf(stderr, "\t* uuid:  %s\n", fs->uuid_enc);

	fprintf(stderr, "\t* links:\n");
	if (fs->by_id)
		fprintf(stderr, "\t\t%s\n", fs->by_id);
	if (fs->by_label)
		fprintf(stderr, "\t\t%s\n", fs->by_label);
	if (fs->by_uuid)
		fprintf(stderr, "\t\t%s\n", fs->by_uuid);

	fprintf(stderr, "\t---\n");
}

/* -------------------------------------------------------------------------
   sysfs_device_<attribue> functions
   ---------------------------------
   These functions are test propoerties of the sysfs heir of a disk to
   determine if it should be considered for inclusion in fstab.
   ------------------------------------------------------------------------- */
static int sysfs_device_removable(const char *path)
{
	FILE *fp;
	char val[2];

	fp = fopen(path, "r");
	if (fp) {
		fgets(val, sizeof(val), fp);
		fclose(fp);

		if (val == NULL)
			return 1;

		if (opt_debug)
			fprintf(stderr, "%s\n---> %s\n", path, val);

		if (strcmp(val, "0") == 0)
			return 0;
	}

	return 1;
}

static int sysfs_device_isexternal(const char *path)
{
	char buf[SYS_PATH_MAX];
	int buflen = SYS_PATH_MAX - 1;
	int len;

	len = readlink(path, buf, buflen);
	if (len < 0)
		return 1;
	buf[len] = '\0';

	if (opt_debug)
		fprintf(stderr, "%s\n---> %s\n", path, buf);

	/*
	 * Hackish method of querying if disk device is attached
	 * via a parent device of usb or firwire type.
	 */
	if (strstr(buf, "/usb") != NULL)
		return 1;

	if (strstr(buf, "/fw") != NULL)
		return 1;

	return 0;
}

/* -------------------------------------------------------------------------
   <device>_filter functions
   -------------------------
   These functions are given to scandir(2) when scanning for a particular
   type of block device, to filter out the desired output.
   ------------------------------------------------------------------------- */
static int disk_filter(const struct dirent *d)
{
	int ret;
	char sysfs_rem_path[SYS_PATH_MAX];
	char sysfs_dev_path[SYS_PATH_MAX];

	ret = snprintf(sysfs_rem_path, sizeof(sysfs_rem_path),
		       "%s/%s/removable", SYS_BLK, d->d_name);
	if (ret < 0)
		return 0;

	ret = sysfs_device_removable(sysfs_rem_path);
	if (ret == 1)
		return 0;

	if (d->d_type == DT_LNK) {
		/*
		 * Linux >= 2.6.25:
		 *   /sys/block/sda -> ../devices/...
		 */
		ret = snprintf(sysfs_dev_path, sizeof(sysfs_dev_path),
			       "%s/%s", SYS_BLK, d->d_name);
	} else if (d->d_type == DT_DIR) {
		/*
		 * Linux < 2.6.25:
		 *   /sys/block/sda/device -> ../../devices/...
		 */
		ret = snprintf(sysfs_dev_path, sizeof(sysfs_dev_path),
			       "%s/%s/device", SYS_BLK, d->d_name);
	}
	if (ret < 0)
		return 0;

	ret = sysfs_device_isexternal(sysfs_dev_path);
	if (ret == 1)
		return 0;

	return 1;
}

static int cdrom_filter(const struct dirent *d)
{
	/*
	 * Return 1 for any symlink starting with cdrom.
	 */
	if (strncmp(d->d_name, "cdrom", 5) == 0 &&
	    d->d_type == DT_LNK)
		return 1;

	return 0;
}

static int floppy_filter(const struct dirent *d)
{
	/*
	 * Return 1 for any item in starting with fd.
	 */
	if (strncmp(d->d_name, "fd", 2) == 0)
		return 1;

	return 0;
}

/* -------------------------------------------------------------------------
   <device>_entry functions
   ------------------------
   Print out a line of fstab for a particular device type.
   ------------------------------------------------------------------------- */
static void filesystem_entry(struct filesystem *fs, const char *node)
{
	const char *fs_auto;
	const char *fs_opts;
	int fs_dump = 0;
	int fs_pass = 2;

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
		fs_pass = 0;
	} else if (strcmp(fs->type, "ntfs") == 0) {
		fs_opts = "ro,dmask=0022,fmask=0133,nls=utf8";
		fs_pass = 0;
	} else if (strcmp(fs->type, "msdos") == 0) {
		fs_opts = "quiet,umask=000,iocharset=utf8";
		fs_pass = 0;
	} else if (strcmp(fs->type, "vfat") == 0) {
		fs_opts = "shortname=lower,quiet,umask=000,utf8";
		fs_pass = 0;
	} else
		return;

	if (fs->label)
		fprintf(stdout, "\n# LABEL=%s\n", fs->label);
	else
		fprintf(stdout, "\n");

	if (opt_uuids && fs->uuid)
		fprintf(stdout, "UUID=%s", fs->uuid);
	else
		fprintf(stdout, "%s", node);

	if (strcmp(fs->type, "swap") == 0) {
		fprintf(stdout, "\tnone\t\t");
		fs_auto = "";
	} else {
		fprintf(stdout, "\t/media/disk%dpart%d",
			fs->diskn,
			fs->partn);
		fs_auto = opt_automnt ? "auto," : "noauto,";
	}

	fprintf(stdout, "\t%s", fs->type);

	fprintf(stdout, "\t%s%s  %d  %d\n",
		fs_auto,
		fs_opts,
		fs_dump,
		fs_pass);
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

	fd = open(node, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "E: failed to get file descriptor for %s\n",
			node);
		return;
	}

	vid = volume_id_open_fd(fd);
	if (vid == NULL) {
		fprintf(stderr, "E: volume_id_open_fd failed to open fd %d\n",
			fd);
		goto end;
	}

	if (ioctl(fd, BLKGETSIZE64, &size) != 0)
		size = 0;

	ret = vol_id_probe(vid, size, node);
	if (ret < 0)
		goto end;

	if (!volume_id_get_label(vid, &fs->label) ||
	    !volume_id_get_usage(vid, &fs->usage) ||
	    !volume_id_get_type(vid, &fs->type) ||
	    !volume_id_get_uuid(vid, &fs->uuid))
		goto end;

	volume_id_encode_string(fs->label, fs->label_enc,
				sizeof(fs->label_enc));
	volume_id_encode_string(fs->uuid, fs->uuid_enc,
				sizeof(fs->uuid_enc));

 end:
	if (vid != NULL)
		volume_id_close(vid);

	close(fd);
}


static void vol_ln(const char *by, const char *dev, char *link, int linklen)
{
	struct dirent **dir;
	int dirnum, n;
	char *base;

	if (strcmp(by, "id") == 0)
		base = DEV_DISK_BYID;
	else if (strcmp(by, "label") == 0)
		base = DEV_DISK_BYLABEL;
	else if (strcmp(by, "uuid") == 0)
		base = DEV_DISK_BYUUID;
	else
		return;

	dirnum = scandir(base, &dir, 0, versionsort);

	for (n = 0; n < dirnum; n++) {
		char ln[DEV_PATH_MAX];
		char buf[DEV_PATH_MAX];
		int buflen = DEV_PATH_MAX - 1;
		int len, ret;

		if (dir[n]->d_type != DT_LNK)
			continue;

		ret = snprintf(ln, sizeof(ln), "%s/%s",
			       base, dir[n]->d_name);
		if (ret < 0)
			continue;

		len = readlink(ln, buf, buflen);
		if (len < 0)
			continue;
		buf[len] = '\0';

		if (strstr(buf, dev) != NULL) {
			strncpy(link, ln, linklen);
			break;
		}
	}
}

/* -------------------------------------------------------------------------
   part_index
   ----------
   Return partition index (eg input = sda6, return 6).
   ------------------------------------------------------------------------- */
static int part_index(const char *part, int baselen)
{
	int i, j, n;
	int plen = strlen(part);
	int nlen = plen - baselen;
	char *num = malloc(nlen + 1);

	if (num == NULL)
		return 0;

	i = 0;
	while (!isdigit(part[i]) && (i < plen))
		i++;

	j = 0;
	while (i < plen && j < nlen)
		num[j++] = part[i++];
	num[j] = '\0';

	n = atoi(num);
	free(num);

	return n;
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
	dirnum = scandir(sysfs_path, &dir, 0, versionsort);

	for (n = 0; n < dirnum; n++) {
		struct filesystem f, *fs;
		int ret;
		char node[DEV_PATH_MAX];
		char *name = dir[n]->d_name;

		if (strncmp(name, disk, disklen) != 0)
			continue;

		ret = snprintf(node, sizeof(node), "%s/%s",
			       DEV_DIR, name);
		if (ret < 0)
			continue;

		/*
		 * store disk + partition indices
		 */
		f.diskn = diskn + 1;
		f.partn = part_index(name, disklen);

		/*
		 * get volume persistent symlinks
		 */
		vol_ln("id", name, f.by_id, sizeof(f.by_id));
		vol_ln("label", name, f.by_label, sizeof(f.by_label));
		vol_ln("uuid", name, f.by_uuid, sizeof(f.by_uuid));

		/*
		 * get volume_id properties
		 */
		fs = &f;
		vol_id(fs, node);

		/*
		 * debug volume filesystem struct
		 */
		filesystem_debug(fs, name, node);

		/*
		 * skip volumes without usage or type properties
		 */
		if (f.usage == NULL || f.type == NULL)
			continue;

		filesystem_entry(fs, node);
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
	dirnum = scandir(SYS_BLK, &dir, disk_filter, versionsort);

	for (n = 0; n < dirnum; n++) {
		if (opt_debug)
			fprintf(stderr, "---> %s\n", dir[n]->d_name);

		scandisk(dir[n]->d_name, n);
	}

	/*
	 * scan for cdrom device node symlinks
	 */
	dirnum = scandir(DEV_DIR, &dir, cdrom_filter, versionsort);

	for (n = 0; n < dirnum; n++) {
		if (opt_debug)
			fprintf(stderr, "---> %s\n", dir[n]->d_name);

		cdrom_entry(dir[n]->d_name);
	}

	/*
	 * scan for floppy device nodes
	 */
	dirnum = scandir(SYS_BLK, &dir, floppy_filter, versionsort);

	for (n = 0; n < dirnum; n++) {
		if (opt_debug)
			fprintf(stderr, "---> %s\n", dir[n]->d_name);

		floppy_entry(dir[n]->d_name);
	}

	return 0;
}
