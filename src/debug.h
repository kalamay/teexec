#ifndef TEEXEC_DEBUG_H
#define TEEXEC_DEBUG_H

#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>

atomic_bool debug_enabled;

void
debug_enable(void);

#define DEBUG_ENABLED \
	(__builtin_expect(atomic_load_explicit(&debug_enabled, memory_order_relaxed), 0))

#define DEBUG_MSG(msg) "%10d:\t" msg
#define DEBUG_ARGS(...) getpid(), ##__VA_ARGS__
#define DEBUG(msg, ...) do { \
	if (DEBUG_ENABLED) { \
		fprintf(stderr, DEBUG_MSG(msg) "\n", DEBUG_ARGS(__VA_ARGS__)); \
	} \
} while (0); \

#endif

