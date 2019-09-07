#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_znode.h>
#include <sys/zap.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/dmu.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_deleg.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_impl.h>
#include <sys/dmu_redact.h>
#include <sys/dmu_tx.h>
#include <sys/sunddi.h>
#include <sys/policy.h>
#include <sys/zone.h>
#include <sys/nvpair.h>
#include <sys/pathname.h>
#include <sys/sdt.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_onexit.h>
#include <sys/zvol.h>
#include <sys/dsl_scan.h>
#include <sys/fm/util.h>
#include <sys/dsl_crypt.h>

#include <sys/dmu_recv.h>
#include <sys/dmu_send.h>
#include <sys/dmu_recv.h>
#include <sys/dsl_destroy.h>
#include <sys/dsl_bookmark.h>
#include <sys/dsl_userhold.h>
#include <sys/zfeature.h>
#include <sys/zcp.h>
#include <sys/zio_checksum.h>
#include <sys/vdev_removal.h>
#include <sys/vdev_trim.h>
#include <sys/vdev_impl.h>
#include <sys/vdev_initialize.h>
#include <sys/zfs_ioctl_os.h>

extern kmutex_t zfsdev_state_lock;
extern void zfs_init(void);
extern void zfs_fini(void);
extern zfsdev_state_t *zfsdev_state_list;

extern uint_t zfs_fsyncer_key;
extern uint_t rrw_tsd_key;
extern uint_t zfs_allow_log_key;

int
zfs_vfs_ref(zfsvfs_t **zfvp)
{
	if (*zfvp == NULL || (*zfvp)->z_sb == NULL ||
	    !atomic_inc_not_zero(&((*zfvp)->z_sb->s_active))) {
		return (SET_ERROR(ESRCH));
	}
	return (0);
}


static int
zfsdev_state_init(struct file *filp)
{
	zfsdev_state_t *zs, *zsprev = NULL;
	minor_t minor;
	boolean_t newzs = B_FALSE;

	ASSERT(MUTEX_HELD(&zfsdev_state_lock));

	minor = zfsdev_minor_alloc();
	if (minor == 0)
		return (SET_ERROR(ENXIO));

	for (zs = zfsdev_state_list; zs != NULL; zs = zs->zs_next) {
		if (zs->zs_minor == -1)
			break;
		zsprev = zs;
	}

	if (!zs) {
		zs = kmem_zalloc(sizeof (zfsdev_state_t), KM_SLEEP);
		newzs = B_TRUE;
	}

	zs->zs_file = filp;
	filp->private_data = zs;

	zfs_onexit_init((zfs_onexit_t **)&zs->zs_onexit);
	zfs_zevent_init((zfs_zevent_t **)&zs->zs_zevent);


	/*
	 * In order to provide for lock-free concurrent read access
	 * to the minor list in zfsdev_get_state_impl(), new entries
	 * must be completely written before linking them into the
	 * list whereas existing entries are already linked; the last
	 * operation must be updating zs_minor (from -1 to the new
	 * value).
	 */
	if (newzs) {
		zs->zs_minor = minor;
		smp_wmb();
		zsprev->zs_next = zs;
	} else {
		smp_wmb();
		zs->zs_minor = minor;
	}

	return (0);
}

static int
zfsdev_state_destroy(struct file *filp)
{
	zfsdev_state_t *zs;

	ASSERT(MUTEX_HELD(&zfsdev_state_lock));
	ASSERT(filp->private_data != NULL);

	zs = filp->private_data;
	zs->zs_minor = -1;
	zfs_onexit_destroy(zs->zs_onexit);
	zfs_zevent_destroy(zs->zs_zevent);

	return (0);
}

static int
zfsdev_open(struct inode *ino, struct file *filp)
{
	int error;

	mutex_enter(&zfsdev_state_lock);
	error = zfsdev_state_init(filp);
	mutex_exit(&zfsdev_state_lock);

	return (-error);
}

static int
zfsdev_release(struct inode *ino, struct file *filp)
{
	int error;

	mutex_enter(&zfsdev_state_lock);
	error = zfsdev_state_destroy(filp);
	mutex_exit(&zfsdev_state_lock);

	return (-error);
}

static long
zfsdev_ioctl_linux(struct file *filp, unsigned cmd, unsigned long arg)
{
	uint_t vecnum;

	vecnum = cmd - ZFS_IOC_FIRST;
	return (zfsdev_ioctl_common(vecnum, arg));
}

int
zfsdev_getminor(struct file *filp, minor_t *minorp)
{
	zfsdev_state_t *zs, *fpd;

	ASSERT(filp != NULL);
	ASSERT(!MUTEX_HELD(&zfsdev_state_lock));

	fpd = filp->private_data;
	if (fpd == NULL)
		return (SET_ERROR(EBADF));

	mutex_enter(&zfsdev_state_lock);

	for (zs = zfsdev_state_list; zs != NULL; zs = zs->zs_next) {

		if (zs->zs_minor == -1)
			continue;

		if (fpd == zs) {
			*minorp = fpd->zs_minor;
			mutex_exit(&zfsdev_state_lock);
			return (0);
		}
	}

	mutex_exit(&zfsdev_state_lock);

	return (SET_ERROR(EBADF));
}

void
zfs_ioctl_init_os(void)
{

}

/* ARGSUSED */
int
zfs_ioc_destroy_snaps(const char *poolname, nvlist_t *innvl, nvlist_t *outnvl)
{
	nvlist_t *snaps;
	nvpair_t *pair;
	boolean_t defer;

	snaps = fnvlist_lookup_nvlist(innvl, "snaps");
	defer = nvlist_exists(innvl, "defer");

	for (pair = nvlist_next_nvpair(snaps, NULL); pair != NULL;
	    pair = nvlist_next_nvpair(snaps, pair)) {
		zfs_unmount_snap(nvpair_name(pair));
	}

	return (dsl_destroy_snapshots_nvl(snaps, defer, outnvl));
}

#ifdef CONFIG_COMPAT
static long
zfsdev_compat_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	return (zfsdev_ioctl_linux(filp, cmd, arg));
}
#else
#define	zfsdev_compat_ioctl	NULL
#endif

static const struct file_operations zfsdev_fops = {
	.open		= zfsdev_open,
	.release	= zfsdev_release,
	.unlocked_ioctl	= zfsdev_ioctl_linux,
	.compat_ioctl	= zfsdev_compat_ioctl,
	.owner		= THIS_MODULE,
};

static struct miscdevice zfs_misc = {
	.minor		= ZFS_DEVICE_MINOR,
	.name		= ZFS_DRIVER,
	.fops		= &zfsdev_fops,
};

MODULE_ALIAS_MISCDEV(ZFS_DEVICE_MINOR);
MODULE_ALIAS("devname:zfs");

static int
zfs_attach(void)
{
	int error;

	mutex_init(&zfsdev_state_lock, NULL, MUTEX_DEFAULT, NULL);
	zfsdev_state_list = kmem_zalloc(sizeof (zfsdev_state_t), KM_SLEEP);
	zfsdev_state_list->zs_minor = -1;

	error = misc_register(&zfs_misc);
	if (error == -EBUSY) {
		/*
		 * Fallback to dynamic minor allocation in the event of a
		 * collision with a reserved minor in linux/miscdevice.h.
		 * In this case the kernel modules must be manually loaded.
		 */
		printk(KERN_INFO "ZFS: misc_register() with static minor %d "
		    "failed %d, retrying with MISC_DYNAMIC_MINOR\n",
		    ZFS_DEVICE_MINOR, error);

		zfs_misc.minor = MISC_DYNAMIC_MINOR;
		error = misc_register(&zfs_misc);
	}

	if (error)
		printk(KERN_INFO "ZFS: misc_register() failed %d\n", error);

	return (error);
}

static void
zfs_detach(void)
{
	zfsdev_state_t *zs, *zsprev = NULL;

	misc_deregister(&zfs_misc);
	mutex_destroy(&zfsdev_state_lock);

	for (zs = zfsdev_state_list; zs != NULL; zs = zs->zs_next) {
		if (zsprev)
			kmem_free(zsprev, sizeof (zfsdev_state_t));
		zsprev = zs;
	}
	if (zsprev)
		kmem_free(zsprev, sizeof (zfsdev_state_t));
}

static void
zfs_allow_log_destroy(void *arg)
{
	char *poolname = arg;

	if (poolname != NULL)
		strfree(poolname);
}

#ifdef DEBUG
#define	ZFS_DEBUG_STR	" (DEBUG mode)"
#else
#define	ZFS_DEBUG_STR	""
#endif

static int __init
_init(void)
{
	int error;

	if ((error = -zvol_init()) != 0)
		return (error);

	spa_init(FREAD | FWRITE);
	zfs_init();

	zfs_ioctl_init();
	zfs_sysfs_init();

	if ((error = zfs_attach()) != 0)
		goto out;

	tsd_create(&zfs_fsyncer_key, NULL);
	tsd_create(&rrw_tsd_key, rrw_tsd_destroy);
	tsd_create(&zfs_allow_log_key, zfs_allow_log_destroy);

	printk(KERN_NOTICE "ZFS: Loaded module v%s-%s%s, "
	    "ZFS pool version %s, ZFS filesystem version %s\n",
	    ZFS_META_VERSION, ZFS_META_RELEASE, ZFS_DEBUG_STR,
	    SPA_VERSION_STRING, ZPL_VERSION_STRING);
#ifndef CONFIG_FS_POSIX_ACL
	printk(KERN_NOTICE "ZFS: Posix ACLs disabled by kernel\n");
#endif /* CONFIG_FS_POSIX_ACL */

	return (0);

out:
	zfs_sysfs_fini();
	zfs_fini();
	spa_fini();
	(void) zvol_fini();
	printk(KERN_NOTICE "ZFS: Failed to Load ZFS Filesystem v%s-%s%s"
	    ", rc = %d\n", ZFS_META_VERSION, ZFS_META_RELEASE,
	    ZFS_DEBUG_STR, error);

	return (error);
}

static void __exit
_fini(void)
{
	zfs_detach();
	zfs_sysfs_fini();
	zfs_fini();
	spa_fini();
	zvol_fini();

	tsd_destroy(&zfs_fsyncer_key);
	tsd_destroy(&rrw_tsd_key);
	tsd_destroy(&zfs_allow_log_key);

	printk(KERN_NOTICE "ZFS: Unloaded module v%s-%s%s\n",
	    ZFS_META_VERSION, ZFS_META_RELEASE, ZFS_DEBUG_STR);
}

#if defined(_KERNEL) && defined(__linux__)
module_init(_init);
module_exit(_fini);

MODULE_DESCRIPTION("ZFS");
MODULE_AUTHOR(ZFS_META_AUTHOR);
MODULE_LICENSE(ZFS_META_LICENSE);
MODULE_VERSION(ZFS_META_VERSION "-" ZFS_META_RELEASE);
#endif
