#ifndef TEEXEC_DEBUG_H
#define TEEXEC_DEBUG_H

#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>

atomic_bool debug_enabled;
atomic_bool debug_more_enabled;

void
debug_enable(void);

void
debug_more_enable(void);

#define DEBUG_CHECK(f) unlikely(atomic_load_explicit(&(f), memory_order_relaxed))
#define DEBUG_ENABLED DEBUG_CHECK(debug_enabled)
#define DEBUG_MORE_ENABLED DEBUG_CHECK(debug_more_enabled)

#define DEBUG_MSG(msg) "%10d:\t" msg
#define DEBUG_ARGS(...) getpid(), ##__VA_ARGS__
#define DEBUG_IF(c, msg, ...) do { \
	if (c) { \
		fprintf(stderr, DEBUG_MSG(msg) "\n", DEBUG_ARGS(__VA_ARGS__)); \
	} \
} while (0)
#define DEBUG(...) DEBUG_IF(DEBUG_ENABLED, __VA_ARGS__)
#define DEBUG_MORE(...) DEBUG_IF(DEBUG_MORE_ENABLED, __VA_ARGS__)

#endif

