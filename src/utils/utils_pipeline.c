#include "ocf/ocf.h"
#include "../engine/cache_engine.h"
#include "../engine/engine_common.h"
#include "utils_pipeline.h"
#include "utils_req.h"

struct ocf_pipeline {
	struct ocf_pipeline_properties *properties;
	struct ocf_request *req;
	int next_step;
	bool finish;
	int error;

	void *priv;
};

static int _ocf_pipeline_run_step(struct ocf_request *req)
{
	ocf_pipeline_t pipeline = req->priv;
	struct ocf_pipeline_step *step;

	step = &pipeline->properties->steps[pipeline->next_step++];

	if (step->cb && !pipeline->finish) {
		step->cb(pipeline, pipeline->priv, step->arg);
	} else {
		pipeline->properties->finish(pipeline, pipeline->priv,
				pipeline->error);
	}

	return 0;
}

static const struct ocf_io_if _io_if_pipeline = {
	.read = _ocf_pipeline_run_step,
	.write = _ocf_pipeline_run_step,
};

int ocf_pipeline_create(ocf_pipeline_t *pipeline, ocf_cache_t cache,
		struct ocf_pipeline_properties *properties, void *priv)
{
	ocf_pipeline_t tmp_pipeline;
	struct ocf_request *req;

	tmp_pipeline = env_vzalloc(sizeof(struct ocf_pipeline));
	if (!tmp_pipeline)
		return -OCF_ERR_NO_MEM;

	req = ocf_req_new(cache->mngt_queue, NULL, 0, 0, 0);
	if (!req) {
		env_vfree(tmp_pipeline);
		return -OCF_ERR_NO_MEM;
	}

	tmp_pipeline->properties = properties;
	tmp_pipeline->req = req;
	tmp_pipeline->next_step = 0;
	tmp_pipeline->finish = false;
	tmp_pipeline->error = 0;
	tmp_pipeline->priv = priv;

	req->info.internal = true;
	req->io_if = &_io_if_pipeline;
	req->priv = tmp_pipeline;

	*pipeline = tmp_pipeline;

	return 0;
}

void ocf_pipeline_destroy(ocf_pipeline_t pipeline)
{
	ocf_req_put(pipeline->req);
	env_vfree(pipeline);
}

void *ocf_pipeline_get_priv(ocf_pipeline_t pipeline)
{
	return pipeline->priv;
}

void ocf_pipeline_next(ocf_pipeline_t pipeline)
{
	ocf_engine_push_req_front(pipeline->req, true);
}

void ocf_pipeline_finish(ocf_pipeline_t pipeline, int error)
{
	pipeline->finish = true;
	pipeline->error = error;
	ocf_engine_push_req_front(pipeline->req, true);
}
