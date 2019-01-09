/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "ocf/ocf.h"
#include "ocf_priv.h"
#include "metadata/metadata.h"
#include "engine/cache_engine.h"
#include "utils/utils_part.h"

int ocf_io_class_get_info(ocf_cache_t cache, uint32_t io_class,
		struct ocf_io_class_info *info)
{
	ocf_part_id_t part_id = io_class;

	OCF_CHECK_NULL(cache);

	if (!info)
		return -OCF_ERR_INVAL;

	if (io_class >= OCF_IO_CLASS_MAX)
		return -OCF_ERR_INVAL;

	if (!ocf_part_is_valid(&cache->user_parts[part_id])) {
		/* Partition does not exist */
		return -OCF_ERR_IO_CLASS_NOT_EXIST;
	}

	if (env_strncpy(info->name, sizeof(info->name),
			cache->user_parts[part_id].config->name,
			sizeof(cache->user_parts[part_id].config->name))) {
		return -OCF_ERR_INVAL;
	}

	info->priority = cache->user_parts[part_id].config->priority;
	info->curr_size = ocf_cache_is_device_attached(cache) ?
			cache->user_parts[part_id].runtime->curr_size : 0;
	info->min_size = cache->user_parts[part_id].config->min_size;
	info->max_size = cache->user_parts[part_id].config->max_size;

	info->eviction_policy_type = cache->conf_meta->eviction_policy_type;
	info->cleaning_policy_type = cache->conf_meta->cleaning_policy_type;

	info->cache_mode = cache->user_parts[part_id].config->cache_mode;

	return 0;
}

int ocf_io_classes_get_info(ocf_cache_t cache, struct ocf_io_class_info *info)
{
	struct ocf_user_part *part;
	ocf_part_id_t part_id;
	int result;

	OCF_CHECK_NULL(cache);

	OCF_CHECK_NULL(info);

	result = ocf_mngt_cache_read_lock(cache);
	if (result)
		return result;

	for_each_part(cache, part, part_id) {
		if (!ocf_part_is_valid(part)) {
			env_memset(info[part_id].name, sizeof(info[part_id].name), 0);
			info[part_id].cache_mode = ocf_cache_mode_none;
			continue;
		}

		if (env_strncpy(info[part_id].name, sizeof(info[part_id].name),
				part->config->name, sizeof(part->config->name))) {
			result = -OCF_ERR_INVAL;
			goto unlock;
		}

		info[part_id].priority = part->config->priority;
		info[part_id].curr_size = ocf_cache_is_device_attached(cache) ?
				part->runtime->curr_size : 0;
		info[part_id].min_size = part->config->min_size;
		info[part_id].max_size = part->config->max_size;

		info[part_id].eviction_policy_type = cache->conf_meta->eviction_policy_type;
		info[part_id].cleaning_policy_type = cache->conf_meta->cleaning_policy_type;

		info[part_id].cache_mode = part->config->cache_mode;
	}

unlock:
	ocf_mngt_cache_read_unlock(cache);

	return result;
}

int ocf_io_class_visit(ocf_cache_t cache, ocf_io_class_visitor_t visitor,
		void *cntx)
{
	struct ocf_user_part *part;
	ocf_part_id_t part_id;
	int result = 0;

	OCF_CHECK_NULL(cache);

	if (!visitor)
		return -OCF_ERR_INVAL;

	for_each_part(cache, part, part_id) {
		if (!ocf_part_is_valid(part))
			continue;

		result = visitor(cache, part_id, cntx);
		if (result)
			break;
	}

	return result;
}
