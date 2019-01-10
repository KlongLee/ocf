#include <ocf/ocf.h>
#include "dobj.h"
#include "ctx.h"

void complete(struct ocf_io *io, int error)
{
	struct dobj_data *data;

	printf("Complete (error=%d)\n", error);
	data = ocf_io_get_data(io);

	if (io->dir == OCF_READ)
		printf("DATA: %s\n", (char *)data->ptr);

	ctx_data_free(data);
	ocf_io_put(io);
}

int main(int argc, char *argv[])
{
	ocf_ctx_t ctx;
	ocf_cache_t cache1;
	ocf_core_t core1;
	struct ocf_io *io;
	struct dobj_data *data;

	struct ocf_mngt_cache_config cache_cfg = {
		.cache_mode = ocf_cache_mode_wb,
		.metadata_volatile = true,
		.backfill.max_queue_size = 65536,
		.backfill.queue_unblock_size = 60000,
		.cache_line_size = 4096,
		.io_queues = 1,
		.name = "cache1",
		.name_size = 7,
	};

	struct ocf_mngt_cache_device_config device_cfg = {
		.uuid.data = "cache",
		.uuid.size = 6,
		.data_obj_type = OBJ_TYPE,
		.discard_on_start = false,
		.cache_line_size = 4096,
		.force = true,
	};

	struct ocf_mngt_core_config core_cfg = {
		.uuid.data = "core",
		.uuid.size = 5,
		.data_obj_type = OBJ_TYPE,
		.core_id = 1,
		.name = "core1",
		.name_size = 6,
	};

	ctx_init(&ctx);
	dobj_init(ctx);
	ocf_mngt_cache_start(ctx, &cache1, &cache_cfg);
	ocf_mngt_cache_attach(cache1, &device_cfg);
	ocf_mngt_cache_add_core(cache1, &core1, &core_cfg);

	data = ctx_data_alloc(1);
	strcpy(data->ptr, "This is some test data");
	io = ocf_core_new_io(core1);
	ocf_io_configure(io, 0, 512, OCF_WRITE, 0, 0);
	ocf_io_set_queue(io, 0);
	ocf_io_set_data(io, data, 0);
	ocf_io_set_cmpl(io, NULL, NULL, complete);
	ocf_core_submit_io(io);

	data = ctx_data_alloc(1);
	io = ocf_core_new_io(core1);
	ocf_io_configure(io, 0, 512, OCF_READ, 0, 0);
	ocf_io_set_queue(io, 0);
	ocf_io_set_data(io, data, 0);
	ocf_io_set_cmpl(io, NULL, NULL, complete);
	ocf_core_submit_io(io);

	return 0;
}
