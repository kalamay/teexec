#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#ifndef countof
# define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef retry
#	ifdef TEMP_FAILURE_RETRY
#		define retry TEMP_FAILURE_RETRY
#	else
#		define retry(exp) __extension__ ({ \
			int rc; \
			do { rc = (exp); } \
			while (rc == -1 && errno == EINTR); \
			rc; \
		})
#	endif
#endif

#ifndef xoom
#	define xoom() do { \
		fputs("out of memory\n", stderr); \
		abort(); \
	} while (0)
#endif

#ifndef xmalloc
#	define xmalloc(sz) __extension__ ({ \
		void *val = malloc(sz); \
		if (val == NULL) { xoom(); } \
		val; \
	})
#endif

#ifndef xrealloc
#	define xrealloc(ptr, sz) __extension__ ({ \
		void *val = realloc(ptr, sz); \
		if (val == NULL) { xoom(); } \
		val; \
	})
#endif
