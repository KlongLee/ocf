/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef OCF_DEBUG_H
#define OCF_DEBUG_H

#ifndef OCF_DEBUG_TAG
#define OCF_DEBUG_TAG "debug"
#endif

#ifndef OCF_DEBUG
#define OCF_DEBUG 0
#endif

#if OCF_DEBUG == 1

/* helpers */
#define OCF_DBG_LOG(log_prefix_func, ctx, prefix, format, ...) \
               log_prefix_func(ctx, log_debug, prefix OCF_DEBUG_TAG ".%s()", \
                               format"\n", __func__, ##__VA_ARGS__)

#define OCF_DBG_TRACE(log_prefix_func, ctx, prefix) \
               OCF_DBG_LOG(log_prefix_func, ctx, prefix, ":%d", __LINE__)

#define OCF_DBG_MSG(log_prefix_func, ctx, prefix, msg) \
               OCF_DBG_LOG(log_prefix_func, ctx, prefix, ": %s", msg)

#define OCF_DBG_PARAM(log_prefix_func, ctx, prefix, format, ...) \
               OCF_DBG_LOG(log_prefix_func, ctx, prefix, ": "format, \
				##__VA_ARGS__)

/* ocf_ctx */
#define OCF_DEBUG_TRACE(ctx) \
               OCF_DBG_TRACE(ocf_log_prefix, ctx, "")

#define OCF_DEBUG_MSG(ctx, msg) \
               OCF_DBG_MSG(ocf_log_prefix, ctx, "", msg)

#define OCF_DEBUG_PARAM(ctx, format, ...) \
               OCF_DBG_PARAM(ocf_log_prefix, ctx, "", format, ##__VA_ARGS__)

/* ocf_cache */
#define OCF_DEBUG_CACHE_TRACE(cache) \
               OCF_DBG_TRACE(ocf_cache_log_prefix, cache, ".")

#define OCF_DEBUG_CACHE_MSG(cache, msg) \
               OCF_DBG_MSG(ocf_cache_log_prefix, cache, ".", msg)

#define OCF_DEBUG_CACHE_PARAM(cache, format, ...) \
               OCF_DBG_PARAM(ocf_cache_log_prefix, cache, ".", format, \
                               ##__VA_ARGS__)

/* ocf_core */
#define OCF_DEBUG_CORE_TRACE(core) \
               OCF_DBG_TRACE(ocf_core_log_prefix, core, ".")

#define OCF_DEBUG_CORE_MSG(core, msg) \
               OCF_DBG_MSG(ocf_core_log_prefix, core, ".", msg)

#define OCF_DEBUG_CORE_PARAM(core, format, ...) \
               OCF_DBG_PARAM(ocf_core_log_prefix, core, ".", format, \
                               ##__VA_ARGS__)

/* other */
#define OCF_DEBUG_REQ(req, format, ...) \
               ocf_cache_log_prefix(req->cache, log_debug, \
                       "."OCF_DEBUG_TAG".%s(%s, %llu, %u): ", format, \
                       __func__, OCF_READ == (req)->rw ? "rd" : "wr", \
                       req->byte_position, req->byte_length, ##__VA_ARGS__)

#else
#define OCF_DEBUG_TRACE(ctx)
#define OCF_DEBUG_MSG(ctx, msg)
#define OCF_DEBUG_PARAM(ctx, format, ...)
#define OCF_DEBUG_CACHE_TRACE(cache)
#define OCF_DEBUG_CACHE_MSG(cache, msg)
#define OCF_DEBUG_CACHE_PARAM(cache, format, ...)
#define OCF_DEBUG_CORE_TRACE(core)
#define OCF_DEBUG_CORE_MSG(core, msg)
#define OCF_DEBUG_CORE_PARAM(core, format, ...)
#define OCF_DEBUG_REQ(rq, format, ...)
#endif

#endif /* OCF_DEBUG_H */

