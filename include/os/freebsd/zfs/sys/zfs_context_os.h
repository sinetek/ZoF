#ifdef _KERNEL

#include <sys/kcondvar.h>
#include <sys/rwlock.h>
#include <sys/sig.h>
#include_next <sys/sdt.h>
#include <sys/misc.h>
#include <sys/kdb.h>
#include <sys/pathname.h>
#include <sys/conf.h>
/* XXX move us */

#define	cv_wait_io(cv, mp)			cv_wait(cv, mp)
#define	cv_wait_io_sig(cv, mp)			cv_wait_sig(cv, mp)

#define	cond_resched()		kern_yield(PRI_USER)

#include <sys/sysctl.h>
#define	ZMOD_RW CTLFLAG_RWTUN
#define	ZMOD_RD CTLFLAG_RDTUN

#define	ZFS_MODULE_PARAM(scope_prefix, name_prefix, name, type, perm, desc) \
	SYSCTL_DECL(_vfs_ ## scope_prefix);	 \
	SYSCTL_##type(_vfs_ ## scope_prefix, OID_AUTO, name, perm,	\
		&name_prefix ## name, 0, desc)

#define	taskq_create_sysdc(a, b, d, e, p, dc, f) \
	    (taskq_create(a, b, maxclsyspri, d, e, f))

#define	tsd_create(keyp, destructor)    do {                 \
		*(keyp) = osd_thread_register((destructor));         \
		KASSERT(*(keyp) > 0, ("cannot register OSD"));       \
} while (0)
#define	EXPORT_SYMBOL(x)
#define	module_param(a, b, c)
#define	 MODULE_PARM_DESC(a, b)


#define	tsd_destroy(keyp)	osd_thread_deregister(*(keyp))
#define	tsd_get(key)	osd_thread_get(curthread, (key))
#define	tsd_set(key, value)	osd_thread_set(curthread, (key), (value))
#define	fm_panic	panic

#define	cond_resched()		kern_yield(PRI_USER)
extern int zfs_debug_level;
extern struct mtx zfs_debug_mtx;
#define	ZFS_LOG(lvl, ...) do {   \
		if (((lvl) & 0xff) <= zfs_debug_level) {  \
			mtx_lock(&zfs_debug_mtx);			  \
			printf("%s:%u[%d]: ",				  \
			    __func__, __LINE__, (lvl)); \
			printf(__VA_ARGS__); \
			printf("\n"); \
			if ((lvl) & 0x100) \
				kdb_backtrace(); \
			mtx_unlock(&zfs_debug_mtx);	\
	}	   \
} while (0)

#define	MSEC_TO_TICK(msec)	((msec) / (MILLISEC / hz))
extern int hz;
extern int tick;
typedef int fstrans_cookie_t;
#define	spl_fstrans_mark() (0)
#define	spl_fstrans_unmark(x) (x = 0)
#define	signal_pending(x) SIGPENDING(x)
#define	current curthread
#define	thread_join(x)
#define	sys_shutdown rebooting
#define	cv_wait_io(cv, mp)			cv_wait(cv, mp)
typedef struct opensolaris_utsname	utsname_t;
extern utsname_t *utsname(void);
extern int spa_import_rootpool(const char *name);
#else
#if BYTE_ORDER != BIG_ENDIAN
#undef _BIG_ENDIAN
#endif
#endif
