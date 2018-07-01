#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "util.h"
#include "advice.h"

#if __APPLE__

#include <unistd.h>

#include "sock.h"

#define hoist(name, ret, ...) \
	export ret hoist_##name(__VA_ARGS__); \
	__attribute__((used, section("__DATA,__interpose"))) \
	static struct { \
		ret (*src)(__VA_ARGS__); \
		ret (*dst)(__VA_ARGS__); \
	} interpose_##name = { \
		hoist_##name, \
		name \
	}; \
	ret hoist_##name(__VA_ARGS__)

#define libc(name) name

void
hoist_init(void)
{
}

#else

#include <dlfcn.h>

struct init {
	void (*init)(void);
};

#define hoist(name, ret, ...) \
	static ret (*libc_##name)(__VA_ARGS__); \
	static void hoist_##name(void) { libc_##name = dlsym(RTLD_NEXT, #name); } \
	__attribute__((used, section("teexec_hoist_init"))) \
	static struct init fp_##name = { hoist_##name }; \
	export ret name(__VA_ARGS__)

#define libc(name) libc_##name

void
hoist_init(void)
{
	extern struct init __start_teexec_hoist_init;
	extern struct init __stop_teexec_hoist_init;
	for (struct init *i = &__start_teexec_hoist_init; i != &__stop_teexec_hoist_init; i++) {
		i->init();
	}
}

#endif

#define join(name, ret, ...) do { \
	ret rc = libc(name)(__VA_ARGS__); \
	after_##name(rc, __VA_ARGS__); \
	return rc; \
} while (0)

hoist(close, int,
		int fd)
{
	before_close(fd);
	join(close, int, fd);
}

hoist(accept, int,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	join(accept, int, sockfd, addr, addrlen);
}

#if HAS_ACCEPT4
hoist(accept4, int,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	join(accept4, int, sockfd, addr, addrlen, flags);
}
#endif

#if HAS_SYS_ACCEPT4
hoist(syscall, long,
		long num, long a, long b, long c, long d, long e, long f)
{
	long n = libc(syscall)(num, a, b, c, d, e, f);
	if (unlikely(num == HAS_SYS_ACCEPT4)) {
		after_accept4((int)n, (int)a, (struct sockaddr *)b, (socklen_t *)c, (int)d);
	}
	return n;
}
#endif

hoist(read, ssize_t,
		int fd, void *buf, size_t count)
{
	join(read, ssize_t, fd, buf, count);
}

#if HAS_READ_CHK
hoist(__read_chk, ssize_t,
		int fd, void *buf, size_t nbytes, size_t buflen)
{
	join(__read_chk, ssize_t, fd, buf, nbytes, buflen);
}
#endif

hoist(readv, ssize_t,
		int fd, const struct iovec *iov, int iovcnt)
{
	join(readv, ssize_t, fd, iov, iovcnt);
}

hoist(recvfrom, ssize_t,
		int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	join(recvfrom, ssize_t, sockfd, buf, len, flags, src_addr, addrlen);
}

hoist(recv, ssize_t,
		int sockfd, void *buf, size_t len, int flags)
{
	join(recvfrom, ssize_t, sockfd, buf, len, flags, NULL, NULL);
}

#if HAS_RECV_CHK
hoist(__recv_chk, ssize_t,
		int sockfd, void *buf, size_t len, size_t buflen, int flags)
{
	join(__recv_chk, ssize_t, sockfd, buf, len, buflen, flags);
}
#endif

#if HAS_RECVFROM_CHK
hoist(__recvfrom_chk, ssize_t,
		int sockfd, void *buf, size_t len, size_t buflen, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	join(__recvfrom_chk, ssize_t, sockfd, buf, len, buflen, flags, src_addr, addrlen);
}
#endif

hoist(recvmsg, ssize_t,
		int sockfd, struct msghdr *msg, int flags)
{
	join(recvmsg, ssize_t, sockfd, msg, flags);
}

#if HAS_RECVMMSG
hoist(recvmmsg, int,
		int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
		int flags, struct timespec *timeout)
{
	join(recvmmsg, int, sockfd, msgvec, vlen, flags, timeout);
}
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

