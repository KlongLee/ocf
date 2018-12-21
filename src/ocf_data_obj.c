/*
 * Copyright(c) 2012-2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "ocf/ocf.h"
#include "ocf_priv.h"
#include "ocf_data_obj_priv.h"
#include "ocf_io_priv.h"
#include "ocf_env.h"

/* *** Bottom interface *** */

/*
 * This is io allocator dedicated for bottom devices.
 * Out IO structure looks like this:
 * --------------> +-------------------------+
 * | OCF is aware  |                         |
 * | of this part. | struct ocf_io_meta      |
 * |               |                         |
 * |               +-------------------------+ <----------------
 * |               |                         |  Bottom adapter |
 * |               | struct ocf_io           |  is aware of    |
 * |               |                         |  this part.     |
 * --------------> +-------------------------+                 |
 *                 |                         |                 |
 *                 | Bottom adapter specific |                 |
 *                 | data structure.         |                 |
 *                 |                         |                 |
 *                 +-------------------------+ <----------------
 */

#define OCF_IO_ALLOCATOR_TOTAL_SIZE(size) \
		(sizeof(struct ocf_io_meta) + sizeof(struct ocf_io) + size)

static env_allocator *ocf_io_allocator_create(uint32_t size, const char *name)
{
	return env_allocator_create(OCF_IO_ALLOCATOR_TOTAL_SIZE(size), name);
}

static void ocf_io_allocator_destroy(env_allocator *allocator)
{
	env_allocator_destroy(allocator);
}

static struct ocf_io *ocf_io_allocator_new(env_allocator *allocator)
{
	void *data = env_allocator_new(allocator);

	return data ? (data + sizeof(struct ocf_io_meta)) : NULL;
}

static void ocf_io_allocator_del(env_allocator *allocator, struct ocf_io *io)
{
	if (!io)
		return;

	env_allocator_del(allocator, (void *)io - sizeof(struct ocf_io_meta));
}

/*
 * Data object type
 */

int ocf_data_obj_type_init(struct ocf_data_obj_type **type,
		const struct ocf_data_obj_properties *properties)
{
	const struct ocf_data_obj_ops *ops = &properties->ops;
	struct ocf_data_obj_type *new_type;
	int ret;

	if (!ops->new_io || !ops->submit_io || !ops->open || !ops->close ||
			!ops->get_max_io_size || !ops->get_length) {
		return -EINVAL;
	}

	if (properties->caps.atomic_writes && !ops->submit_metadata)
		return -EINVAL;

	new_type = env_zalloc(sizeof(**type), ENV_MEM_NORMAL);
	if (!new_type)
		return -OCF_ERR_NO_MEM;

	new_type->allocator = ocf_io_allocator_create(
			properties->io_context_size, properties->name);
	if (!new_type->allocator) {
		ret = -ENOMEM;
		goto err;
	}

	new_type->properties = properties;

	*type = new_type;

	return 0;

err:
	env_free(new_type);
	return ret;
}

void ocf_data_obj_type_deinit(struct ocf_data_obj_type *type)
{
	ocf_io_allocator_destroy(type->allocator);
	env_free(type);
}

/*
 * Data object backend API
 */

void *ocf_data_obj_get_priv(ocf_data_obj_t obj)
{
	OCF_CHECK_NULL(obj);

	return obj->priv;
}

void ocf_data_obj_set_priv(ocf_data_obj_t obj, void *priv)
{
	OCF_CHECK_NULL(obj);

	obj->priv = priv;
}

struct ocf_io *ocf_data_obj_new_io(ocf_data_obj_t obj)
{
	struct ocf_io *io;

	OCF_CHECK_NULL(obj);

	io = ocf_io_allocator_new(obj->type->allocator);
	if (!io)
		return NULL;

	io->obj = obj;
	io->ops = &obj->type->properties->io_ops;

	return io;
}

void ocf_data_obj_del_io(struct ocf_io* io)
{
	OCF_CHECK_NULL(io);

	ocf_io_allocator_del(io->obj->type->allocator, io);
}

void *ocf_data_obj_get_data_from_io(struct ocf_io* io)
{
	return (void *)io + sizeof(struct ocf_io);
}

/*
 * Data object frontend API
 */

int ocf_dobj_init(ocf_data_obj_t obj, ocf_data_obj_type_t type,
		struct ocf_data_obj_uuid *uuid, bool uuid_copy)
{
	if (!obj || !type)
		return -OCF_ERR_INVAL;

	obj->type = type;

	if (!uuid) {
		obj->uuid_copy = false;
		return 0;
	}

	obj->uuid_copy = uuid_copy;

	if (uuid_copy) {
		obj->uuid.data = env_strdup(uuid->data, ENV_MEM_NORMAL);
		if (!obj->uuid.data)
			return -OCF_ERR_NO_MEM;
	} else {
		obj->uuid.data = uuid->data;
	}

	obj->uuid.size = uuid->size;

	return 0;
}

void ocf_dobj_deinit(ocf_data_obj_t obj)
{
	OCF_CHECK_NULL(obj);

	if (obj->uuid_copy && obj->uuid.data)
		env_free(obj->uuid.data);
}

int ocf_dobj_create(ocf_data_obj_t *obj, ocf_data_obj_type_t type,
		struct ocf_data_obj_uuid *uuid)
{
	ocf_data_obj_t tmp_obj;
	int ret;

	OCF_CHECK_NULL(obj);

	tmp_obj = env_zalloc(sizeof(*tmp_obj), ENV_MEM_NORMAL);
	if (!tmp_obj)
		return -OCF_ERR_NO_MEM;

	ret = ocf_dobj_init(tmp_obj, type, uuid, true);
	if (ret) {
		env_free(tmp_obj);
		return ret;
	}

	*obj = tmp_obj;

	return 0;
}

void ocf_data_obj_destroy(ocf_data_obj_t obj)
{
	OCF_CHECK_NULL(obj);

	ocf_dobj_deinit(obj);
	env_free(obj);
}

ocf_data_obj_type_t ocf_dobj_get_type(ocf_data_obj_t obj)
{
	OCF_CHECK_NULL(obj);

	return obj->type;
}

const struct ocf_data_obj_uuid *ocf_dobj_get_uuid(
		ocf_data_obj_t obj)
{
	OCF_CHECK_NULL(obj);

	return &obj->uuid;
}

ocf_cache_t ocf_dobj_get_cache(ocf_data_obj_t obj)
{
	OCF_CHECK_NULL(obj);

	return obj->cache;
}

int ocf_dobj_is_atomic(ocf_data_obj_t obj)
{
        return obj->type->properties->caps.atomic_writes;
}

struct ocf_io *ocf_dobj_new_io(ocf_data_obj_t obj)
{
	ENV_BUG_ON(!obj->type->properties->ops.new_io);

	return obj->type->properties->ops.new_io(obj);
}

void ocf_dobj_submit_io(struct ocf_io *io)
{
	ENV_BUG_ON(!io->obj->type->properties->ops.submit_io);

	io->obj->type->properties->ops.submit_io(io);
}

void ocf_dobj_submit_flush(struct ocf_io *io)
{
	ENV_BUG_ON(!io->obj->type->properties->ops.submit_flush);

	if (!io->obj->type->properties->ops.submit_flush) {
		ocf_io_end(io, 0); 
		return;
	}

	io->obj->type->properties->ops.submit_flush(io);
}

void ocf_dobj_submit_discard(struct ocf_io *io)
{
	if (!io->obj->type->properties->ops.submit_discard) {
		ocf_io_end(io, 0); 
		return;
	}

	io->obj->type->properties->ops.submit_discard(io);
}

int ocf_dobj_open(ocf_data_obj_t obj)
{
        ENV_BUG_ON(!obj->type->properties->ops.open);

        return obj->type->properties->ops.open(obj);
}

void ocf_dobj_close(ocf_data_obj_t obj)
{
        ENV_BUG_ON(!obj->type->properties->ops.close);

        obj->type->properties->ops.close(obj);
}

unsigned int ocf_dobj_get_max_io_size(ocf_data_obj_t obj)
{
        ENV_BUG_ON(!obj->type->properties->ops.get_max_io_size);

        return obj->type->properties->ops.get_max_io_size(obj);
}

uint64_t ocf_dobj_get_length(ocf_data_obj_t obj)
{
        ENV_BUG_ON(!obj->type->properties->ops.get_length);

	return obj->type->properties->ops.get_length(obj);
}
