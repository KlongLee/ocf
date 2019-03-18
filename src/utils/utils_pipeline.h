/*
 * Copyright(c) 2019 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __UTILS_PIPELINE_H__
#define __UTILS_PIPELINE_H__

#include "../ocf_cache_priv.h"

typedef struct ocf_pipeline *ocf_pipeline_t;

typedef void (*ocf_pipeline_step_cb_t)(ocf_pipeline_t pipeline,
		void *priv, void *arg);

typedef void (*ocf_pipeline_finish_t)(ocf_pipeline_t pipeline,
		void *priv, int error);

struct ocf_pipeline_step {
	ocf_pipeline_step_cb_t cb;
	void *arg;
};

struct ocf_pipeline_properties {
	ocf_pipeline_finish_t finish;
	struct ocf_pipeline_step steps[];
};

int ocf_pipeline_create(ocf_pipeline_t *pipeline, ocf_cache_t cache,
		struct ocf_pipeline_properties *properties, void *priv);

void *ocf_pipeline_get_priv(ocf_pipeline_t pipeline);

void ocf_pipeline_destroy(ocf_pipeline_t pipeline);

void ocf_pipeline_next(ocf_pipeline_t pipeline);

void ocf_pipeline_finish(ocf_pipeline_t pipeline, int error);

#endif /* __UTILS_PIPELINE_H__ */
