#ifndef TEEXEC_ADVICE_H
#define TEEXEC_ADVICE_H

void before_close(int fd);
void after_close(int rc, int fd);

void
after_accept(int rc,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen);

#if HAS_ACCEPT4 || HAS_SYS_ACCEPT4
void
after_accept4(int rc,
		int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
#endif

void
after_read(ssize_t rc,
		int fd, void *buf, size_t count);

#if HAS_READ_CHK
void
after___read_chk(ssize_t rc,
		int fd, void *buf, size_t nbytes, size_t buflen);
#endif

void
after_readv(ssize_t rc,
		int fd, const struct iovec *iov, int iovcnt);

void
after_recvfrom(ssize_t rc,
		int sockfd, void *buf, size_t len, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen);

void
after_recv(ssize_t rc, int sockfd, void *buf, size_t len, int flags);

#if HAS_RECV_CHK
void
after___recv_chk(ssize_t rc, int sockfd, void *buf, size_t len, size_t buflen, int flags);
#endif

#if HAS_RECVFROM_CHK
void
after___recvfrom_chk(ssize_t rc, int sockfd, void *buf, size_t len, size_t buflen, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen);
#endif

void
after_recvmsg(ssize_t rc,
		int sockfd, struct msghdr *msg, int flags);

#if HAS_RECVMMSG
void
after_recvmmsg(int rc, int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
		int flags, struct timespec *timeout);
#endif

#endif

