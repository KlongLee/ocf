/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "ocf/ocf.h"
#include "utils_req.h"
#include "utils_cache_line.h"
#include "../ocf_request.h"
#include "../ocf_cache_priv.h"
#include "../ocf_queue_priv.h"

#define OCF_UTILS_RQ_DEBUG 0

#if 1 == OCF_UTILS_RQ_DEBUG
#define OCF_DEBUG_TRACE(cache) \
	ocf_cache_log(cache, log_info, "[Utils][RQ] %s\n", __func__)

#define OCF_DEBUG_PARAM(cache, format, ...) \
	ocf_cache_log(cache, log_info, "[Utils][RQ] %s - "format"\n", \
			__func__, ##__VA_ARGS__)
#else
#define OCF_DEBUG_TRACE(cache)
#define OCF_DEBUG_PARAM(cache, format, ...)
#endif

enum ocf_req_size {
	ocf_req_size_1 = 0,
	ocf_req_size_2,
	ocf_req_size_4,
	ocf_req_size_8,
	ocf_req_size_16,
	ocf_req_size_32,
	ocf_req_size_64,
	ocf_req_size_128,
	ocf_req_size_max,
};

struct ocf_req_allocator {
	env_allocator *allocator[ocf_req_size_max];
	size_t size[ocf_req_size_max];
};

static inline size_t ocf_req_sizeof_map(struct ocf_request *req)
{
	uint32_t lines = req->alloc_core_line_count;
	size_t size = (lines * sizeof(struct ocf_map_info));

	ENV_BUG_ON(lines == 0);
	return size;
}

static inline size_t ocf_req_sizeof(uint32_t lines)
{
	size_t size = sizeof(struct ocf_request) +
			(lines * sizeof(struct ocf_map_info));

	ENV_BUG_ON(lines == 0);
	return size;
}

#define ALLOCATOR_NAME_FMT "ocf_req_%u"
/* Max number of digits in decimal representation of unsigned int is 10 */
#define ALLOCATOR_NAME_MAX (sizeof(ALLOCATOR_NAME_FMT) + 10)

int ocf_req_allocator_init(struct ocf_ctx *ocf_ctx)
{
	int i;
	struct ocf_req_allocator *req;
	char name[ALLOCATOR_NAME_MAX] = { '\0' };

	OCF_DEBUG_TRACE(cache);

	ocf_ctx->resources.req = env_zalloc(sizeof(*(ocf_ctx->resources.req)),
			ENV_MEM_NORMAL);
	req = ocf_ctx->resources.req;

	if (!req)
		goto ocf_utils_req_init_ERROR;

	for (i = 0; i < ARRAY_SIZE(req->allocator); i++) {
		req->size[i] = ocf_req_sizeof(1 << i);

		if (snprintf(name, sizeof(name), ALLOCATOR_NAME_FMT,
				(1 << i)) < 0) {
			goto ocf_utils_req_init_ERROR;
		}

		req->allocator[i] = env_allocator_create(req->size[i], name);

		if (!req->allocator[i])
			goto ocf_utils_req_init_ERROR;

		OCF_DEBUG_PARAM(cache, "New request allocator, lines = %u, "
				"size = %lu", 1 << i, req->size[i]);
	}

	return 0;

ocf_utils_req_init_ERROR:

	ocf_req_allocator_deinit(ocf_ctx);

	return -1;
}

void ocf_req_allocator_deinit(struct ocf_ctx *ocf_ctx)
{
	int i;
	struct ocf_req_allocator *req;

	OCF_DEBUG_TRACE(cache);


	if (!ocf_ctx->resources.req)
		return;

	req = ocf_ctx->resources.req;

	for (i = 0; i < ARRAY_SIZE(req->allocator); i++) {
		if (req->allocator[i]) {
			env_allocator_destroy(req->allocator[i]);
			req->allocator[i] = NULL;
		}
	}

	env_free(req);
	ocf_ctx->resources.req = NULL;
}

static inline env_allocator *_ocf_req_get_allocator_1(
	struct ocf_cache *cache)
{
	return cache->owner->resources.req->allocator[0];
}

static env_allocator *_ocf_req_get_allocator(
	struct ocf_cache *cache, uint32_t count)
{
	struct ocf_ctx *ocf_ctx = cache->owner;
	unsigned int idx = 31 - __builtin_clz(count);

	if (__builtin_ffs(count) <= idx)
		idx++;

	ENV_BUG_ON(count == 0);

	if (idx >= ocf_req_size_max)
		return NULL;

	return ocf_ctx->resources.req->allocator[idx];
}

static void start_cache_req(struct ocf_request *req)
{
	ocf_cache_t cache = req->cache;
	req->d2c = !ocf_refcnt_inc(&cache->refcnt.metadata);
}

int ocf_req_new(struct ocf_request **out_req, ocf_queue_t queue,
	ocf_core_t core, uint64_t addr, uint32_t bytes, int rw)
{
	uint64_t core_line_first, core_line_last, core_line_count;
	ocf_cache_t cache = queue->cache;
	struct ocf_request *req;
	env_allocator *allocator;

	if (queue != cache->mngt_queue &&
			!ocf_refcnt_inc(&cache->refcnt.io_req)) {
		return -EBUSY;
	}

	if (likely(bytes)) {
		core_line_first = ocf_bytes_2_lines(cache, addr);
		core_line_last = ocf_bytes_2_lines(cache, addr + bytes - 1);
		core_line_count = core_line_last - core_line_first + 1;
	} else {
		core_line_first = ocf_bytes_2_lines(cache, addr);
		core_line_last = core_line_first;
		core_line_count = 1;
	}

	allocator = _ocf_req_get_allocator(cache, core_line_count);
	if (allocator) {
		req = env_allocator_new(allocator);
	} else {
		req = env_allocator_new(_ocf_req_get_allocator_1(cache));
	}

	if (unlikely(!req)) {
		ocf_refcnt_dec(&cache->refcnt.io_req);
		return -ENOMEM;
	}

	if (allocator)
		req->map = req->__map;

	OCF_DEBUG_TRACE(cache);

	ocf_queue_get(queue);
	req->io_queue = queue;

	/* TODO: Store core pointer instead of id */
	req->core_id = core ? ocf_core_get_id(core) : 0;
	req->cache = cache;

	start_cache_req(req);

	env_atomic_set(&req->ref_count, 1);

	req->byte_position = addr;
	req->byte_length = bytes;
	req->core_line_first = core_line_first;
	req->core_line_last = core_line_last;
	req->core_line_count = core_line_count;
	req->alloc_core_line_count = core_line_count;
	req->rw = rw;
	req->part_id = PARTITION_DEFAULT;

	*out_req = req;
	return 0;
}

int ocf_req_alloc_map(struct ocf_request *req)
{
	if (req->map)
		return 0;

	req->map = env_zalloc(ocf_req_sizeof_map(req), ENV_MEM_NOIO);
	if (!req->map) {
		req->error = -ENOMEM;
		return -ENOMEM;
	}

	return 0;
}

int ocf_req_new_extended(struct ocf_request **out_req, ocf_queue_t queue,
		ocf_core_t core, uint64_t addr, uint32_t bytes, int rw)
{
	int ret;

	ret = ocf_req_new(out_req, queue, core, addr, bytes, rw);

	if (likely(!ret) && ocf_req_alloc_map(*out_req)) {
		ocf_req_put(*out_req);
		return -ENOMEM;
	}

	return ret;
}

int ocf_req_new_discard(struct ocf_request **out_req, ocf_queue_t queue,
		ocf_core_t core, uint64_t addr, uint32_t bytes, int rw)
{
	int ret;

	ret = ocf_req_new_extended(out_req, queue, core, addr,
			OCF_MIN(bytes, MAX_TRIM_RQ_SIZE), rw);
	if (ret)
		return ret;

	(*out_req)->discard.sector = BYTES_TO_SECTORS(addr);
	(*out_req)->discard.nr_sects = BYTES_TO_SECTORS(bytes);
	(*out_req)->discard.handled = 0;

	return ret;
}

void ocf_req_get(struct ocf_request *req)
{
	OCF_DEBUG_TRACE(req->cache);

	env_atomic_inc(&req->ref_count);
}

void ocf_req_put(struct ocf_request *req)
{
	env_allocator *allocator;

	if (env_atomic_dec_return(&req->ref_count))
		return;

	ocf_queue_put(req->io_queue);

	OCF_DEBUG_TRACE(req->cache);

	if (!req->d2c)
		ocf_refcnt_dec(&req->cache->refcnt.metadata);

	if (req->io_queue != req->cache->mngt_queue)
		ocf_refcnt_dec(&req->cache->refcnt.io_req);

	allocator = _ocf_req_get_allocator(req->cache,
			req->alloc_core_line_count);
	if (allocator) {
		env_allocator_del(allocator, req);
	} else {
		env_free(req->map);
		env_allocator_del(_ocf_req_get_allocator_1(req->cache), req);
	}
}

void ocf_req_clear_info(struct ocf_request *req)
{
	ENV_BUG_ON(env_memset(&req->info, sizeof(req->info), 0));
}

void ocf_req_clear_map(struct ocf_request *req)
{
	if (likely(req->map))
		ENV_BUG_ON(env_memset(req->map,
			   sizeof(req->map[0]) * req->core_line_count, 0));
}
