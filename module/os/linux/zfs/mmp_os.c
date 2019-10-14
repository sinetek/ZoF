#include <sys/zfs_context.h>
#include <sys/mmp.h>

static int
param_set_multihost_interval(const char *val, zfs_kernel_param_t *kp)
{
	int ret;

	ret = param_set_ulong(val, kp);
	if (ret < 0)
		return (ret);

	if (spa_mode_global != 0)
		mmp_signal_all_threads();

	return (ret);
}

module_param_call(zfs_multihost_interval, param_set_multihost_interval,
    param_get_ulong, &zfs_multihost_interval, 0644);
MODULE_PARM_DESC(zfs_multihost_interval,
	"Milliseconds between mmp writes to each leaf");
