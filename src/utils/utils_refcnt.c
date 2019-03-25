/*
 * Copyright(c) 2019 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "../utils/utils_refcnt.h"

void ocf_refcnt_dec(struct ocf_refcnt *rc)
{
	int val = env_atomic_dec_return(&rc->counter);
	ENV_BUG_ON(val < 0);

	if (!val && env_atomic_cmpxchg(&rc->callback, 1, 0))
		rc->cb(rc->priv);
}

bool ocf_refcnt_inc(struct ocf_refcnt  *rc)
{
	bool incremented = false;

	if (!env_atomic_read(&rc->freeze)) {
		env_atomic_inc(&rc->counter);
		if (!env_atomic_read(&rc->freeze))
			incremented = true;
		else
			ocf_refcnt_dec(rc);
	}

	return incremented;
}

void ocf_refcnt_freeze(struct ocf_refcnt *rc)
{
	env_atomic_inc(&rc->freeze);
}

void ocf_refcnt_register_zero_cb(struct ocf_refcnt *rc, ocf_refcnt_cb_t cb,
		void *priv)
{
	ENV_BUG_ON(!env_atomic_read(&rc->freeze));

	env_atomic_inc(&rc->counter);
	rc->cb = cb;
	rc->priv = priv;
	env_atomic_set(&rc->callback, 1);
	ocf_refcnt_dec(rc);
}

void ocf_refcnt_unfreeze(struct ocf_refcnt *rc)
{
	env_atomic_dec(&rc->freeze);
}

static void _ocf_refcnt_sync_wait_cb(void *priv)
{
	env_waitqueue *wq = priv;
	env_waitqueue_wake_up(wq);
}

void ocf_refcnt_wait_for_zero(struct ocf_refcnt *rc)
{
	env_waitqueue wq;
	env_waitqueue_init(&wq);
	ocf_refcnt_register_zero_cb(rc, _ocf_refcnt_sync_wait_cb, &wq);
	env_waitqueue_wait(wq, !env_atomic_read(&rc->callback));
	return;
}
