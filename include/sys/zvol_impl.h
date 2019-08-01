#ifndef	_SYS_ZVOL_IMPL_H
#define	_SYS_ZVOL_IMPL_H

#include <sys/zfs_context.h>

#ifndef __linux__
struct gendisk {
	void *private_data;
};
#endif

#define	ZVOL_RDONLY	0x1
/*
 * Whether the zvol has been written to (as opposed to ZVOL_RDONLY, which
 * specifies whether or not the zvol _can_ be written to)
 */
#define	ZVOL_WRITTEN_TO	0x2



/*
 * The in-core state of each volume.
 */
typedef struct zvol_state {
	char			zv_name[MAXNAMELEN];	/* name */
	uint64_t		zv_volsize;		/* advertised space */
	uint64_t		zv_volblocksize;	/* volume block size */
	objset_t		*zv_objset;	/* objset handle */
	uint32_t		zv_flags;	/* ZVOL_* flags */
	uint32_t		zv_open_count;	/* open counts */
	uint32_t		zv_changed;	/* disk changed */
	zilog_t			*zv_zilog;	/* ZIL handle */
	rangelock_t		zv_rangelock;	/* for range locking */
	dnode_t			*zv_dn;		/* dnode hold */
	dev_t			zv_dev;		/* device id */
	struct gendisk		*zv_disk;	/* generic disk */
#ifdef __linux__
	struct request_queue	*zv_queue;	/* request queue */
	dataset_kstats_t	zv_kstat;	/* zvol kstats */
#endif
	list_node_t		zv_next;	/* next zvol_state_t linkage */
	uint64_t		zv_hash;	/* name hash */
	struct hlist_node	zv_hlink;	/* hash link */
	kmutex_t		zv_state_lock;	/* protects zvol_state_t */
	atomic_t		zv_suspend_ref;	/* refcount for suspend */
	krwlock_t		zv_suspend_lock;	/* suspend lock */
} zvol_state_t;

extern list_t zvol_state_list;
extern taskq_t *zvol_taskq;
extern krwlock_t zvol_state_lock;
#define	ZVOL_HT_SIZE	1024
extern struct hlist_head *zvol_htable;
#define	ZVOL_HT_HEAD(hash)	(&zvol_htable[(hash) & (ZVOL_HT_SIZE-1)])
extern zil_replay_func_t *zvol_replay_vector[TX_MAX_TYPE];

extern unsigned int zvol_volmode;
extern unsigned int zvol_inhibit_dev;

uint64_t zvol_name_hash(const char *name);
zvol_state_t *zvol_find_by_name_hash(const char *name, uint64_t hash, int mode);
void zvol_rename_minor(zvol_state_t *zv, const char *newname);
int zvol_setup_zv(zvol_state_t *zv);
void zvol_free(void *arg);
int zvol_create_minor_impl(const char *name);
void zvol_remove_minors_impl(const char *name);
void zvol_last_close(zvol_state_t *zv);
int zvol_first_open(zvol_state_t *zv, boolean_t readonly);
zvol_state_t *zvol_find_by_dev(dev_t dev);
void zvol_insert(zvol_state_t *zv);
int zvol_get_data(void *arg, lr_write_t *lr, char *buf, struct lwb *lwb, zio_t *zio);
void zvol_log_truncate(zvol_state_t *zv, dmu_tx_t *tx, uint64_t off, uint64_t len,
    boolean_t sync);
void zvol_log_write(zvol_state_t *zv, dmu_tx_t *tx, uint64_t offset,
    uint64_t size, int sync);

#endif
