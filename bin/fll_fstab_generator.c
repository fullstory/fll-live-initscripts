/*
 *  Copyright (C) 2010  Kel Modderman <kel@otaku42.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libudev.h>

#include "cmdline.h"

struct gengetopt_args_info args;
FILE *fstab;

static int device_wanted(struct udev_device *device, unsigned int wanted,
			 char **w)
{
	struct udev_device *parent = device;
	struct udev_list_entry *u_list_ent;
	struct udev_list_entry *u_first_list_ent;
	const char *devnode;
	int i;

	if (!wanted)
		return 0;

	do {
		devnode = udev_device_get_devnode(parent);
		for (i = 0; i < wanted; ++i)
			if (strcmp(devnode, w[i]) == 0)
				return 1;

		u_first_list_ent = udev_device_get_devlinks_list_entry(parent);
		udev_list_entry_foreach(u_list_ent, u_first_list_ent) {
			devnode = udev_list_entry_get_name(u_list_ent);
			for (i = 0; i < wanted; ++i)
				if (strcmp(devnode, w[i]) == 0)
					return 1;
		}
		parent = udev_device_get_parent_with_subsystem_devtype(parent,
								       "block",
								       "disk");
	} while (parent != NULL);

	return 0;
}

static int device_removable(struct udev_device *device)
{
	struct udev_device *parent = device;
	const char *removable;

	if (args.removable_flag)
		return 0;

	do {
		removable = udev_device_get_sysattr_value(parent,
							  "removable");
		if (removable != NULL)
			return atoi(removable);
		parent = udev_device_get_parent_with_subsystem_devtype(parent,
								       "block",
								       "disk");
	} while (parent != NULL);

	return 1;
}

static int device_devtype_disk(struct udev_device *device)
{
	const char *devtype;

	devtype =  udev_device_get_devtype(device);
	if (devtype != NULL)
		return strcmp(devtype, "disk") == 0;
	
	return 0;
}

static int device_devmapper(struct udev_device *device)
{
	const char *dm_name;

	dm_name = udev_device_get_property_value(device, "DM_NAME");
	if (dm_name != NULL)
		return 1;
	else
		return 0;
}

static void print_mntent(const char *fs_spec, const char *fs_file,
			 const char *fs_vfstype, const char *fs_mntops,
			 int fs_freq, int fs_passno)
{
	if (args.uuids_flag)
		fprintf(fstab, "%-45s %-20s %-10s %-45s %d %d\n", fs_spec,
			fs_file, fs_vfstype, fs_mntops, fs_freq, fs_passno);
	else
		fprintf(fstab, "%-20s %-20s %-10s %-45s %d %d\n", fs_spec,
			fs_file, fs_vfstype, fs_mntops, fs_freq, fs_passno);
}

static int linux_filesystem(const char *fstype)
{
	if (strcmp(fstype, "swap") == 0)
		return 1;
	if (strcmp(fstype, "ext4") == 0)
		return 1;
	if (strcmp(fstype, "ext3") == 0)
		return 1;
	if (strcmp(fstype, "ext2") == 0)
		return 1;
	if (strcmp(fstype, "xfs") == 0)
		return 1;
	if (strcmp(fstype, "jfs") == 0)
		return 1;
	if (strcmp(fstype, "reiserfs") == 0)
		return 1;
	if (strcmp(fstype, "reiser4") == 0)
		return 1;
	
	return 0;
}

static const char* parse_mounts(struct udev_device *device, const char *fs_type)
{
	struct udev_list_entry *u_list_ent;
	struct udev_list_entry *u_first_list_ent;
	struct mntent *mnt;
	FILE *fp;
	const char *fs_spec;
	const char *fs_file;
	const char *devnode;

	fp = setmntent ("/proc/mounts", "r");
	if (fp == NULL)
		return NULL;
	
	devnode = udev_device_get_devnode(device);
	u_first_list_ent = udev_device_get_devlinks_list_entry(device);

	for (;;) {
		mnt = getmntent(fp);
		if (mnt == NULL)
			break;

		if (strcmp(fs_type, mnt->mnt_type) != 0)
			continue;

		if (strcmp(devnode, mnt->mnt_fsname) == 0) {
			fs_file = mnt->mnt_dir;
			break;
		}

		udev_list_entry_foreach(u_list_ent, u_first_list_ent) {
			fs_spec = udev_list_entry_get_name(u_list_ent);
			if (strcmp(fs_spec, mnt->mnt_fsname) == 0) {
				fs_file = mnt->mnt_dir;
				break;
			}
		}

		if (fs_file != NULL)
			break;
	}
	endmntent(fp);

	return fs_file;
}

static void process_disk(struct udev_device *device, int disk)
{
	struct udev_list_entry *u_list_ent;
	struct udev_list_entry *u_first_list_ent;
	const char *devnode;
	const char *symlink;
	const char *part;
	const char *fstype;
	const char *mntpnt;
	const char *defops;
	char ops[256];
	char dir[256];
	int freq = 0;
	int pass = 0;
	int linux_fstype;

	devnode = udev_device_get_devnode(device);
	if (devnode == NULL)
		return;

	fstype = udev_device_get_property_value(device, "ID_FS_TYPE");
	if (fstype == NULL)
		return;

	linux_fstype = linux_filesystem(fstype);

	if ((args.labels_flag || args.uuids_flag) && disk) {
		u_first_list_ent = udev_device_get_devlinks_list_entry(device);
		udev_list_entry_foreach(u_list_ent, u_first_list_ent) {
			symlink = udev_list_entry_get_name(u_list_ent);
			if (args.labels_flag &&
			    strncmp(symlink, "/dev/disk/by-label",
			    	    strlen("/dev/disk/by-label")) == 0) {
				char label[256];
				snprintf(label, sizeof(label), "LABEL=%s",
					 basename(symlink));
				devnode = label;
			}
			if (args.uuids_flag &&
			    strncmp(symlink, "/dev/disk/by-uuid",
				    strlen("/dev/disk/by-uuid")) == 0) {
				char uuid[256];
				snprintf(uuid, sizeof(uuid), "UUID=%s",
					 basename(symlink));
				devnode = uuid;
			}
		}
	}

	if (strcmp(fstype, "swap") == 0) {
		if (!args.noswap_flag)
			print_mntent(devnode, "none", fstype, "sw", 0, 0);
		return;
	}

	mntpnt = args.nomounts_flag ? NULL : parse_mounts(device, fstype);

	if (mntpnt != NULL && linux_fstype) {
		snprintf(dir, sizeof(dir), mntpnt);

		if (strcmp(fstype, "ext4") == 0)
			snprintf(ops, sizeof(ops), "%s,%s",
				"defaults,errors=remount-ro",
				"noatime,barrier=0");
		else if (strncmp(fstype, "ext", 3) == 0)
			snprintf(ops, sizeof(ops), "%s,%s",
				"defaults,errors=remount-ro",
				"noatime");
		else
			snprintf(ops, sizeof(ops), "%s",
				"defaults,noatime");
		
		if (strcmp(mntpnt, "/") == 0)
			pass = 1;
		else
			pass = 2;
	}
	else {
		if (!disk) {
			snprintf(dir, sizeof(dir), "/media/%s",
				 basename(devnode));
		}
		else {
			part = udev_device_get_sysattr_value(device,
							     "partition");
			if (part != NULL)
				snprintf(dir, sizeof(dir),
					 "/media/disk%dpart%s", disk, part);
			else
				snprintf(dir, sizeof(dir),
					 "/media/disk%d", disk);
		}

		defops = args.auto_flag ? "auto" : "noauto";

		if (strcmp(fstype, "ext4") == 0)
			snprintf(ops, sizeof(ops), "%s,%s", defops,
				 "users,rw,exec,noatime,barrier=0");
		else if (linux_fstype)
			snprintf(ops, sizeof(ops), "%s,%s", defops,
				 "users,rw,exec,noatime");
		else if (strcmp(fstype, "ntfs") == 0)
			snprintf(ops, sizeof(ops), "%s,%s", defops,
				 "users,ro,dmask=0022,fmask=0133,nls=utf8");
		else if (strcmp(fstype, "msdos") == 0)
			snprintf(ops, sizeof(ops), "%s,%s", defops,
				 "users,rw,quiet,umask=000,iocharset=utf8");
		else if (strcmp(fstype, "vfat") == 0)
			snprintf(ops, sizeof(ops), "%s,%s", defops,
				 "users,rw,quiet,umask=000,shortname=lower");
		else if (strcmp(fstype, "hfsplus") == 0)
			snprintf(ops, sizeof(ops), "%s,%s", defops,
				 "users,ro,exec");
		else
			return;
	}

	if (args.mkdir_flag && 
	    mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
		if (errno != EEXIST)
			fprintf(stderr, "Error: mkdir(%s): %s\n", dir,
				strerror(errno));
	}

	print_mntent(devnode, dir, fstype, ops, freq, pass);
}

int main(int argc, char **argv)
{
        struct udev *udev;
	struct udev_enumerate *u_enum;
        struct udev_list_entry *u_list_ent;
        struct udev_list_entry *u_first_list_ent;
	int disk = 0;

	if (cmdline_parser(argc, argv, &args) != 0) {
		fprintf(stderr, "Error: cmdline_parser(argc, argv, &args)\n");
		return 1;
	}

	if (args.file_given) {
		fstab = fopen(args.file_arg, "w");
		if (fstab == NULL) {
			fprintf(stderr, "Error: fpopen(%s): %s\n",
				args.file_arg, strerror(errno));
			cmdline_parser_free(&args);
			return 1;
		}
	}
	else
		fstab = stdout;

	udev = udev_new();
	if (udev == NULL) {
		fprintf(stderr, "Error: udev_new()\n");
		cmdline_parser_free(&args);
		fclose(fstab);
		return 1;
	}

	u_enum = udev_enumerate_new(udev);
	if (u_enum == NULL) {
		fprintf(stderr, "Error: udev_enumerate_new(udev)\n");
		cmdline_parser_free(&args);
		fclose(fstab);
		udev_unref(udev);
		return 1;
	}

	udev_enumerate_add_match_subsystem(u_enum, "block");
	udev_enumerate_add_match_property(u_enum, "ID_TYPE", "disk");
	udev_enumerate_add_match_property(u_enum, "DEVNAME", "/dev/mapper/*");
	udev_enumerate_scan_devices(u_enum);

	u_first_list_ent = udev_enumerate_get_list_entry(u_enum);
	udev_list_entry_foreach(u_list_ent, u_first_list_ent) {
		struct udev_device *device;
		struct udev *context;
		const char *name;

		context = udev_enumerate_get_udev(u_enum);
		name = udev_list_entry_get_name(u_list_ent);
		device = udev_device_new_from_syspath(context, name);
		if (device == NULL)
			continue;

		if (args.inputs_num) {
			if (!device_wanted(device, args.inputs_num,
					   args.inputs))
				continue;
		}
		else if (device_removable(device)) {
			if (!device_wanted(device, args.wanted_given,
					   args.wanted_arg))
				continue;
		}

		if (device_devtype_disk(device))
			disk++;

		if (device_devmapper(device))
			process_disk(device, 0);
		else
			process_disk(device, disk);
	}
	udev_enumerate_unref(u_enum);
	udev_unref(udev);
	fclose(fstab);
	cmdline_parser_free(&args);
	return 0;
}
