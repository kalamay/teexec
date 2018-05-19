#ifndef TEEXEC_TRACE_H
#define TEEXEC_TRACE_H

#include <sys/types.h>
#include <sys/socket.h>

void
trace_start(int clientfd, int serverfd);

void
trace_stop(int clientfd);

void
trace(int clientfd, const char *buf, ssize_t len);

void
tracev(int clientfd, const struct iovec *iov, size_t iovcnt);

#endif

