#include <string.h>
#include <ctype.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <errno.h>

#include "debug.h"
#include "sock.h"
#include "util.h"
#include "trace.h"

static const char *
str(const char *in, ssize_t len)
{
	_Thread_local static char buf[44];

	char *p = buf, *pe = p + sizeof(buf) - sizeof("\\xff\"\"...");
	*p++ = '"';

	ssize_t i;
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

void
before_close(int fd)
{
	trace_stop(fd);
}

void
after_close(int rc,
		int fd)
{
	DEBUG("close(%d) = %s",
			fd, rcmsg(rc));
}

void
after_accept(int rc,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	(void)addrlen;
	DEBUG("accept(%d, \"%s\") = %s",
			sockfd, addr_encode(addr), rcmsg(rc));
	if (rc > -1) {
		trace_start(rc, sockfd);
	}
}

#if HAS_ACCEPT4 || HAS_SYS_ACCEPT4
void
after_accept4(int rc,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
	(void)addrlen;
	DEBUG("accept4(%d, \"%s\", %d) = %s",
			sockfd, addr_encode(addr), flags, rcmsg(rc));
	if (rc > -1) {
		trace_start(rc, sockfd);
	}
}
#endif

void
after_read(ssize_t rc,
		int fd, void *buf, size_t count)
{
	DEBUG_MORE("read(%d, %s, %zu) = %s",
			fd, str(buf, rc), count, rcmsg(rc));
	if (rc > 0) {
		trace(fd, buf, rc);
	}
}

#if HAS_READ_CHK
void
after___read_chk(ssize_t rc,
		int fd, void *buf, size_t nbytes, size_t buflen)
{
	DEBUG_MORE("__read_chk(%d, %s, %zu, %zu) = %s",
			fd, str(buf, rc), nbytes, buflen, rcmsg(rc));
	if (rc > 0) {
		trace(fd, buf, rc);
	}
}
#endif

void
after_readv(ssize_t rc,
		int fd, const struct iovec *iov, int iovcnt)
{
	DEBUG_MORE("readv(%d, %p, %d) = %s",
			fd, iov, iovcnt, rcmsg(rc));
	if (rc > 0) {
		tracev(fd, iov, iovcnt);
	}
}

void
after_recvfrom(ssize_t rc,
		int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	DEBUG_MORE("recvfrom(%d, %s, %zu, %d, %p, %p) = %s",
			sockfd, str(buf, rc), len, flags, src_addr, addrlen, rcmsg(rc));
	if (rc > 0) {
		trace(sockfd, buf, rc);
	}
}

void
after_recv(ssize_t rc, int sockfd, void *buf, size_t len, int flags)
{
	DEBUG_MORE("recv(%d, %s, %zu, %d) = %s",
			sockfd, str(buf, rc), len, flags, rcmsg(rc));
	if (rc > 0) {
		trace(sockfd, buf, rc);
	}
}

#if HAS_RECV_CHK
void
after___recv_chk(ssize_t rc, int sockfd, void *buf, size_t len, size_t buflen, int flags)
{
	DEBUG_MORE("__recv_chk(%d, %s, %zu, %zu, %d) = %s",
			sockfd, str(buf, rc), len, buflen, flags, rcmsg(rc));
	if (rc > 0) {
		trace(sockfd, buf, rc);
	}
}
#endif

#if HAS_RECVFROM_CHK
void
after___recvfrom_chk(ssize_t rc, int sockfd, void *buf, size_t len, size_t buflen, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	DEBUG_MORE("__recvfrom_chk(%d, %s, %zu, %zu, %d, %p, %p) = %s",
			sockfd, str(buf, rc), len, buflen, flags, src_addr, addrlen, rcmsg(rc));
	if (rc > 0) {
		trace(sockfd, buf, rc);
	}
}
#endif

void
after_recvmsg(ssize_t rc,
		int sockfd, struct msghdr *msg, int flags)
{
	DEBUG_MORE("recvmsg(%d, %p, %d) = %s",
			sockfd, msg, flags, rcmsg(rc));
	if (rc > 0) {
		tracev(sockfd, msg->msg_iov, msg->msg_iovlen);
	}
}

#if HAS_RECVMMSG
void
after_recvmmsg(int rc, int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
		int flags, struct timespec *timeout)
{
	DEBUG_MORE("recvmmsg(%d, %p, %u, %d, %p) = %s",
			sockfd, msgvec, vlen, flags, timeout, rcmsg(rc));
	if (rc > 0) {
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
}
#endif

