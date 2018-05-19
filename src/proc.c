#include "proc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sysctl.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <err.h>

const char *
proc_path(void)
{
	_Thread_local static char buf[4096];
	_Thread_local static bool init = true;
	if (init) {
#if defined(__linux)
		ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf)-1);
		if (n < 0) { err(1, "failed to get executable path"); }
#elif defined(__APPLE__)
		char tmp[sizeof(buf)*2];
		uint32_t tmplen = sizeof(tmp)-1;
		if (_NSGetExecutablePath(tmp, &tmplen) != 0) {
			err(1, "failed to get executable path");
		}
		tmp[tmplen] = '\0';
		if (!realpath(tmp, buf)) {
			err(1, "failed to get executable path");
		}
#elif defined(__FreeBSD__)
		int mib[4];
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PATHNAME;
		mib[3] = -1;
		size_t len = sizeof(buf)-1;
		if (sysctl(mib, 4, buf, &len, NULL, 0) != 0) {
			err(1, "failed to get executable path");
		}
		buf[len] = '\0';
#elif defined(BSD)
		ssize_t n = readlink("/proc/curproc/file", buf, sizeof(buf)-1);
		if (n < 0) { err(1, "failed to get executable path"); }
#else
#error platform not supported
#endif
		init = false;
	}
	return buf;
}

static bool
proc_access(const char *dir, size_t dirlen,
		const char *exe, size_t exelen,
		char buf[4096])
{
	if (dirlen > 0) {
		char tmp[8192];
		memcpy(tmp, dir, dirlen);
		tmp[dirlen] = '/';
		memcpy(tmp+dirlen+1, exe, exelen);
		tmp[dirlen+exelen+1] = '\0';
		if (access(tmp, X_OK) == 0 && realpath(tmp, buf)) {
			return true;
		}
	}
	return false;
}

const char *
proc_find(const char *exe, char buf[static 4096])
{
	size_t exelen = strlen(exe);

	/* First simply check if the executable path is executable as-is. */
	if (proc_access(".", 1, exe, exelen, buf)) {
		return buf;
	}

	/* If the path is absolute, there isn't anything to search for. */
	if (exe[0] == '/') {
		return 0;
	}

	char *path = getenv("PATH");
	if (path == NULL) {
		return 0;
	}

	char *pathsep = NULL;

	while ((pathsep = strchr(path, ':')) != NULL) {
		if (proc_access(path, pathsep - path, exe, exelen, buf)) {
			return buf;
		}
		path = pathsep + 1;
	}

	return NULL;
}

