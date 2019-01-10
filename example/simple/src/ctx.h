#ifndef __CTX_H__
#define __CTX_H__

#include <ocf/ocf.h>

#define OBJ_TYPE 1

ctx_data_t *ctx_data_alloc(uint32_t pages);
void ctx_data_free(ctx_data_t *ctx_data);

int ctx_init(ocf_ctx_t *ocf_ctx);
void ctx_cleanup(ocf_ctx_t ctx);

#endif
