#include "debug.h"

#include <stdlib.h>

static void __attribute__((constructor))
init(void)
{
	char *env = getenv("TEEXEC_DEBUG");
	if (env && env[0] == '1') {
		debug_enable();
	}
}

void
debug_enable(void)
{
	atomic_store_explicit(&debug_enabled, 1, memory_order_relaxed);
}

