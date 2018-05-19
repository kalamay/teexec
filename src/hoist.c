#include <stdio.h>
#include <sys/uio.h>
#include <errno.h>

#include "debug.h"
#include "sock.h"
#include "util.h"
#include "bypass.h"
#include "trace.h"

#define export __attribute__((visibility("default")))
#define constructor(name) __attribute__((constructor)) static void name(void)

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

hoist(close, int,
		int fd)
{
	trace_stop(fd);
	int rc = libc(close)(fd);
	DEBUG("close(%d) = %d",
			fd, rc);
	return rc;
}

hoist(accept, int,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	int n = libc(accept)(sockfd, addr, addrlen);
	if (n > -1) {
		DEBUG("accept(%d, %p, %p) = %d",
				sockfd, addr, addrlen, n);
		trace_start(n, sockfd);
	}
	return n;
}

#if HAS_ACCEPT4
hoist(accept4, int,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	int n = libc(accept4)(sockfd, addr, addrlen, flags);
	if (n > -1) {
		DEBUG("accept4(%d, %p, %p, %d) = %d",
				sockfd, addr, addrlen, flags, n);
		trace_start(n, sockfd);
	}
	return n;
}
#endif

hoist(read, ssize_t,
		int fd, void *buf, size_t count)
{
	ssize_t n = libc(read)(fd, buf, count);
	if (n > 0) {
		DEBUG("read(%d, %p, %zu) = %zd",
				fd, buf, count, n);
		trace(fd, buf, n);
	}
	return n;
}

#if HAS_READ_CHK
hoist(__read_chk, ssize_t,
		int fd, void *buf, size_t nbytes, size_t buflen)
{
	ssize_t n = libc(__read_chk)(fd, buf, nbytes, buflen);
	if (n > 0) {
		DEBUG("read(%d, %p, %zu) = %zd",
				fd, buf, nbytes, n);
		trace(fd, buf, n);
	}
	return n;
}
#endif

hoist(readv, ssize_t,
		int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t n = libc(readv)(fd, iov, iovcnt);
	if (n > 0) {
		DEBUG("readv(%d, %p, %d) = %zd",
				fd, iov, iovcnt, n);
		tracev(fd, iov, iovcnt);
	}
	return n;
}

hoist(recvfrom, ssize_t,
		int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	ssize_t n = libc(recvfrom)(sockfd, buf, len, flags, src_addr, addrlen);
	if (n > 0) {
		DEBUG("recvfrom(%d, %p, %zu, %d, %p, %p) = %zd",
				sockfd, buf, len, flags, src_addr, addrlen, n);
		trace(sockfd, buf, n);
	}
	return n;
}

hoist(recv, ssize_t,
		int sockfd, void *buf, size_t len, int flags)
{
	ssize_t n = libc(recvfrom)(sockfd, buf, len, flags, NULL, NULL);
	if (n > 0) {
		DEBUG("recv(%d, %p, %zu, %d) = %zd",
				sockfd, buf, len, flags, n);
		trace(sockfd, buf, n);
	}
	return n;
}

#if HAS_RECV_CHK
hoist(__recv_chk, ssize_t,
		int sockfd, void *buf, size_t len, size_t buflen, int flags)
{
	ssize_t n = libc(__recv_chk)(sockfd, buf, len, buflen, flags);
	if (n > 0) {
		DEBUG("recv(%d, %p, %zu, %d) = %zd",
				sockfd, buf, len, flags, n);
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
	if (n > 0) {
		DEBUG("recvfrom(%d, %p, %zu, %d, %p, %p) = %zd",
				sockfd, buf, len, flags, src_addr, addrlen, n);
		trace(sockfd, buf, n);
	}
	return n;
}
#endif

hoist(recvmsg, ssize_t,
		int sockfd, struct msghdr *msg, int flags)
{
	ssize_t n = libc(recvmsg)(sockfd, msg, flags);
	if (n > 0) {
		DEBUG("recvmsg(%d, %p, %d) = %zd",
				sockfd, msg, flags, n);
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
	if (n > 0) {
		DEBUG("recvmmsg(%d, %p, %u, %d, %p) = %d",
				sockfd, msgvec, vlen, flags, timeout, n);

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
	int fd = libc(accept4)(s, (struct sockaddr *)&ss, &slen,
			SOCK_CLOEXEC | (nonblock ? SOCK_NONBLOCK : 0));
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

