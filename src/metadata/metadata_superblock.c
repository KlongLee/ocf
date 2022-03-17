/*
 * Copyright(c) 2020-2021 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "metadata.h"
#include "metadata_core.h"
#include "metadata_internal.h"
#include "metadata_segment_id.h"
#include "metadata_superblock.h"
#include "../ocf_priv.h"
#include "../utils/utils_io.h"
#include "../utils/utils_cache_line.h"

#define OCF_METADATA_SUPERBLOCK_DEBUG 0

#if 1 == OCF_METADATA_SUPERBLOCK_DEBUG
#define OCF_DEBUG_TRACE(cache) \
	ocf_cache_log(cache, log_info, "[Metadata][Superblock] %s\n", \
	__func__)
#define OCF_DEBUG_PARAM(cache, format, ...) \
	ocf_cache_log(cache, log_info, "[Metadata][Superblock] %s - " \
			format"\n", __func__, ##__VA_ARGS__)
#else
#define OCF_DEBUG_TRACE(cache)
#define OCF_DEBUG_PARAM(cache, format, ...)
#endif

int ocf_metadata_segment_init_in_place(
		struct ocf_metadata_segment *segment,
		struct ocf_cache *cache,
		struct ocf_metadata_raw *raw,
		ocf_flush_page_synch_t lock_page_pfn,
		ocf_flush_page_synch_t unlock_page_pfn,
		struct ocf_metadata_segment *superblock);

/**
 * @brief Super Block - Set Shutdown Status
 *
 * @param shutdown_status - status to be assigned to cache.
 *
 * @return Operation status (0 success, otherwise error)
 */
void ocf_metadata_set_shutdown_status(ocf_cache_t cache,
		enum ocf_metadata_shutdown_status shutdown_status,
		ocf_metadata_end_t cmpl, void *priv)
{
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *superblock;

	OCF_DEBUG_TRACE(cache);

	/*
	 * Get metadata hash service control structure
	 */
	/* TODO: get metadata ctrl from args rather than via cache */
	ctrl = (struct ocf_metadata_ctrl *) cache->metadata.priv;

	/*
	 * Get super block
	 */
	superblock = ocf_metadata_raw_get_mem(
			&ctrl->raw_desc[metadata_segment_sb_config]);

	/* Set shutdown status */
	superblock->clean_shutdown = shutdown_status;
	superblock->magic_number = CACHE_MAGIC_NUMBER;

	/* Flush superblock */
	ocf_metadata_flush_superblock(cache, cmpl, priv);
}

static void ocf_metadata_store_segment(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	int segment = ocf_pipeline_arg_get_int(arg);
	struct ocf_metadata_ctrl *ctrl;
	ocf_cache_t cache = context->cache;
	int error;

	ctrl = (struct ocf_metadata_ctrl *)cache->metadata.priv;

	context->segment_copy[segment].mem_pool =
		env_malloc(ctrl->raw_desc[segment].mem_pool_limit, ENV_MEM_NORMAL);
	if (!context->segment_copy[segment].mem_pool)
		OCF_PL_FINISH_RET(pipeline, -OCF_ERR_NO_MEM);

	error = env_memcpy(context->segment_copy[segment].mem_pool,
			ctrl->raw_desc[segment].mem_pool_limit, METADATA_MEM_POOL(ctrl, segment),
			ctrl->raw_desc[segment].mem_pool_limit);
	if (error) {
		env_free(context->segment_copy[segment].mem_pool);
		context->segment_copy[segment].mem_pool = NULL;
		OCF_PL_FINISH_RET(pipeline, error);
	}

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_check_crc_sb_config(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;
	ocf_cache_t cache = context->cache;
	int segment = metadata_segment_sb_config;
	uint32_t crc;

	ctrl = (struct ocf_metadata_ctrl *)cache->metadata.priv;
	sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);

	crc = env_crc32(0, (void *)sb_config,
			offsetof(struct ocf_superblock_config, checksum));

	if (crc != sb_config->checksum[segment]) {
		/* Checksum does not match */
		ocf_cache_log(cache, log_err,
				"Loading %s ERROR, invalid checksum\n",
				ocf_metadata_segment_names[segment]);
		OCF_PL_FINISH_RET(pipeline, -OCF_ERR_INVAL);
	}

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_load_superblock_post(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;
	ocf_cache_t cache = context->cache;

	ctrl = (struct ocf_metadata_ctrl *)cache->metadata.priv;
	sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);

	if (sb_config->core_count > OCF_CORE_MAX) {
		ocf_cache_log(cache, log_err,
			"Loading cache state ERROR, invalid cores count\n");
		OCF_PL_FINISH_RET(pipeline, -OCF_ERR_INVAL);
	}

	if (sb_config->valid_parts_no > OCF_USER_IO_CLASS_MAX) {
		ocf_cache_log(cache, log_err,
			"Loading cache state ERROR, invalid partition count\n");
		OCF_PL_FINISH_RET(pipeline, -OCF_ERR_INVAL);
	}

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_load_sb_restore(
		struct ocf_metadata_context *context)
{
	ocf_cache_t cache = context->cache;
	struct ocf_metadata_ctrl *ctrl;
	int segment, error;

	ctrl = (struct ocf_metadata_ctrl *)cache->metadata.priv;

	for (segment = metadata_segment_sb_config;
			segment < metadata_segment_fixed_size_max; segment++) {
		if (!context->segment_copy[segment].mem_pool)
			continue;

		error = env_memcpy(METADATA_MEM_POOL(ctrl, segment),
				ctrl->raw_desc[segment].mem_pool_limit,
				context->segment_copy[segment].mem_pool,
				ctrl->raw_desc[segment].mem_pool_limit);
		ENV_BUG_ON(error);
	}
}

static void ocf_metadata_load_superblock_finish(ocf_pipeline_t pipeline,
		void *priv, int error)
{
	struct ocf_metadata_context *context = priv;
	ocf_cache_t cache = context->cache;
	int segment;

	if (error) {
		ocf_cache_log(cache, log_err, "Metadata read FAILURE\n");
		ocf_metadata_error(cache);
		ocf_metadata_load_sb_restore(context);
	}

	for (segment = metadata_segment_sb_config;
			segment < metadata_segment_fixed_size_max; segment++) {
		if (context->segment_copy[segment].mem_pool)
			env_free(context->segment_copy[segment].mem_pool);
	}

	context->cmpl(context->priv, error);
	ocf_pipeline_destroy(pipeline);
}

struct ocf_pipeline_arg ocf_metadata_load_sb_store_segment_args[] = {
	OCF_PL_ARG_INT(metadata_segment_sb_config),
	OCF_PL_ARG_INT(metadata_segment_sb_runtime),
	OCF_PL_ARG_INT(metadata_segment_part_config),
	OCF_PL_ARG_INT(metadata_segment_part_runtime),
	OCF_PL_ARG_INT(metadata_segment_core_config),
	OCF_PL_ARG_TERMINATOR(),
};

struct ocf_pipeline_arg ocf_metadata_load_sb_load_segment_args[] = {
	OCF_PL_ARG_INT(metadata_segment_sb_runtime),
	OCF_PL_ARG_INT(metadata_segment_part_config),
	OCF_PL_ARG_INT(metadata_segment_part_runtime),
	OCF_PL_ARG_INT(metadata_segment_core_config),
	OCF_PL_ARG_INT(metadata_segment_core_uuid),
	OCF_PL_ARG_TERMINATOR(),
};

struct ocf_pipeline_properties ocf_metadata_load_sb_pipeline_props = {
	.priv_size = sizeof(struct ocf_metadata_context),
	.finish = ocf_metadata_load_superblock_finish,
	.steps = {
		OCF_PL_STEP_FOREACH(ocf_metadata_store_segment,
				ocf_metadata_load_sb_store_segment_args),
		OCF_PL_STEP_ARG_INT(ocf_metadata_load_segment,
				metadata_segment_sb_config),
		OCF_PL_STEP(ocf_metadata_check_crc_sb_config),
		OCF_PL_STEP_FOREACH(ocf_metadata_load_segment,
				ocf_metadata_load_sb_load_segment_args),
		OCF_PL_STEP_FOREACH(ocf_metadata_check_crc,
				ocf_metadata_load_sb_load_segment_args),
		OCF_PL_STEP(ocf_metadata_load_superblock_post),
		OCF_PL_STEP_TERMINATOR(),
	},
};

/*
 * Super Block - Load, This function has to prevent to pointers overwrite
 */
void ocf_metadata_load_superblock(ocf_cache_t cache, ocf_metadata_end_t cmpl,
		void *priv)
{
	struct ocf_metadata_context *context;
	ocf_pipeline_t pipeline;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;
	struct ocf_superblock_runtime *sb_runtime;
	int result;

	OCF_DEBUG_TRACE(cache);

	/* TODO: get ctrl from args rather than from cache */
	ctrl = cache->metadata.priv;
	ENV_BUG_ON(!ctrl);

	sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);
	ENV_BUG_ON(!sb_config);

	sb_runtime = METADATA_MEM_POOL(ctrl, metadata_segment_sb_runtime);
	ENV_BUG_ON(!sb_runtime);

	result = ocf_pipeline_create(&pipeline, cache,
			&ocf_metadata_load_sb_pipeline_props);
	if (result)
		OCF_CMPL_RET(priv, result);

	context = ocf_pipeline_get_priv(pipeline);

	context->cmpl = cmpl;
	context->priv = priv;
	context->pipeline = pipeline;
	context->cache = cache;
	context->ctrl = cache->metadata.priv;

	ocf_pipeline_next(pipeline);
}

struct ocf_pipeline_arg ocf_metadata_load_sb_recov_load_segment_args[] = {
	OCF_PL_ARG_INT(metadata_segment_part_config),
	OCF_PL_ARG_INT(metadata_segment_core_config),
	OCF_PL_ARG_INT(metadata_segment_core_uuid),
	OCF_PL_ARG_TERMINATOR(),
};

struct ocf_pipeline_properties ocf_metadata_load_sb_recov_pipeline_props = {
	.priv_size = sizeof(struct ocf_metadata_context),
	.finish = ocf_metadata_load_superblock_finish,
	.steps = {
		OCF_PL_STEP_FOREACH(ocf_metadata_store_segment,
				ocf_metadata_load_sb_store_segment_args),
		OCF_PL_STEP_ARG_INT(ocf_metadata_load_segment,                                      
				metadata_segment_sb_config),  
		OCF_PL_STEP(ocf_metadata_check_crc_sb_config),
		OCF_PL_STEP_FOREACH(ocf_metadata_load_segment,
				ocf_metadata_load_sb_recov_load_segment_args),
		OCF_PL_STEP_FOREACH(ocf_metadata_check_crc,
				ocf_metadata_load_sb_recov_load_segment_args),
		OCF_PL_STEP(ocf_metadata_load_superblock_post),
		OCF_PL_STEP_TERMINATOR(),
	},
};

/*
 * Super Block - Recovery load
 */
void ocf_metadata_load_superblock_recovery(ocf_cache_t cache,
		ocf_metadata_end_t cmpl, void *priv)
{
	struct ocf_metadata_context *context;
	ocf_pipeline_t pipeline;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;
	struct ocf_superblock_runtime *sb_runtime;
	int result;

	OCF_DEBUG_TRACE(cache);

	/* TODO: get ctrl from args rather than from cache */
	ctrl = cache->metadata.priv;
	ENV_BUG_ON(!ctrl);

	sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);
	ENV_BUG_ON(!sb_config);

	sb_runtime = METADATA_MEM_POOL(ctrl, metadata_segment_sb_runtime);
	ENV_BUG_ON(!sb_runtime);

	result = ocf_pipeline_create(&pipeline, cache,
			&ocf_metadata_load_sb_recov_pipeline_props);
	if (result)
		OCF_CMPL_RET(priv, result);

	context = ocf_pipeline_get_priv(pipeline);

	context->cmpl = cmpl;
	context->priv = priv;
	context->pipeline = pipeline;
	context->cache = cache;
	context->ctrl = cache->metadata.priv;

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_flush_superblock_prepare(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	ocf_cache_t cache = context->cache;
	ocf_core_t core;
	ocf_core_id_t core_id;

	/* Synchronize core objects types */
	for_each_core_metadata(cache, core, core_id) {
		core->conf_meta->type = ocf_ctx_get_volume_type_id(
				cache->owner, core->volume.type);
	}

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_flush_superblock_flap(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;

	ctrl = context->ctrl;
	sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);

	sb_config->flapping_idx = (sb_config->flapping_idx + 1) % 2;

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_calculate_crc_sb_config(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;

	ctrl = context->ctrl;
	sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);

	sb_config->checksum[metadata_segment_sb_config] = env_crc32(0,
			(void *)sb_config,
			offsetof(struct ocf_superblock_config, checksum));

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_flush_superblock_finish(ocf_pipeline_t pipeline,
		void *priv, int error)
{
	struct ocf_metadata_context *context = priv;
	struct ocf_metadata_ctrl *ctrl;
	struct ocf_superblock_config *sb_config;
	ocf_cache_t cache = context->cache;

	if (error) {
		ocf_metadata_error(cache);

		ctrl = context->ctrl;
		sb_config = METADATA_MEM_POOL(ctrl, metadata_segment_sb_config);

		sb_config->flapping_idx = (sb_config->flapping_idx - 1) % 2;
	}

	context->cmpl(context->priv, error);
	ocf_pipeline_destroy(pipeline);
}

static void ocf_metadata_flush_disk_end(void *priv, int error)
{
	struct ocf_metadata_context *context = priv;
	ocf_pipeline_t pipeline = context->pipeline;

	if (error) {
		OCF_PL_FINISH_RET(pipeline, error);
		return;
	}

	ocf_pipeline_next(pipeline);
}

static void ocf_metadata_flush_disk(ocf_pipeline_t pipeline,
		void *priv, ocf_pipeline_arg_t arg)
{
	struct ocf_metadata_context *context = priv;
	ocf_cache_t cache = context->cache;

	ocf_submit_volume_flush(ocf_cache_get_volume(cache),
		ocf_metadata_flush_disk_end, context);
}

struct ocf_pipeline_arg ocf_metadata_flush_sb_args[] = {
	OCF_PL_ARG_INT(metadata_segment_part_config),
	OCF_PL_ARG_INT(metadata_segment_core_config),
	OCF_PL_ARG_INT(metadata_segment_core_uuid),
	OCF_PL_ARG_TERMINATOR(),
};

struct ocf_pipeline_properties ocf_metadata_flush_sb_pipeline_props = {
	.priv_size = sizeof(struct ocf_metadata_context),
	.finish = ocf_metadata_flush_superblock_finish,
	.steps = {
		OCF_PL_STEP(ocf_metadata_flush_superblock_prepare),
		OCF_PL_STEP_FOREACH(ocf_metadata_calculate_crc,
				ocf_metadata_flush_sb_args),
		OCF_PL_STEP_FOREACH(ocf_metadata_flush_segment,
				ocf_metadata_flush_sb_args),
		OCF_PL_STEP(ocf_metadata_flush_disk),
		OCF_PL_STEP(ocf_metadata_flush_superblock_flap),
		OCF_PL_STEP(ocf_metadata_calculate_crc_sb_config),
		OCF_PL_STEP_ARG_INT(ocf_metadata_flush_segment,
				metadata_segment_sb_config),
		OCF_PL_STEP(ocf_metadata_flush_disk),
		OCF_PL_STEP_TERMINATOR(),
	},
};

/*
 * Super Block - FLUSH
 */
void ocf_metadata_flush_superblock(ocf_cache_t cache,
		ocf_metadata_end_t cmpl, void *priv)
{
	struct ocf_metadata_context *context;
	ocf_pipeline_t pipeline;
	int result;

	OCF_DEBUG_TRACE(cache);

	result = ocf_pipeline_create(&pipeline, cache,
			&ocf_metadata_flush_sb_pipeline_props);
	if (result)
		OCF_CMPL_RET(priv, result);

	context = ocf_pipeline_get_priv(pipeline);

	context->cmpl = cmpl;
	context->priv = priv;
	context->pipeline = pipeline;
	context->cache = cache;
	context->ctrl = cache->metadata.priv;

	ocf_pipeline_next(pipeline);
}

struct ocf_metadata_superblock
{
	struct ocf_metadata_segment segment;
	struct ocf_superblock_config *config;
	ocf_cache_t cache;
};

#define _ocf_segment_to_sb(_segment) \
	container_of(_segment, struct ocf_metadata_superblock, segment);

int ocf_metadata_superblock_init(
		struct ocf_metadata_segment **self,
		struct ocf_cache *cache,
		struct ocf_metadata_raw *raw)
{
	struct ocf_metadata_superblock *sb = env_vzalloc(sizeof(*sb));
	int result;

	if (!sb)
		return -OCF_ERR_NO_MEM;

	result = ocf_metadata_segment_init_in_place(&sb->segment, cache,
			raw, NULL, NULL, &sb->segment);

	if (result) {
		env_vfree(sb);
		return result;
	}

	sb->config = ocf_metadata_raw_get_mem(sb->segment.raw);
	sb->cache = cache;

	*self = &sb->segment;
	return 0;
}


void ocf_metadata_superblock_destroy(
		struct ocf_cache *cache,
		struct ocf_metadata_segment *self)
{
	ocf_metadata_segment_destroy(cache, self);
}

uint32_t ocf_metadata_superblock_get_checksum(
		struct ocf_metadata_segment *self,
		enum ocf_metadata_segment_id segment)
{
	struct ocf_metadata_superblock *sb = _ocf_segment_to_sb(self);

	return sb->config->checksum[segment];
}

void ocf_metadata_superblock_set_checksum(
		struct ocf_metadata_segment *self,
		enum ocf_metadata_segment_id segment,
		uint32_t csum)
{
	struct ocf_metadata_superblock *sb = _ocf_segment_to_sb(self);

	sb->config->checksum[segment] = csum;
}

bool ocf_metadata_superblock_get_clean_shutdown(
		struct ocf_metadata_segment *self)
{
	struct ocf_metadata_superblock *sb = _ocf_segment_to_sb(self);

	return sb->config->clean_shutdown;
}

unsigned ocf_metadata_superblock_get_flapping_idx(
		struct ocf_metadata_segment *self)
{
	struct ocf_metadata_superblock *sb = _ocf_segment_to_sb(self);

	return sb->config->flapping_idx;
}

unsigned ocf_metadata_superblock_get_next_flapping_idx(
		struct ocf_metadata_segment *self)
{
	struct ocf_metadata_superblock *sb = _ocf_segment_to_sb(self);

	return (sb->config->flapping_idx + 1) % 2;
}

int ocf_metadata_validate_superblock(ocf_ctx_t ctx,
		struct ocf_superblock_config *superblock)
{
	if (superblock->magic_number != CACHE_MAGIC_NUMBER) {
		ocf_log(ctx, log_info, "Cannot detect pre-existing metadata\n");
		return -OCF_ERR_NO_METADATA;
	}

	if (METADATA_VERSION() != superblock->metadata_version) {
		ocf_log(ctx, log_err, "Metadata version mismatch!\n");
		return -OCF_ERR_METADATA_VER;
	}

	if (!ocf_cache_line_size_is_valid(superblock->line_size)) {
		ocf_log(ctx, log_err, "ERROR: Invalid cache line size!\n");
		return -OCF_ERR_INVAL;
	}

	if ((unsigned)superblock->metadata_layout >= ocf_metadata_layout_max) {
		ocf_log(ctx, log_err, "ERROR: Invalid metadata layout!\n");
		return -OCF_ERR_INVAL;
	}

	if (superblock->cache_mode >= ocf_cache_mode_max) {
		ocf_log(ctx, log_err, "ERROR: Invalid cache mode!\n");
		return -OCF_ERR_INVAL;
	}

	if (superblock->clean_shutdown > ocf_metadata_clean_shutdown) {
		ocf_log(ctx, log_err, "ERROR: Invalid shutdown status!\n");
		return -OCF_ERR_INVAL;
	}

	if (superblock->dirty_flushed > DIRTY_FLUSHED) {
		ocf_log(ctx, log_err, "ERROR: Invalid flush status!\n");
		return -OCF_ERR_INVAL;
	}

	return 0;
}

static void ocf_metadata_read_sb_complete(struct ocf_io *io, int error)
{
	struct ocf_metadata_read_sb_ctx *context = io->priv1;
	ctx_data_t *data = ocf_io_get_data(io);

	if (!error) {
		/* Read data from data into super block buffer */
		ctx_data_rd_check(context->ctx, &context->superblock, data,
				sizeof(context->superblock));
	}

	ctx_data_free(context->ctx, data);
	ocf_io_put(io);

	context->error = error;
	context->cmpl(context);

	env_free(context);
}

int ocf_metadata_read_sb(ocf_ctx_t ctx, ocf_volume_t volume,
		ocf_metadata_read_sb_end_t cmpl, void *priv1, void *priv2)
{
	struct ocf_metadata_read_sb_ctx *context;
	size_t sb_pages = BYTES_TO_PAGES(sizeof(context->superblock));
	ctx_data_t *data;
	struct ocf_io *io;
	int result = 0;

	/* Allocate memory for first page of super block */
	context = env_zalloc(sizeof(*context), ENV_MEM_NORMAL);
	if (!context) {
		ocf_log(ctx, log_err, "Memory allocation error");
		return -OCF_ERR_NO_MEM;
	}

	context->cmpl = cmpl;
	context->ctx = ctx;
	context->priv1 = priv1;
	context->priv2 = priv2;

	/* Allocate resources for IO */
	io = ocf_volume_new_io(volume, NULL, 0, sb_pages * PAGE_SIZE,
			OCF_READ, 0, 0);
	if (!io) {
		ocf_log(ctx, log_err, "Memory allocation error");
		result = -OCF_ERR_NO_MEM;
		goto err_io;
	}

	data = ctx_data_alloc(ctx, sb_pages);
	if (!data) {
		ocf_log(ctx, log_err, "Memory allocation error");
		result = -OCF_ERR_NO_MEM;
		goto err_data;
	}

	/*
	 * Read first page of cache device in order to recover metadata
	 * properties
	 */
	result = ocf_io_set_data(io, data, 0);
	if (result) {
		ocf_log(ctx, log_err, "Metadata IO configuration error\n");
		result = -OCF_ERR_IO;
		goto err_set_data;
	}

	ocf_io_set_cmpl(io, context, NULL, ocf_metadata_read_sb_complete);
	ocf_volume_submit_io(io);

	return 0;

err_set_data:
	ctx_data_free(ctx, data);
err_data:
	ocf_io_put(io);
err_io:
	env_free(context);
	return result;
}

void ocf_metadata_sb_zero(struct ocf_metadata_segment *self,
		ocf_metadata_end_t cmpl, void *priv)
{
	struct ocf_metadata_superblock *sb = _ocf_segment_to_sb(self);

	ocf_metadata_raw_zero(sb->cache, sb->segment.raw, cmpl, priv);
}
