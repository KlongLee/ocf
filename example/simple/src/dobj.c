#include <ocf/ocf.h>
#include "dobj.h"
#include "data.h"
#include "ctx.h"

static int dobj_open(ocf_data_obj_t obj)
{
	const struct ocf_data_obj_uuid *uuid = ocf_dobj_get_uuid(obj);
	struct dobj *dobj = ocf_dobj_get_priv(obj);

	dobj->name = uuid->data;
	dobj->mem = malloc(200*1024*1024);

	return 0;
}

static void dobj_close(ocf_data_obj_t obj)
{
	struct dobj *dobj = ocf_dobj_get_priv(obj);

	free(dobj);
}

static int intelcas_dobj_io_set_data(struct ocf_io *io, ctx_data_t *data,
		uint32_t offset)
{
	struct dobj_io *dobj_io = ocf_io_get_priv(io);

	dobj_io->offset = offset;
	dobj_io->data = data;

	return 0;
}

static ctx_data_t *intelcas_dobj_io_get_data(struct ocf_io *io)
{
	struct dobj_io *dobj_io = ocf_io_get_priv(io);

	return dobj_io->data;
}

static void dobj_submit_io(struct ocf_io *io)
{
	struct dobj_data *data;
	struct dobj *dobj;

	data = ocf_io_get_data(io);
	dobj = ocf_dobj_get_priv(io->obj);

	if (io->dir == OCF_WRITE)
		memcpy(dobj->mem + io->addr, data->ptr, io->bytes);
	else
		memcpy(data->ptr, dobj->mem + io->addr, io->bytes);

	io->end(io, 0);
}

static void dobj_submit_flush(struct ocf_io *io)
{
	io->end(io, 0);
}

static void dobj_submit_discard(struct ocf_io *io)
{
	io->end(io, 0);
}

static unsigned int dobj_get_max_io_size(ocf_data_obj_t obj)
{
	return 4096;
}

static uint64_t dobj_get_length(ocf_data_obj_t obj)
{
	return 200*1024*1024;
}

const struct ocf_data_obj_properties dobj_properties = {
	.name = "Example dobj",
	.io_priv_size = sizeof(struct dobj_io),
	.dobj_priv_size = sizeof(struct dobj),
	.caps = {
		.atomic_writes = 0,
	},
	.ops = {
		.open = dobj_open,
		.close = dobj_close,
		.submit_io = dobj_submit_io,
		.submit_flush = dobj_submit_flush,
		.submit_discard = dobj_submit_discard,
		.get_max_io_size = dobj_get_max_io_size,
		.get_length = dobj_get_length,
	},
	.io_ops = {
		.set_data = intelcas_dobj_io_set_data,
		.get_data = intelcas_dobj_io_get_data,
	},
};

int dobj_init(ocf_ctx_t ocf_ctx)
{
	return ocf_ctx_register_data_obj_type(ocf_ctx, OBJ_TYPE,
			&dobj_properties);
}

void dobj_cleanup(ocf_ctx_t ocf_ctx)
{
	ocf_ctx_unregister_data_obj_type(ocf_ctx, OBJ_TYPE);
}
