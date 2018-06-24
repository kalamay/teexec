#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/uio.h>
#include <errno.h>

#include "debug.h"
#include "sock.h"
#include "util.h"
#include "bypass.h"
#include "trace.h"

#if __APPLE__

#define hoist(name, ret, ...) \
	export ret hoist_##name(__VA_ARGS__); \
	__attribute__((used)) \
	__attribute__((section("__DATA,__interpose"))) \
	static struct { \
		ret (*src)(__VA_ARGS__); \
		ret (*dst)(__VA_ARGS__); \
	} interpose_##name = { \
		hoist_##name, \
		name \
	}; \
	ret hoist_##name(__VA_ARGS__)

#define libc(name) name

#else

#include <dlfcn.h>

#define hoist(name, ret, ...) \
	static ret (*libc_##name)(__VA_ARGS__); \
	constructor(init_##name) { libc_##name = dlsym(RTLD_NEXT, #name); } \
	export ret name(__VA_ARGS__)

#define libc(name) libc_##name

#endif

static const char *
str(const char *in, size_t len)
{
	_Thread_local static char buf[44];

	char *p = buf, *pe = p + sizeof(buf) - sizeof("\\xff\"\"...");
	*p++ = '"';

	size_t i;
	for (i = 0; i < len && p < pe; i++) {
		char ch = in[i];
		switch (ch) {
		case '\0': memcpy(p, "\\0", 2); p += 2; break;
		case '\a': memcpy(p, "\\a", 2); p += 2; break;
		case '\b': memcpy(p, "\\b", 2); p += 2; break;
		case '\f': memcpy(p, "\\f", 2); p += 2; break;
		case '\n': memcpy(p, "\\n", 2); p += 2; break;
		case '\r': memcpy(p, "\\r", 2); p += 2; break;
		case '\t': memcpy(p, "\\t", 2); p += 2; break;
		case '\v': memcpy(p, "\\v", 2); p += 2; break;
		case '\\': memcpy(p, "\\\\", 2); p += 2; break;
		case '\'': memcpy(p, "'", 2); p += 2; break;
		case '\"': memcpy(p, "\"", 2); p += 2; break;
		default:
			if (isprint(ch)) {
				*p++ = ch;
			}
			else {
				int n = snprintf(p, pe - p, "\\x%02x", ch);
				if (n < 0 || n > pe - p) {
					break;
				}
				p += n;
			}
		}
	}
	*p++ = '"';

	if (i < len) {
		memcpy(p, "...", 3);
		p += 3;
	}
	*p = '\0';
	return buf;
}

static const char *
rcmsg(ssize_t rc)
{
	_Thread_local static char buf[64];
	int n1 = snprintf(buf, sizeof(buf), "%zd", rc);
	if (rc < 0) {
		int n2 = snprintf(buf+n1, sizeof(buf)-n1, ", %s", strerror(errno));
		if (n2 < 0 || n2 >= (int)sizeof(buf)-n1) {
			buf[n1] = '\0';
		}
	}
	return buf;
}

hoist(close, int,
		int fd)
{
	trace_stop(fd);
	int rc = libc(close)(fd);
	DEBUG("close(%d) = %s",
			fd, rcmsg(rc));
	return rc;
}

hoist(accept, int,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int n = libc(accept)(sockfd, addr, addrlen);
	DEBUG("accept(%d, \"%s\") = %s",
			sockfd, addr_encode(addr), rcmsg(n));
	if (n > -1) {
		trace_start(n, sockfd);
	}
	return n;
}

#if HAS_ACCEPT4
hoist(accept4, int,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	int n = libc(accept4)(sockfd, addr, addrlen, flags);
	DEBUG("accept4(%d, \"%s\", %d) = %s",
			sockfd, addr_encode(addr), flags, rcmsg(n));
	if (n > -1) {
		trace_start(n, sockfd);
	}
	return n;
}
#endif

hoist(read, ssize_t,
		int fd, void *buf, size_t count)
{
	ssize_t n = libc(read)(fd, buf, count);
	DEBUG_MORE("read(%d, %s, %zu) = %s",
			fd, str(buf, (size_t)n), count, rcmsg(n));
	if (n > 0) {
		trace(fd, buf, n);
	}
	return n;
}

#if HAS_READ_CHK
hoist(__read_chk, ssize_t,
		int fd, void *buf, size_t nbytes, size_t buflen)
{
	ssize_t n = libc(__read_chk)(fd, buf, nbytes, buflen);
	DEBUG_MORE("__read_chk(%d, %s, %zu, %zu) = %s",
			fd, str(buf, (size_t)n), nbytes, buflen, rcmsg(n));
	if (n > 0) {
		trace(fd, buf, n);
	}
	return n;
}
#endif

hoist(readv, ssize_t,
		int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t n = libc(readv)(fd, iov, iovcnt);
	DEBUG_MORE("readv(%d, %p, %d) = %s",
			fd, iov, iovcnt, rcmsg(n));
	if (n > 0) {
		tracev(fd, iov, iovcnt);
	}
	return n;
}

hoist(recvfrom, ssize_t,
		int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	ssize_t n = libc(recvfrom)(sockfd, buf, len, flags, src_addr, addrlen);
	DEBUG_MORE("recvfrom(%d, %s, %zu, %d, %p, %p) = %s",
			sockfd, str(buf, (size_t)n), len, flags, src_addr, addrlen, rcmsg(n));
	if (n > 0) {
		trace(sockfd, buf, n);
	}
	return n;
}

hoist(recv, ssize_t,
		int sockfd, void *buf, size_t len, int flags)
{
	ssize_t n = libc(recvfrom)(sockfd, buf, len, flags, NULL, NULL);
	DEBUG_MORE("recv(%d, %s, %zu, %d) = %s",
			sockfd, str(buf, (size_t)n), len, flags, rcmsg(n));
	if (n > 0) {
		trace(sockfd, buf, n);
	}
	return n;
}

#if HAS_RECV_CHK
hoist(__recv_chk, ssize_t,
		int sockfd, void *buf, size_t len, size_t buflen, int flags)
{
	ssize_t n = libc(__recv_chk)(sockfd, buf, len, buflen, flags);
	DEBUG_MORE("__recv_chk(%d, %s, %zu, %zu, %d) = %s",
			sockfd, str(buf, (size_t)n), len, buflen, flags, rcmsg(n));
	if (n > 0) {
		trace(sockfd, buf, n);
	}
	return n;
}
#endif

#if HAS_RECVFROM_CHK
hoist(__recvfrom_chk, ssize_t,
		int sockfd, void *buf, size_t len, size_t buflen, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	ssize_t n = libc(__recvfrom_chk)(sockfd, buf, len, buflen, flags, src_addr, addrlen);
	DEBUG_MORE("__recvfrom_chk(%d, %s, %zu, %zu, %d, %p, %p) = %s",
			sockfd, str(buf, (size_t)n), len, buflen, flags, src_addr, addrlen, rcmsg(n));
	if (n > 0) {
		trace(sockfd, buf, n);
	}
	return n;
}
#endif

hoist(recvmsg, ssize_t,
		int sockfd, struct msghdr *msg, int flags)
{
	ssize_t n = libc(recvmsg)(sockfd, msg, flags);
	DEBUG_MORE("recvmsg(%d, %p, %d) = %s",
			sockfd, msg, flags, rcmsg(n));
	if (n > 0) {
		tracev(sockfd, msg->msg_iov, msg->msg_iovlen);
	}
	return n;
}

#if HAS_RECVMMSG
hoist(recvmmsg, int,
		int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
		int flags, struct timespec *timeout)
{
	int n = libc(recvmmsg)(sockfd, msgvec, vlen, flags, timeout);
	DEBUG_MORE("recvmmsg(%d, %p, %u, %d, %p) = %s",
			sockfd, msgvec, vlen, flags, timeout, rcmsg(n));
	if (n > 0) {
		size_t iovcnt = 0;
		for (unsigned int i = 0; i < vlen; i++) {
			iovcnt += msgvec[i].msg_hdr.msg_iovlen;
		}

		struct iovec iov[iovcnt], *p = iov;
		for (unsigned int i = 0; i < vlen; i++) {
			for (size_t j = 0; j < msgvec[i].msg_hdr.msg_iovlen; j++, p++) {
				*p = msgvec[i].msg_hdr.msg_iov[j];
			}
		}

		tracev(sockfd, iov, iovcnt);
	}
	return n;
}
#endif

#if 0
#if HAS_SPLICE
hoist(splice, ssize_t,
		int fd_in, loff_t *off_in,
		int fd_out, loff_t *off_out,
		size_t len, unsigned int flags)
{
	ssize_t n = libc(splice)(fd_in, off_in, fd_out, off_out, len, flags);
	DEBUG("splice(%d, %p->%ld, %d, %p->%ld, %zu, %u) = %zd",
			fd_in, off_in, *off_in, fd_out, off_out, *off_out, len, flags, n);
	return n;
}
#endif
#endif

int xclose(int fd)
{
	return retry(libc(close)(fd));
}

int xaccept(int s, bool nonblock)
{
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
#if HAS_ACCEPT4
	int fd = retry(libc(accept4)(s, (struct sockaddr *)&ss, &slen,
			SOCK_CLOEXEC | (nonblock ? SOCK_NONBLOCK : 0)));
#else
	int fd = retry(libc(accept)(s, (struct sockaddr *)&ss, &slen));
	if (fd >= 0) {
		sock_cloexec(fd, true);
		if (nonblock) {
			sock_nonblock(fd, true);
		}
	}
#endif
	return fd;
}

