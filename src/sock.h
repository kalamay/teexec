#ifndef TEEXEC_SOCK_H
#define TEEXEC_SOCK_H

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/un.h>

#define SOCK_ERROR_NONE    0
#define SOCK_ERROR_SYS     1
#define SOCK_ERROR_SOCKET  2
#define SOCK_ERROR_CONNECT 3
#define SOCK_ERROR_BIND    4
#define SOCK_ERROR_LISTEN  5
#define SOCK_ERROR_ACCEPT  6
#define SOCK_ERROR_SETOPT  7
#define SOCK_ERROR_GAI     8

union addr {
	struct sockaddr sa;
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
	struct sockaddr_un un;
	struct sockaddr_storage ss;
};

struct sock {
	int fd;
	short error;
	unsigned char errortype;
	bool passive;
	union addr addr;
};

struct sockopt {
	/* Options applied by sock_open, sock_accept, and sock_setopt */
	unsigned sndbuf;   /* Socket send buffer size. */
	unsigned rcvbuf;   /* Socket receive buffer size. */
	bool reuseaddr;    /* Allow reuse of local addresses. */
	bool reuseport;    /* Allow multiple binds to the same address and port. */
	bool nodelay;      /* Disable TCP Nagle algorithm. */
	bool defer_accept; /* Awaken listener only when data arrives on the socket. */
	bool keepalive;    /* Send TCP keep alive probes. */

	/* Options applied by sock_open and sock_accept */
	bool cloexec;      /* Set the close-on-exec flag. */
	bool nonblock;     /* Put in non-blocking mode. */

	/* Options applied by sock_open. */
	bool passive;      /* Open a passive (listening) socket. */
	short backlog;     /* Connection backlog size. */
	int type;          /* Socket type: SOCK_STREAM, SOCK_DGRAM, etc. */
	int family;        /* Address familty: AF_UNSPEC, AF_INET4, AF_INET6. */
};

bool
sock_open(struct sock *sock, const struct sockopt *opt, const char *net);

bool
sock_accept(struct sock *sock, const struct sockopt *opt, int fd);

void
sock_close(struct sock *sock);


int
sock_error(const struct sock *sock, char *buf, size_t buflen);

void
sock_perror(const struct sock *sock);

bool
sock_set_inet(struct sock *sock, const struct sockopt *opt, const char *host, const char *serv);

bool
sock_set_un(struct sock *sock, const struct sockopt *opt, const char *path);

bool
sock_set_fd(struct sock *sock, const struct sockopt *opt, int fd);


bool
sock_setopt(int fd, const struct sockopt *opt);

bool
sock_nonblock(int fd, bool on);

bool
sock_cloexec(int fd, bool on);


const char *
addr_encode(const struct sockaddr *addr);

bool
addr_equal(const struct sockaddr *a, const struct sockaddr *b);


#define SOCKOPT(_type, _passive) ((struct sockopt) { \
	.sndbuf = 0, \
	.rcvbuf = 0, \
	.reuseaddr = true, \
	.reuseport = false, \
	.nodelay = false, \
	.defer_accept = false, \
	.keepalive = false, \
	.cloexec = true, \
	.nonblock = false, \
	.passive = (_passive), \
	.backlog = 1024, \
	.type = (_type), \
	.family = AF_UNSPEC \
})

#define SOCKOPT_STREAM SOCKOPT(SOCK_STREAM, false)
#define SOCKOPT_STREAM_PASSIVE SOCKOPT(SOCK_STREAM, true)

#endif

