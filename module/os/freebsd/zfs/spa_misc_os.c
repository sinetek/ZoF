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
s * ==========================================================================
 * SPA Import Progress Routines
 * ==========================================================================
 */

typedef struct spa_import_progress {
	uint64_t		pool_guid;	/* unique id for updates */
	char			*pool_name;
	spa_load_state_t	spa_load_state;
	uint64_t		mmp_sec_remaining;	/* MMP activity check */
	uint64_t		spa_load_max_txg;	/* rewind txg */
} spa_import_progress_t;

spa_history_list_t *spa_import_progress_list = NULL;

void
spa_import_progress_init(void)
{
}

void
spa_import_progress_destroy(void)
{
}

void
spa_import_progress_remove(uint64_t pool_guid)
{
}

int
spa_import_progress_set_state(uint64_t pool_guid,
    spa_load_state_t load_state)
{
	return (0);
}

void
spa_import_progress_add(spa_t *spa)
{

}

int
spa_import_progress_set_max_txg(uint64_t pool_guid, uint64_t load_max_txg)
{
	return (0);
}

int
spa_import_progress_set_mmp_check(uint64_t pool_guid,
    uint64_t mmp_sec_remaining)
{
	return (0);
}
