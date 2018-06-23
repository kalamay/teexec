#include <stdlib.h>
#include <limits.h>

#include "debug.h"
#include "trace.h"

constructor(init)
{
	char *env, *end;
	long val;
	int fd = -1, mode = 0;

	if ((env = getenv("TEEXEC_INIT"))) {
		val = strtol(env, &end, 10);
		if (*end == ':' && val >= 0 && val <= INT_MAX) {
			fd = (int)val;
			val = strtol(end+1, &end, 10);
			if (*end == '\0' && val >= 0 && val <= INT_MAX) {
				mode = (int)val;
			}
		}
	}

	if (mode & TRACE_DEBUG) {
		debug_enable();
	}

	trace_init(fd, mode);
}

