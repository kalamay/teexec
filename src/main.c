#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <getopt.h>
#include <err.h>

#include "cmd.h"
#include "proc.h"
#include "sock.h"
#include "debug.h"
#include "trace.h"

#if __APPLE__
#define ENV_PRELOAD "DYLD_INSERT_LIBRARIES="
#define ENV_DEBUG_LIBS "DYLD_PRINT_LIBRARIES=1"
#define ENV_DEBUG_STATS "DYLD_PRINT_STATISTIC=1"
#else
#define ENV_PRELOAD "LD_PRELOAD="
#define ENV_DEBUG_LIBS "LD_DEBUG=libs:statistics"
#endif
#define ENV_INIT "TEEXEC_INIT="

#define TRACE_DEFAULT "/tmp/teexec.sock"

static const struct opt opts[] = {
	{ 'v', "verbose",      NULL,   "verbose output (for furthur diagnostics repeat up to 4 )" },
	{ 't', "trace",        "sock", "trace socket (default \"" TRACE_DEFAULT "\")" },
	{ 'm', "multiplex",    NULL,   "bundle primary connections into a single channel" },
	{ 'E', "preserve-env", NULL,   "preserve environment variables" },
	{ 0,   NULL,           NULL,   NULL },
};

static const struct cmd cmd = {
	"teexec",
	opts,
	"command [args...]",
	NULL,
	NULL
};

int
main(int argc, char **argv, char **envp)
{
	unsetenv("TEEXEC_INIT");

	const char *trace = TRACE_DEFAULT;
	int verbose = 0;
	int mode = 0;
	bool preserve = false;
	int ch;
	while ((ch = cmd_getopt(argc, argv, &cmd)) != -1) {
		switch (ch) {
		case 'v': verbose++; break;
		case 't': trace = optarg; break;
		case 'm': mode |= TRACE_MULTIPLEX; break;
		case 'E': preserve = true; break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		errx(1, "command not set");
	}

	if (verbose > 1) {
		mode |= TRACE_DEBUG;
		if (verbose > 2) {
			mode |= TRACE_DEBUG_MORE;
		}
	}

	char exe[4096];
	if (!proc_find(argv[0], exe)) {
		errx(1, "command not found: %s", argv[0]);
	}

	struct sockopt opt = SOCKOPT_STREAM_PASSIVE;
	opt.nonblock = true;
	opt.cloexec = false;

	struct sock sock;
	if (!sock_open(&sock, &opt, trace)) {
		sock_perror(&sock);
		exit(1);
	}

	char env_lib[4096+sizeof(ENV_PRELOAD)];
	memcpy(env_lib, ENV_PRELOAD, sizeof(ENV_PRELOAD));
	strcat(env_lib, proc_path());
#ifdef LIBNAME
	strrchr(env_lib, '/')[0] = '\0';
	strrchr(env_lib, '/')[0] = '\0';
	strcat(env_lib, "/lib/" LIBNAME);
#endif

	char env_init[256];
	snprintf(env_init, sizeof(env_init), ENV_INIT "%d:%d", sock.fd, mode);

	int envc = 0;
	if (preserve) {
		for (char *const *e = envp; *e; e++, envc++) {}
	}

	char *env[envc+5];
	if (preserve) {
		for (int i = 0; i < envc; i++) { env[i] = envp[i]; }
	}
	env[envc++] = env_lib;
	if (verbose > 3) {
#	ifdef ENV_DEBUG_LIBS
		env[envc++] = ENV_DEBUG_LIBS;
#	endif
#	ifdef ENV_DEBUG_STATS
		env[envc++] = ENV_DEBUG_STATS;
#	endif
	}
	env[envc++] = env_init;
	env[envc] = NULL;

	char *name = strrchr(argv[0], '/');
	if (name) {
		argv[0] = name + 1;
	}

	if (verbose > 0) {
		debug_enable();

		DEBUG("self=%s", proc_path());
		fprintf(stderr, DEBUG_MSG("target=%s ["), DEBUG_ARGS(exe));
		for (int i = 0; argv[i]; i++) {
			if (i > 0) { fprintf(stderr, ", "); }
			fprintf(stderr, "\"%s\"", argv[i]);
		}
		fprintf(stderr, "]\n");
		DEBUG("trace=%s [%d]", trace, sock.fd);
		DEBUG("environment=%d", envc);
		for (int i = 0; i < envc; i++) {
			DEBUG("  %s", env[i]);
		}
	}

	execve(exe, argv, env);
	err(1, "failed to exec");
}

