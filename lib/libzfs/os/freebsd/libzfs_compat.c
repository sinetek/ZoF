/*
 * CDDL HEADER SART
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2013 Martin Matuska <mm@FreeBSD.org>. All rights reserved.
 */

#include <libzfs_compat.h>
#include <sys/sysctl.h>

#include <sys/linker.h>
#include <sys/module.h>

int zfs_ioctl_version = ZFS_IOCVER_UNDEF;
// static int zfs_spa_version = -1;

/*
 * Get zfs_ioctl_version
 */
int
get_zfs_ioctl_version(void)
{
	size_t ver_size;
	int ver = ZFS_IOCVER_NONE;

	ver_size = sizeof (ver);
	sysctlbyname("vfs.zfs.version.ioctl", &ver, &ver_size, NULL, 0);

	return (ver);
}
#if 0
/*
 * Get the SPA version
 */
static int
get_zfs_spa_version(void)
{
	size_t ver_size;
	int ver = 0;

	ver_size = sizeof (ver);
	sysctlbyname("vfs.zfs.version.spa", &ver, &ver_size, NULL, 0);

	return (ver);
}
#endif
/*
 * This is FreeBSD version of ioctl, because Solaris' ioctl() updates
 * zc_nvlist_dst_size even if an error is returned, on FreeBSD if an
 * error is returned zc_nvlist_dst_size won't be updated.
 */
static int
zcmd_ioctl(int fd, int request, zfs_cmd_t *zc)
{
	size_t oldsize;
	int ret, cflag = ZFS_CMD_COMPAT_NONE;

	oldsize = zc->zc_nvlist_dst_size;
	ret = zcmd_ioctl_compat(fd, request, zc, cflag);

	if (ret == 0 && oldsize < zc->zc_nvlist_dst_size) {
		ret = -1;
		errno = ENOMEM;
	}

	return (ret);
}

const char *
libzfs_error_init(int error)
{

	return (strerror(error));
}

int
zfs_ioctl(libzfs_handle_t *hdl, int request, zfs_cmd_t *zc)
{
	return (zcmd_ioctl(hdl->libzfs_fd, request, zc));
}

/*
 * Verify the required ZFS_DEV device is available and optionally attempt
 * to load the ZFS modules.  Under normal circumstances the modules
 * should already have been loaded by some external mechanism.
 *
 * Environment variables:
 * - ZFS_MODULE_LOADING="YES|yes|ON|on" - Attempt to load modules.
 * - ZFS_MODULE_TIMEOUT="<seconds>"     - Seconds to wait for ZFS_DEV
 */
int
libzfs_load_module(const char *module)
{
	if (modfind(ZFS_DRIVER) < 0) {
		/* Not present in kernel, try loading it. */
		if (kldload(module) < 0 && errno != EEXIST) {
			return (errno);
		}
	}
	return (0);
}

int
zpool_relabel_disk(libzfs_handle_t *hdl, const char *path, const char *msg)
{
	return (0);
}

int
zpool_label_disk(libzfs_handle_t *hdl, zpool_handle_t *zhp, char *name)
{
	return (0);
}

int
find_shares_object(differ_info_t *di)
{
	return (0);
}

/*
 * Attach/detach the given filesystem to/from the given jail.
 */
int
zfs_jail(zfs_handle_t *zhp, int jailid, int attach)
{
	libzfs_handle_t *hdl = zhp->zfs_hdl;
	zfs_cmd_t zc = { { 0 } };
	char errbuf[1024];
	unsigned long cmd;
	int ret;

	if (attach) {
		(void) snprintf(errbuf, sizeof (errbuf),
		    dgettext(TEXT_DOMAIN, "cannot jail '%s'"), zhp->zfs_name);
	} else {
		(void) snprintf(errbuf, sizeof (errbuf),
		    dgettext(TEXT_DOMAIN, "cannot unjail '%s'"), zhp->zfs_name);
	}

	switch (zhp->zfs_type) {
	case ZFS_TYPE_VOLUME:
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "volumes can not be jailed"));
		return (zfs_error(hdl, EZFS_BADTYPE, errbuf));
	case ZFS_TYPE_SNAPSHOT:
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "snapshots can not be jailed"));
		return (zfs_error(hdl, EZFS_BADTYPE, errbuf));
	case ZFS_TYPE_BOOKMARK:
		zfs_error_aux(hdl, dgettext(TEXT_DOMAIN,
		    "bookmarks can not be jailed"));
		return (zfs_error(hdl, EZFS_BADTYPE, errbuf));
	case ZFS_TYPE_POOL:
	case ZFS_TYPE_FILESYSTEM:
		/* OK */
		;
	}
	assert(zhp->zfs_type == ZFS_TYPE_FILESYSTEM);

	(void) strlcpy(zc.zc_name, zhp->zfs_name, sizeof (zc.zc_name));
	zc.zc_objset_type = DMU_OST_ZFS;
	zc.zc_jailid = jailid;

	cmd = attach ? ZFS_IOC_JAIL : ZFS_IOC_UNJAIL;
	if ((ret = ioctl(hdl->libzfs_fd, cmd, &zc)) != 0)
		zfs_standard_error(hdl, errno, errbuf);

	return (ret);
}

