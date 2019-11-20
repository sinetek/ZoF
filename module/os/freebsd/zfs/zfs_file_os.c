/*
 * CDDL HEADER START
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

#include <sys/dmu.h>
#include <sys/dmu_impl.h>
#include <sys/dmu_recv.h>
#include <sys/dmu_tx.h>
#include <sys/dbuf.h>
#include <sys/dnode.h>
#include <sys/zfs_context.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_traverse.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_synctask.h>
#include <sys/zfs_ioctl.h>
#include <sys/zap.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_file.h>
#include <sys/buf.h>
#include <sys/stat.h>

/*
 * zfs_file_open -> filp_open
 * zfs_file_close -> filp_close
 * zfs_file_seek -> vfs_llseek
 * zfs_file_sync -> spl_filp_fsync
 * zfs_file_pwrite -> spl_kernel_write
 * zfs_file_pread -> spl_kernel_read
 * zfs_file_stat -> vfs_getattr
 * zfs_file_unlink -> vfs_unlink
 * zfs_file_get -> fget
 * zfs_file_put -> fput
 */

int
zfs_file_open(const char *path, int flags, int mode, zfs_file_t **fpp)
{
	struct thread *td;
	int rc, fd;

	td = curthread;
	rc = kern_openat(td, AT_FDCWD, path, UIO_SYSSPACE, flags, mode);
	if (rc)
		return (rc);
	fd = td->td_retval[0];
	if (fget(curthread, fd, &cap_no_rights, fpp)) 
		kern_close(td, fd);
	return (0);
}

void
zfs_file_close(zfs_file_t *fp)
{
	fo_close(fp, curthread);
}

static int
zfs_file_write_impl(zfs_file_t *fp, const void *buf, size_t count, loff_t *offp,
    ssize_t *resid)
{
	ssize_t rc;
	struct uio auio;
	struct thread *td;
	struct iovec aiov;

	td = curthread;
	aiov.iov_base = (void *)(uintptr_t)buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = count;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = *offp;

	if (fp->f_type == DTYPE_VNODE)
		bwillwrite();

	rc = fo_write(fp, &auio, td->td_ucred, FOF_OFFSET, td);
	if (resid)
		*resid = auio.uio_resid;
	else if (auio.uio_resid)
		return (EIO);
	*offp += count - auio.uio_resid;
	return (rc);
}

int
zfs_file_write(zfs_file_t *fp, const void *buf, size_t count, ssize_t *resid)
{
	loff_t off = fp->f_offset;
	ssize_t rc;

	rc = zfs_file_write_impl(fp, buf, count, &off, resid);
	if (rc == 0)
		fp->f_offset = off;

	return (rc);
}

int
zfs_file_pwrite(zfs_file_t *fp, const void *buf, size_t count, loff_t off,
    ssize_t *resid)
{			
	return (zfs_file_write_impl(fp, buf, count, &off, resid));
}
	
	
static int
zfs_file_read_impl(zfs_file_t *fp, void *buf, size_t count, loff_t *offp,
    ssize_t *resid)
{
	ssize_t rc;
	struct uio auio;
	struct thread *td;
	struct iovec aiov;

	td = curthread;
	aiov.iov_base = (void *)(uintptr_t)buf;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = count;
	auio.uio_rw = UIO_READ;
	auio.uio_td = td;
	auio.uio_offset = *offp;

	rc = fo_read(fp, &auio, td->td_ucred, FOF_OFFSET, td);
	*resid = auio.uio_resid;
	*offp += count - auio.uio_resid;
	return (rc);
}

int
zfs_file_read(zfs_file_t *fp, void *buf, size_t count, ssize_t *resid)
{
	loff_t off = fp->f_offset;
	ssize_t rc;

	rc = zfs_file_read_impl(fp, buf, count, &off, resid);
	if (rc == 0)
		fp->f_offset = off;
	return (rc);
}

int
zfs_file_pread(zfs_file_t *fp, void *buf, size_t count, loff_t off,
    ssize_t *resid)
{
	return (zfs_file_read_impl(fp, buf, count, &off, resid));
}

int
zfs_file_seek(zfs_file_t *fp, loff_t *offp, int whence)
{
	int rc;
	struct thread *td;

	td = curthread;
	if ((fp->f_ops->fo_flags & DFLAG_SEEKABLE) == 0)
		return (ESPIPE);
	rc = fo_seek(fp, *offp, whence, td);
	if (rc == 0)
		*offp = td->td_uretoff.tdu_off;
	return (rc);
}

int
zfs_file_getattr(zfs_file_t *fp, zfs_file_attr_t *zfattr)
{
	struct thread *td;
	struct stat sb;
	int rc;

	td = curthread;

	rc = fo_stat(fp, &sb, td->td_ucred, td);
	if (rc)
		return (rc);
	zfattr->zfa_size = sb.st_size;
	zfattr->zfa_mode = sb.st_mode;

	return (0);
}

static __inline int
zfs_vop_fsync(vnode_t *vp)
{
	struct mount *mp;
	int error;

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		goto drop;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_FSYNC(vp, MNT_WAIT, curthread);
	VOP_UNLOCK(vp, 0);
	vn_finished_write(mp);
drop:
	return (error);
}

int
zfs_file_fsync(zfs_file_t *fp, int flags)
{
	struct vnode *v;

	if (fp->f_type != DTYPE_VNODE)
		return (EINVAL);

	v = fp->f_data;
	return (zfs_vop_fsync(v));
}

int
zfs_file_get(int fd, zfs_file_t **fpp)
{
	struct file *fp;

	if (fget(curthread, fd, &cap_no_rights, &fp))
		return (EBADF);

	*fpp = fp;
	return (0);
}

void
zfs_file_put(int fd)
{
	struct file *fp;

	/* No CAP_ rights required, as we're only releasing. */
	if (fget(curthread, fd, &cap_no_rights, &fp) == 0) {
		fdrop(fp, curthread);
		fdrop(fp, curthread);
	}
}

loff_t
zfs_file_off(zfs_file_t *fp)
{
	return (fp->f_offset);
}

int
zfs_file_unlink(const char *fnamep)
{
	enum uio_seg seg = UIO_SYSSPACE;
	int rc;

#if __FreeBSD_version >= 1300018
	rc = kern_funlinkat(curthread, AT_FDCWD, fnamep, FD_NONE, seg, 0, 0);
#else
#ifdef AT_BENEATH
	rc = kern_unlinkat(curthread, AT_FDCWD, fnamep, seg, 0, 0);
#else
	rc = kern_unlinkat(curthread, AT_FDCWD, fnamep, seg, 0);
#endif
#endif
	return (rc);
}
