#include "sock.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/tcp.h>

static bool
parse_int(const char *net, int *out)
{
	char *end;
	long l = strtol(net, &end, 10);
	if (*end != '\0' || l < INT_MIN || l > INT_MAX) {
		return false;
	}
	*out = (int)l;
	return true;
}

static inline bool
set_error(struct sock *sock, int ec, int et)
{
	sock->error = ec;
	sock->errortype = et;
	return ec == SOCK_ERROR_NONE;
}

bool
sock_open(struct sock *sock, const struct sockopt *opt, const char *net)
{
	/* If the full net value is an integer, interpret the input as using an
	 * existing (and presumably inherited) file descriptor. */
	int fd;
	if (parse_int(net, &fd)) {
		return sock_set_fd(sock, opt, fd);
	}

	/* Otherwise search for a ':' character separating the host name and the
	 * service name or port number. If no such character is found, treat net
	 * as a path for a UNIX socket. */
	const char *serv = strchr(net, ':');
	if (serv == NULL) {
		return sock_set_un(sock, opt, net);
	}

	char host[256];
	if (serv - net > (ssize_t)sizeof(host) - 1) {
		return set_error(sock, ENAMETOOLONG, SOCK_ERROR_SYS);
	}

	memcpy(host, net, serv - net);
	host[serv - net] = '\0';
	serv++;

	return sock_set_inet(sock, opt, host, serv);
}

bool
sock_accept(struct sock *sock, const struct sockopt *opt, int s)
{
	socklen_t slen = sizeof(sock->addr);

	/* If using accept4, we can skip the syscalls to set non-blocking and
	 * close-exec behavior. This is only available on Linux currently. */
#if HAS_ACCEPT4
	int flags = 0;
	if (opt && opt->cloexec) { flags |= SOCK_CLOEXEC; }
	if (opt && opt->nonblock) { flags |= SOCK_NONBLOCK; }
	int fd = retry(accept4(s, &sock->addr.sa, &slen, flags));
#else
	int fd = retry(accept(s, &sock->addr.sa, &slen));
#endif

	if (fd < 0) {
		return set_error(sock, errno, SOCK_ERROR_ACCEPT);
	}

	/* Additional socket options are not required for accept. */
	if (opt && !sock_setopt(fd, opt)) {
		set_error(sock, errno, SOCK_ERROR_SYS);
		retry(close(fd));
		return false;
	}

	/* Set non-blocking and close-on-exec behavior if we couldn't use accept4 */
#if !HAS_ACCEPT4
	if (opt && opt->nonblock && !sock_nonblock(fd, true)) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}
	if (opt && opt->cloexec && !sock_cloexec(fd, true)) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}
#endif

	sock->fd = fd;
	sock->passive = false;
	return set_error(sock, 0, SOCK_ERROR_NONE);
}

void
sock_close(struct sock *sock)
{
	/* Close can fail from an EINTR. */
	retry(close(sock->fd));

	/* For passive (listening) UNIX sockets, we unlink the file on close. */
	if (sock->passive && sock->addr.sa.sa_family == AF_UNIX) {
		unlink(sock->addr.un.sun_path);
	}

	sock->fd = -1;
	sock->passive = false;
}

static bool
set_addr(struct sock *sock, const struct sockopt *opt, int family, const struct sockaddr *addr, socklen_t addrlen)
{
	/* On Linux, socket may optionally take flags on the type argument to
	 * control non-block and close-on-exec behavior. This avoids some syscalls
	 * when applying the socket options. */
#if HAS_SOCK_FLAGS
	int type = opt->type;
	if (opt->cloexec) { type |= SOCK_CLOEXEC; }
	if (opt->nonblock) { type |= SOCK_NONBLOCK; }
	int fd = socket(family, type, 0);
#else
	int fd = socket(family, opt->type, 0);
#endif

	if (fd < 0) {
		set_error(sock, errno, SOCK_ERROR_SOCKET);
		goto error;
	}

	/* Apply socket options. */
	if (!sock_setopt(fd, opt)) {
		set_error(sock, errno, SOCK_ERROR_SYS);
		goto error;
	}

	/* Set non-blocking and close-on-exec behavior if we couldn't use the
	 * socket flags. */
#if !HAS_SOCK_FLAGS
	if (opt->nonblock && !sock_nonblock(fd, true)) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}
	if (opt->cloexec && !sock_cloexec(fd, true)) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}
#endif

	if (opt->passive) {
		if (bind(fd, addr, addrlen) < 0) {
			set_error(sock, errno, SOCK_ERROR_BIND);
			goto error;
		}
		if (opt->type == SOCK_STREAM && listen(fd, opt->backlog) < 0) {
			set_error(sock, errno, SOCK_ERROR_LISTEN);
			goto error;
		}
	}
	else {
		if (retry(connect(fd, addr, addrlen)) < 0 && errno != EINPROGRESS) {
			set_error(sock, errno, SOCK_ERROR_CONNECT);
			goto error;
		}
	}

	sock->fd = fd;
	sock->passive = opt->passive;
	return set_error(sock, 0, SOCK_ERROR_NONE);

error:
	retry(close(fd));
	return false;
}

bool
sock_set_inet(struct sock *sock, const struct sockopt *opt, const char *host, const char *serv)
{
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = opt->family;
	hints.ai_socktype = opt->type;
	hints.ai_flags = opt->passive ? AI_PASSIVE : 0;

	int ec = getaddrinfo(host, serv, &hints, &res);
	if (ec) {
		return set_error(sock, ec, SOCK_ERROR_GAI);
	}

	for (struct addrinfo *r = res; res; res = res->ai_next) {
		if (set_addr(sock, opt, r->ai_family, r->ai_addr, r->ai_addrlen)) {
			memcpy(&sock->addr.ss, r->ai_addr, r->ai_addrlen);
			break;
		}
	}
	freeaddrinfo(res);

	return sock->errortype == SOCK_ERROR_NONE;
}

bool
sock_set_un(struct sock *sock, const struct sockopt *opt, const char *path)
{
	size_t n = strnlen(path, sizeof(sock->addr.un.sun_path));
	if (n >= sizeof(sock->addr.un.sun_path)) {
		return set_error(sock, ENAMETOOLONG, SOCK_ERROR_SYS);
	}

	sock->addr.un.sun_family = AF_UNIX;
	memcpy(sock->addr.un.sun_path, path, n);
	sock->addr.un.sun_path[n] = '\0';

	if (opt->passive && opt->reuseaddr && unlink(path) < 0 && errno != ENOENT) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}

	return set_addr(sock, opt, AF_UNIX, &sock->addr.sa, sizeof(sock->addr.un));
}

bool
sock_set_fd(struct sock *sock, const struct sockopt *opt, int fd)
{
	int sval;
	socklen_t slen;

	sock->fd = fd;
	sock->passive = false;

	/* Check the socket type of the file descriptor. The getsockopt call will
	 * fail if it is not a socket, thereby ensuring we have both a socket and
	 * it is of the desired type. */
	slen = sizeof(sval);
	if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &sval, &slen) < 0) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}
	if (sval != opt->type) {
		return set_error(sock, EINVAL, SOCK_ERROR_SYS);
	}

	/* Check if the socket listening is enabled. This allows us to update the
	 * passive field of the sock struct. */
	slen = sizeof(sval);
	if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &sval, &slen) < 0) {
		return set_error(sock, errno, SOCK_ERROR_SYS);
	}
	sock->passive = sval;

	/* Get the address of the socket. */
	slen = sizeof(sock->addr);
	if (getsockname(fd, &sock->addr.sa, &slen) < 0) {
		return set_error(sock, EINVAL, SOCK_ERROR_SYS);
	}

	return set_error(sock, 0, SOCK_ERROR_NONE);
}

bool
sock_setopt(int fd, const struct sockopt *opt)
{
	int on = 1;

	if (opt->reuseaddr &&
			setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		return false;
	}

#ifdef SO_REUSEPORT
	if (opt->reuseport &&
			setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
		return false;
	}
#else
	if (opt->reuseport) {
		errno = ENOTSUP;
		return false;
	}
#endif

	if (opt->keepalive &&
			setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0) {
		return false;
	}

	if (opt->sndbuf > 0 &&
			setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &opt->sndbuf, sizeof(opt->sndbuf)) < 0) {
		return false;
	}
	if (opt->rcvbuf > 0 &&
			setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt->rcvbuf, sizeof(opt->rcvbuf)) < 0) {
		return false;
	}

	if (opt->type == SOCK_STREAM) {
#ifdef TCP_DEFER_ACCEPT
		if (opt->defer_accept) {
			setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &on, sizeof(on));
		}
#endif
		if (opt->nodelay) {
			setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
		}
	}
#ifdef SO_NOSIGPIPE
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
	return true;
}

bool
sock_nonblock(int fd, bool on)
{
	int old = fcntl(fd, F_GETFL, 0);
	if (old < 0) { return false; }
	int new = on ? old | O_NONBLOCK : old & ~O_NONBLOCK;
	return old != new && fcntl(fd, F_SETFL, new) < 0 ? false : true;
}

bool
sock_cloexec(int fd, bool on)
{
	int old = fcntl(fd, F_GETFD, 0);
	if (old < 0) { return false; }
	int new = on ? old | FD_CLOEXEC : old & ~FD_CLOEXEC;
	return old != new && fcntl(fd, F_SETFD, new) < 0 ? false : true;
}

int
sock_error(const struct sock *sock, char *buf, size_t buflen)
{
	static const char *types[] = {
		[SOCK_ERROR_SYS]     = "system",
		[SOCK_ERROR_SOCKET]  = "socket",
		[SOCK_ERROR_CONNECT] = "connect",
		[SOCK_ERROR_BIND]    = "bind",
		[SOCK_ERROR_LISTEN]  = "listen",
		[SOCK_ERROR_ACCEPT]  = "accept",
		[SOCK_ERROR_SETOPT]  = "setsockopt",
		[SOCK_ERROR_GAI]     = "getaddrinfo",
	};

	size_t type = sock->errortype;
	if (type > SOCK_ERROR_NONE && type < countof(types)) {
		const char *msg = type == SOCK_ERROR_GAI ?
			gai_strerror(sock->error) : strerror(sock->error);
		return snprintf(buf, buflen, "%s error: %s\n", types[type], msg);
	}
	return 0;
}

void
sock_perror(const struct sock *sock)
{
	char buf[256];
	int len = sock_error(sock, buf, sizeof(buf));
	if (len > 0) {
		retry(write(STDERR_FILENO, buf, len));
	}
}

