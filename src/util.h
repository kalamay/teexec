#ifndef TEEXEC_UTIL_H
#define TEEXEC_UTIL_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#define export __attribute__((visibility("default")))
#define constructor(name) __attribute__((constructor)) static void name(void)

#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef TEMP_FAILURE_RETRY
#	define retry TEMP_FAILURE_RETRY
#else
#	define retry(exp) __extension__ ({ \
		int rc; \
		do { rc = (exp); } \
		while (rc == -1 && errno == EINTR); \
		rc; \
	})
#endif

#define xoom() do { \
	fputs("out of memory\n", stderr); \
	abort(); \
} while (0)

#define xmalloc(sz) __extension__ ({ \
	void *val = malloc(sz); \
	if (val == NULL) { xoom(); } \
	val; \
})

#define xrealloc(ptr, sz) __extension__ ({ \
	void *val = realloc(ptr, sz); \
	if (val == NULL) { xoom(); } \
	val; \
})

#endif

