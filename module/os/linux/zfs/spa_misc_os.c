
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
/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2011, 2019 by Delphix. All rights reserved.
 * Copyright 2015 Nexenta Systems, Inc.  All rights reserved.
 * Copyright (c) 2014 Spectra Logic Corporation, All rights reserved.
 * Copyright 2013 Saso Kiselkov. All rights reserved.
 * Copyright (c) 2017 Datto Inc.
 * Copyright (c) 2017, Intel Corporation.
 */

#include <sys/zfs_context.h>
#include <sys/spa_impl.h>
#include <sys/txg.h>
#include <sys/avl.h>
#include <sys/unique.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_prop.h>
#include <sys/fm/util.h>
#include <sys/dsl_scan.h>
#include <sys/fs/zfs.h>
#include <sys/kstat.h>
#include "zfs_prop.h"


/*
 * ==========================================================================
 * SPA Import Progress Routines
 * ==========================================================================
 */

typedef struct spa_import_progress {
	uint64_t		pool_guid;	/* unique id for updates */
	char			*pool_name;
	spa_load_state_t	spa_load_state;
	uint64_t		mmp_sec_remaining;	/* MMP activity check */
	uint64_t		spa_load_max_txg;	/* rewind txg */
	procfs_list_node_t	smh_node;
} spa_import_progress_t;

spa_history_list_t *spa_import_progress_list = NULL;

static int
spa_import_progress_show_header(struct seq_file *f)
{
	seq_printf(f, "%-20s %-14s %-14s %-12s %s\n", "pool_guid",
	    "load_state", "multihost_secs", "max_txg",
	    "pool_name");
	return (0);
}

static int
spa_import_progress_show(struct seq_file *f, void *data)
{
	spa_import_progress_t *sip = (spa_import_progress_t *)data;

	seq_printf(f, "%-20llu %-14llu %-14llu %-12llu %s\n",
	    (u_longlong_t)sip->pool_guid, (u_longlong_t)sip->spa_load_state,
	    (u_longlong_t)sip->mmp_sec_remaining,
	    (u_longlong_t)sip->spa_load_max_txg,
	    (sip->pool_name ? sip->pool_name : "-"));

	return (0);
}

/* Remove oldest elements from list until there are no more than 'size' left */
static void
spa_import_progress_truncate(spa_history_list_t *shl, unsigned int size)
{
	spa_import_progress_t *sip;
	while (shl->size > size) {
		sip = list_remove_head(&shl->procfs_list.pl_list);
		if (sip->pool_name)
			spa_strfree(sip->pool_name);
		kmem_free(sip, sizeof (spa_import_progress_t));
		shl->size--;
	}

	IMPLY(size == 0, list_is_empty(&shl->procfs_list.pl_list));
}

void
spa_import_progress_init(void)
{
	spa_import_progress_list = kmem_zalloc(sizeof (spa_history_list_t),
	    KM_SLEEP);

	spa_import_progress_list->size = 0;

	spa_import_progress_list->procfs_list.pl_private =
	    spa_import_progress_list;

	procfs_list_install("zfs",
	    "import_progress",
	    0644,
	    &spa_import_progress_list->procfs_list,
	    spa_import_progress_show,
	    spa_import_progress_show_header,
	    NULL,
	    offsetof(spa_import_progress_t, smh_node));
}

void
spa_import_progress_destroy(void)
{
	spa_history_list_t *shl = spa_import_progress_list;
	procfs_list_uninstall(&shl->procfs_list);
	spa_import_progress_truncate(shl, 0);
	procfs_list_destroy(&shl->procfs_list);
	kmem_free(shl, sizeof (spa_history_list_t));
}

int
spa_import_progress_set_state(uint64_t pool_guid,
    spa_load_state_t load_state)
{
	spa_history_list_t *shl = spa_import_progress_list;
	spa_import_progress_t *sip;
	int error = ENOENT;

	if (shl->size == 0)
		return (0);

	mutex_enter(&shl->procfs_list.pl_lock);
	for (sip = list_tail(&shl->procfs_list.pl_list); sip != NULL;
	    sip = list_prev(&shl->procfs_list.pl_list, sip)) {
		if (sip->pool_guid == pool_guid) {
			sip->spa_load_state = load_state;
			error = 0;
			break;
		}
	}
	mutex_exit(&shl->procfs_list.pl_lock);

	return (error);
}

int
spa_import_progress_set_max_txg(uint64_t pool_guid, uint64_t load_max_txg)
{
	spa_history_list_t *shl = spa_import_progress_list;
	spa_import_progress_t *sip;
	int error = ENOENT;

	if (shl->size == 0)
		return (0);

	mutex_enter(&shl->procfs_list.pl_lock);
	for (sip = list_tail(&shl->procfs_list.pl_list); sip != NULL;
	    sip = list_prev(&shl->procfs_list.pl_list, sip)) {
		if (sip->pool_guid == pool_guid) {
			sip->spa_load_max_txg = load_max_txg;
			error = 0;
			break;
		}
	}
	mutex_exit(&shl->procfs_list.pl_lock);

	return (error);
}

int
spa_import_progress_set_mmp_check(uint64_t pool_guid,
    uint64_t mmp_sec_remaining)
{
	spa_history_list_t *shl = spa_import_progress_list;
	spa_import_progress_t *sip;
	int error = ENOENT;

	if (shl->size == 0)
		return (0);

	mutex_enter(&shl->procfs_list.pl_lock);
	for (sip = list_tail(&shl->procfs_list.pl_list); sip != NULL;
	    sip = list_prev(&shl->procfs_list.pl_list, sip)) {
		if (sip->pool_guid == pool_guid) {
			sip->mmp_sec_remaining = mmp_sec_remaining;
			error = 0;
			break;
		}
	}
	mutex_exit(&shl->procfs_list.pl_lock);

	return (error);
}

/*
 * A new import is in progress, add an entry.
 */
void
spa_import_progress_add(spa_t *spa)
{
	spa_history_list_t *shl = spa_import_progress_list;
	spa_import_progress_t *sip;
	char *poolname = NULL;

	sip = kmem_zalloc(sizeof (spa_import_progress_t), KM_SLEEP);
	sip->pool_guid = spa_guid(spa);

	(void) nvlist_lookup_string(spa->spa_config, ZPOOL_CONFIG_POOL_NAME,
	    &poolname);
	if (poolname == NULL)
		poolname = spa_name(spa);
	sip->pool_name = spa_strdup(poolname);
	sip->spa_load_state = spa_load_state(spa);

	mutex_enter(&shl->procfs_list.pl_lock);
	procfs_list_add(&shl->procfs_list, sip);
	shl->size++;
	mutex_exit(&shl->procfs_list.pl_lock);
}

void
spa_import_progress_remove(uint64_t pool_guid)
{
	spa_history_list_t *shl = spa_import_progress_list;
	spa_import_progress_t *sip;

	mutex_enter(&shl->procfs_list.pl_lock);
	for (sip = list_tail(&shl->procfs_list.pl_list); sip != NULL;
	    sip = list_prev(&shl->procfs_list.pl_list, sip)) {
		if (sip->pool_guid == pool_guid) {
			if (sip->pool_name)
				spa_strfree(sip->pool_name);
			list_remove(&shl->procfs_list.pl_list, sip);
			shl->size--;
			kmem_free(sip, sizeof (spa_import_progress_t));
			break;
		}
	}
	mutex_exit(&shl->procfs_list.pl_lock);
}

#include <linux/mod_compat.h>

static int
param_set_deadman_failmode(const char *val, zfs_kernel_param_t *kp)
{
	int error;

	error = param_set_deadman_failmode_common(val);

	return (error || param_set_charp(val, kp));
}

static int
param_set_deadman_ziotime(const char *val, zfs_kernel_param_t *kp)
{
	spa_t *spa = NULL;
	int error;

	error = param_set_ulong(val, kp);
	if (error < 0)
		return (SET_ERROR(error));

	if (spa_mode_global != 0) {
		mutex_enter(&spa_namespace_lock);
		while ((spa = spa_next(spa)) != NULL)
			spa->spa_deadman_ziotime =
			    MSEC2NSEC(zfs_deadman_ziotime_ms);
		mutex_exit(&spa_namespace_lock);
	}

	return (0);
}

static int
param_set_deadman_synctime(const char *val, zfs_kernel_param_t *kp)
{
	spa_t *spa = NULL;
	int error;

	error = param_set_ulong(val, kp);
	if (error < 0)
		return (SET_ERROR(error));

	if (spa_mode_global != 0) {
		mutex_enter(&spa_namespace_lock);
		while ((spa = spa_next(spa)) != NULL)
			spa->spa_deadman_synctime =
			    MSEC2NSEC(zfs_deadman_synctime_ms);
		mutex_exit(&spa_namespace_lock);
	}

	return (0);
}

static int
param_set_slop_shift(const char *buf, zfs_kernel_param_t *kp)
{
	unsigned long val;
	int error;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return (SET_ERROR(error));

	if (val < 1 || val > 31)
		return (SET_ERROR(-EINVAL));

	error = param_set_int(buf, kp);
	if (error < 0)
		return (SET_ERROR(error));

	return (0);
}

module_param_call(zfs_deadman_synctime_ms, param_set_deadman_synctime,
    param_get_ulong, &zfs_deadman_synctime_ms, 0644);
MODULE_PARM_DESC(zfs_deadman_synctime_ms,
	"Pool sync expiration time in milliseconds");

module_param_call(zfs_deadman_ziotime_ms, param_set_deadman_ziotime,
    param_get_ulong, &zfs_deadman_ziotime_ms, 0644);
MODULE_PARM_DESC(zfs_deadman_ziotime_ms,
	"IO expiration time in milliseconds");

module_param_call(spa_slop_shift, param_set_slop_shift, param_get_int,
	&spa_slop_shift, 0644);
MODULE_PARM_DESC(spa_slop_shift, "Reserved free space in pool");

module_param_call(zfs_deadman_failmode, param_set_deadman_failmode,
	param_get_charp, &zfs_deadman_failmode, 0644);
MODULE_PARM_DESC(zfs_deadman_failmode, "Failmode for deadman timer");
