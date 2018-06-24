#include "trace.h"
#include "debug.h"
#include "bypass.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <assert.h>

/* TODO: this is horribly thread-unsafe at the moment */

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

#define MULTIBUF 64

static int trace_mode = 0;
static int trace_fd = -1;
static int max_fd = 0;

struct entry { int fd; unsigned id; };
static struct entry *table = NULL;
static unsigned table_size = 0;
static unsigned table_scan = 0;
static unsigned table_id = 0;

#define POLLFD_1 { -1, POLLOUT, 0 }
#define POLLFD_2 POLLFD_1, POLLFD_1
#define POLLFD_4 POLLFD_2, POLLFD_2
#define POLLFD_8 POLLFD_4, POLLFD_4
#define POLLFD_16 POLLFD_8, POLLFD_8
#define POLLFD_32 POLLFD_16, POLLFD_16
#define POLLFD_64 POLLFD_32, POLLFD_32

struct pollfd reuse[] = { POLLFD_64 };

static bool
fd_trash(int tracefd)
{
	for (size_t i = 0; i < countof(reuse); i++) {
		if (reuse[i].fd < 0) {
			reuse[i].fd = tracefd;
			return true;
		}
	}
	return false;
}

static int
fd_restore(void)
{
	/* Poll with immediate timeout to detect any closed trace sockets. */
	int n = poll(reuse, countof(reuse), 0);
	if (n > 0) {
		for (size_t i = 0; i < countof(reuse); i++) {
			if (reuse[i].revents & (POLLERR|POLLHUP|POLLNVAL)) {
				DEBUG("pair closed: %d", reuse[i].fd);
				xclose(reuse[i].fd);
				reuse[i].fd = -1;
			}
			else if (reuse[i].revents & POLLOUT) {
				int tracefd = reuse[i].fd;
				reuse[i].fd = -1;
				return tracefd;
			}
		}
	}
	return -1;
}

static int
fd_get_pair(int clientfd)
{
	if (clientfd < 0 || (unsigned)clientfd >= table_size) {
		return -1;
	}
	return table[clientfd].fd - 1;
}

static unsigned
fd_get_id(int clientfd)
{
	if (clientfd < 0 || (unsigned)clientfd >= table_size) {
		return -1;
	}
	return table[clientfd].id;
}

static void
fd_pair(int clientfd, int tracefd)
{
	if ((unsigned)clientfd >= table_size) {
		unsigned sz = (unsigned)clientfd;
		if (sz > 0) {
			sz |= sz >> 1;
			sz |= sz >> 2;
			sz |= sz >> 4;
			sz |= sz >> 8;
			sz |= sz >> 16;
			sz++;
		}
		else {
			sz = 1024;
		}
		table = xrealloc(table, sz * sizeof(*table));
		memset(table + table_size, 0, sizeof(*table) * (sz - table_size));
		table_size = sz;
	}

	table[clientfd].fd = tracefd + 1;
	table[clientfd].id = ++table_id;
}

static int
fd_multi(void)
{
	int fd = -1;
	unsigned scan = table_scan, last = table_size-1, end = scan+last+1;
	for (; scan < end && fd < 0; scan++) {
		fd = fd_get_pair(scan & last);
	}
	table_scan = scan;
	return fd;
}

static void
fd_unpair(int clientfd, int tracefd, bool eof)
{
	table[clientfd].fd = 0;
	if (!eof) {
		eof = !fd_trash(tracefd);
	}
	if (eof) {
		xclose(tracefd);
	}
}

static void
fd_trace(int clientfd, int tracefd, struct iovec *iov, size_t iovcnt, ssize_t len)
{
	assert(iovcnt > 0);
	assert(iov[0].iov_len == 0);

	if (trace_mode & TRACE_MULTIPLEX) {
		/* The first iovec is an empty buffer for adding the multiplexing data. */
		unsigned id = fd_get_id(clientfd);
		int n = snprintf(iov->iov_base, MULTIBUF, "@%u#%zd\r\n", id, len);
		if (n > 0 && n <= MULTIBUF) {
			iov->iov_len = n;
			len += n;
		}
	}

	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = (struct iovec *)iov,
		.msg_iovlen = iovcnt,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0
	};

	ssize_t n = sendmsg(tracefd, &msg, MSG_NOSIGNAL|MSG_DONTWAIT);

	DEBUG("pair copy: %zd/%zd", n, len);
	if (n < len) {
		if (n < 0)       { DEBUG("pair failed: %d, %s", tracefd, strerror(errno)); }
		else if (n == 0) { DEBUG("pair closed: %d", tracefd); }
		else             { DEBUG("pair too slow: %d", tracefd); }
		fd_unpair(clientfd, tracefd, true);
	}
}

void
trace_init(int fd, int mode)
{
	struct rlimit limit;
	getrlimit(RLIMIT_NOFILE, &limit);

	max_fd = limit.rlim_max > INT_MAX ? INT_MAX : (int)limit.rlim_max;

	if (fd >= 0 && fd <= max_fd) {
		trace_fd = fd;
	}
	trace_mode = mode;
}

void
trace_start(int clientfd, int serverfd)
{
	if (clientfd < 0 || clientfd > max_fd) { return; }

	(void)serverfd;

	int tracefd = fd_restore();
	if (tracefd < 0) {
		tracefd = xaccept(trace_fd, true);
		if (tracefd < 0 && trace_mode & TRACE_MULTIPLEX) {
			tracefd = fd_multi();
		}
	}
	if (tracefd >= 0) {
		DEBUG("pair: %d->%d", clientfd, tracefd);
		fd_pair(clientfd, tracefd);
	}
}

void
trace_stop(int clientfd)
{
	int tracefd = fd_get_pair(clientfd);
	if (tracefd >= 0) {
		if (trace_mode & TRACE_MULTIPLEX) {
			char multi[MULTIBUF];
			struct iovec iov = { .iov_base = multi, .iov_len = 0 };
			fd_trace(clientfd, tracefd, &iov, 1, 0);
		}
		fd_unpair(clientfd, tracefd, false);
	}
}

void
trace(int clientfd, const char *buf, ssize_t len)
{
	if (len == 0) { return; }

	int tracefd = fd_get_pair(clientfd);
	if (tracefd > -1) {
		/* Set up an extra buffer for possible multiplexing. */
		char multi[MULTIBUF];
		struct iovec iov[2] = {
			{ .iov_base = multi, .iov_len = 0 },
			{ .iov_base = (char *)buf, .iov_len = len }
		};
		fd_trace(clientfd, tracefd, iov, countof(iov), len);
	}
}

void
tracev(int clientfd, const struct iovec *iov, size_t iovcnt)
{
	int tracefd = fd_get_pair(clientfd);
	if (tracefd > -1) {
		/* Set up an extra buffer for possible multiplexing. */
		char multi[MULTIBUF];
		struct iovec copy[iovcnt+1];
		copy[0].iov_base = multi;
		copy[0].iov_len = 0;

		ssize_t len = 0;

		for (size_t i = 0; i < iovcnt; i++) {
			len += iov[i].iov_len;
			copy[i+1] = iov[i];
		}

		if (len > 0) {
			fd_trace(clientfd, tracefd, copy, iovcnt+1, len);
		}
	}
}

