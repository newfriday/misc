#include "aio.h"

AioContext *
aio_context_new(void);

QEMUBH *
aio_bh_new(AioContext *ctx, QEMUBHFunc *cb, void *opaque);

void
aio_list_bh(AioContext *ctx);
