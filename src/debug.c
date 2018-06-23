#include "debug.h"

void
debug_enable(void)
{
	atomic_store_explicit(&debug_enabled, 1, memory_order_relaxed);
}

