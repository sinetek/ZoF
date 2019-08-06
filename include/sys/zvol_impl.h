#ifndef	_SYS_ZVOL_IMPL_H
#define	_SYS_ZVOL_IMPL_H

#include <sys/zfs_context.h>

#ifndef __linux__
struct gendisk {
	void *private_data;
};

struct hlist_head {
        struct hlist_node *first;
};

struct hlist_node {
        struct hlist_node *next, **pprev;
};

typedef struct {
        volatile int counter;
} atomic_t;

#include <sys/bio.h>

typedef struct cdev *platform_dev_t;

#define hlist_for_each(p, head)                                         \
        for (p = (head)->first; p; p = (p)->next)

#define hlist_entry(ptr, type, field)   container_of(ptr, type, field)

#define container_of(ptr, type, member)                         \
({                                                              \
        const __typeof(((type *)0)->member) *__p = (ptr);       \
        (type *)((uintptr_t)__p - offsetof(type, member));      \
})

static inline void
hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{

        n->next = h->first;
        if (h->first != NULL)
                h->first->pprev = &n->next;
        WRITE_ONCE(h->first, n);
        n->pprev = &h->first;
}

static inline void
hlist_del(struct hlist_node *n)
{

        WRITE_ONCE(*(n->pprev), n->next);
        if (n->next != NULL)
                n->next->pprev = n->pprev;
}


#define	READ_ONCE(x) ({			\
	__typeof(x) __var = ({		\
		barrier();		\
		ACCESS_ONCE(x);		\
	});				\
	barrier();			\
	__var;				\
})

#define HLIST_HEAD_INIT { }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT
#define INIT_HLIST_HEAD(head) (head)->first = NULL

#define INIT_HLIST_NODE(node)					\
	do {																\
		(node)->next = NULL;											\
		(node)->pprev = NULL;											\
	} while (0)


static inline int
atomic_read(const atomic_t *v)
{
	return READ_ONCE(v->counter);
}

static inline int
atomic_inc(atomic_t *v)
{
	return atomic_fetchadd_int(&v->counter, 1) + 1;
}

static inline int
atomic_dec(atomic_t *v)
{
	return atomic_fetchadd_int(&v->counter, -1) - 1;
}

#else
typedef dev_t platform_dev_t;

#endif

#define	ZVOL_RDONLY	0x1
/*
 * Whether the zvol has been written to (as opposed to ZVOL_RDONLY, which
 * specifies whether or not the zvol _can_ be written to)
 */
#define	ZVOL_WRITTEN_TO	0x2

#define	ZVOL_DUMPIFIED	0x4

#define	ZVOL_EXCL	0x8

/*
 * The in-core state of each volume.
 */
struct zvol_state {
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
	struct gendisk		*zv_disk;	/* generic disk */
	list_node_t		zv_next;	/* next zvol_state_t linkage */
	uint64_t		zv_hash;	/* name hash */
	struct hlist_node	zv_hlink;	/* hash link */
	kmutex_t		zv_state_lock;	/* protects zvol_state_t */
	atomic_t		zv_suspend_ref;	/* refcount for suspend */
	krwlock_t		zv_suspend_lock;	/* suspend lock */
	platform_dev_t			zv_dev;		/* device id */

#ifdef __linux__
	struct request_queue	*zv_queue;	/* request queue */
	dataset_kstats_t	zv_kstat;	/* zvol kstats */
#endif
#ifdef __FreeBSD__
	struct g_provider *zv_provider;	/* GEOM provider */
	struct bio_queue_head zv_queue;
	int zv_state;
	int zv_sync_cnt;
	uint64_t zv_volmode;
#endif
};


extern list_t zvol_state_list;
extern taskq_t *zvol_taskq;
extern krwlock_t zvol_state_lock;
#define	ZVOL_HT_SIZE	1024
extern struct hlist_head *zvol_htable;
#define	ZVOL_HT_HEAD(hash)	(&zvol_htable[(hash) & (ZVOL_HT_SIZE-1)])
extern zil_replay_func_t *zvol_replay_vector[TX_MAX_TYPE];

extern unsigned int zvol_volmode;
extern unsigned int zvol_inhibit_dev;
extern unsigned int zvol_threads;
/*
 * platform independent functions exported to platform code
 */

zvol_state_t *zvol_find_by_name_hash(const char *name, uint64_t hash, int mode);
int zvol_first_open(zvol_state_t *zv, boolean_t readonly);
uint64_t zvol_name_hash(const char *name);
void zvol_remove_minors_impl(const char *name);
void zvol_last_close(zvol_state_t *zv);
zvol_state_t *zvol_find_by_dev(platform_dev_t dev);
void zvol_insert(zvol_state_t *zv);
void zvol_log_truncate(zvol_state_t *zv, dmu_tx_t *tx, uint64_t off, uint64_t len,
    boolean_t sync);
void zvol_log_write(zvol_state_t *zv, dmu_tx_t *tx, uint64_t offset,
    uint64_t size, int sync);
int zvol_get_data(void *arg, lr_write_t *lr, char *buf, struct lwb *lwb, zio_t *zio);

/*
 * platform dependent functions exported to platform independent code
 */

void zvol_rename_minor(zvol_state_t *zv, const char *newname);
int zvol_setup_zv(zvol_state_t *zv);
void zvol_free(void *arg);
int zvol_create_minor_impl(const char *name);

extern int zvol_init_os(void);
extern void zvol_fini_os(void);

#endif
