#ifndef _SYS_ZFS_VNOPS_H_
#define	_SYS_ZFS_VNOPS_H_
int dmu_write_pages(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t size, struct vm_page **ppa, dmu_tx_t *tx);
int dmu_read_pages(objset_t *os, uint64_t object, vm_page_t *ma, int count,
    int *rbehind, int *rahead, int last_size);
extern int zfs_remove(znode_t *dzp, char *name, cred_t *cr, int flags);
extern int zfs_mkdir(znode_t *dzp, char *dirname, vattr_t *vap,
    znode_t **zpp, cred_t *cr, int flags, vsecattr_t *vsecp);
extern int zfs_rmdir(znode_t *dzp, char *name, znode_t *cwd,
    cred_t *cr, int flags);
extern int zfs_setattr(znode_t *zp, vattr_t *vap, int flag, cred_t *cr);
extern int zfs_rename(znode_t *sdzp, char *snm, znode_t *tdzp,
    char *tnm, cred_t *cr, int flags);
extern int zfs_symlink(znode_t *dzp, const char *name, vattr_t *vap,
    const char *link, znode_t **zpp, cred_t *cr, int flags);
extern int zfs_link(znode_t *tdzp, znode_t *sp,
    char *name, cred_t *cr, int flags);
extern int zfs_space(znode_t *zp, int cmd, struct flock *bfp, int flag,
    offset_t offset, cred_t *cr);
extern int zfs_create(znode_t *dzp, char *name, vattr_t *vap, int excl,
    int mode, znode_t **zpp, cred_t *cr, int flag, vsecattr_t *vsecp);
extern int zfs_setsecattr(znode_t *zp, vsecattr_t *vsecp, int flag,
    cred_t *cr);
ssize_t
zpl_write_common(vnode_t *vp, char *buf, size_t len, loff_t *ppos,
    uio_seg_t segment, int flags, cred_t *cr);
#endif
