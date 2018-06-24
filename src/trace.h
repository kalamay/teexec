#ifndef TEEXEC_TRACE_H
#define TEEXEC_TRACE_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdatomic.h>

#include "util.h"

#define TRACE_DEBUG      (1<<0)
#define TRACE_DEBUG_MORE (1<<1)
#define TRACE_MULTIPLEX  (1<<2)

void
trace_init(int fd, int mode);

void
trace_start(int clientfd, int serverfd);

void
trace_stop(int clientfd);

void
trace(int clientfd, const char *buf, ssize_t len);

void
tracev(int clientfd, const struct iovec *iov, size_t iovcnt);

#endif

