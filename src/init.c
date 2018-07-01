#include <stdlib.h>
#include <limits.h>
#include <sys/resource.h>

#include "debug.h"
#include "hoist.h"
#include "trace.h"

constructor(init)
{
	char *env, *end;
	int max_fd;
	long fd, mode;
	struct rlimit limit;

	/* Get the maximum number of file descriptors. This will limit the
	 * valid range for the configured file descriptor, and it will be
	 * used to configure the trace system. */
	getrlimit(RLIMIT_NOFILE, &limit);
	max_fd = limit.rlim_max > INT_MAX ? INT_MAX : (int)limit.rlim_max;

	/* Check for the TEEXEC_INIT environment variable with the format:
	 *
	 *     fd:flags
	 *
	 * where `fd` is the integer value of the inherited trace socket and
	 * `flags` is the bit flags to configure the run mode. */
	if (!(env = getenv("TEEXEC_INIT"))) { return; }
	fd = strtol(env, &end, 10);
	if (*end != ':' || fd < 0 || fd > max_fd) { return; }
	mode = strtol(end+1, &end, 10);
	if (*end != '\0' || mode < 0 || mode > INT_MAX) { return; }

	/* We've got a possibly valid file descriptor and flag set. */
	if (mode & TRACE_DEBUG) {
		debug_enable();
	}
	if (mode & TRACE_DEBUG_MORE) {
		debug_more_enable();
	}
	hoist_init();
	trace_init(max_fd, (int)fd, (int)mode);
}

