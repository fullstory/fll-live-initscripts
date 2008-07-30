/*
 *  Copyright (c) 2008 Luis R. Rodriguez <lrodriguez@atheros.com>
 *            (c) 2008 Kel Modderman <kel@otaku42.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * ---
 *
 * There is no standard way yet to check if an interface is an 802.11 device.
 *
 * For example the Linux HAL used to use check the existance of:
 *
 *   /sys/class/net/<interface>/wireless/
 *
 * directory if a device is wireless. Wireless-tools and Network manager
 * rely on calling Wireless-extensions IOCTLs (SIOCGIWRANGE and SIOCGIWNAME,
 * respectively) which drivers seem to always implement.
 *
 * All of these interfaces are wireless-extensions specific and may not be
 * ported later to cfg80211. Our new communication interface uses nl80211
 * (linux/nl80211.h) so you should be able to rely on it for new devices.
 *
 * This is a small application which uses the old wireless-extensions
 * and nl80211 to determine if an interface is 802.11. I'll try to keep
 * it as up to date and as small as possible.
 *
 * http://www.kernel.org/pub/linux/kernel/people/mcgrof/is80211.c
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/wireless.h>

int main(int argc, char **argv)
{
	int r, fd;
	struct iwreq iwr;

	if (argc != 2) {
		fprintf(stderr, "Usage: fll_is80211 <ifname>\n");
		return 1;
	}

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return 1;

	strncpy(iwr.ifr_ifrn.ifrn_name, argv[1], IFNAMSIZ);
	r = ioctl(fd, SIOCGIWNAME, &iwr);
	close(fd);

	return !(r == 0);
}
