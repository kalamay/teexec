#include "kern.h"

#if HAS_PTRACE
# include <unistd.h>
# include <signal.h>
# include <sys/ptrace.h>
# include <sys/types.h>
# include <sys/user.h>
# include <sys/wait.h>
# include <sys/syscall.h>
# define sys_cast(T, v) ((T)v)
# ifdef __x86_64__
#  define sys_num(reg) (reg.orig_rax)
#  define sys_1(T, reg) sys_cast(T, reg.rdi)
#  define sys_2(T, reg) sys_cast(T, reg.rsi)
#  define sys_3(T, reg) sys_cast(T, reg.rdx)
#  define sys_4(T, reg) sys_cast(T, reg.r10)
#  define sys_5(T, reg) sys_cast(T, reg.r8)
#  define sys_6(T, reg) sys_cast(T, reg.r9)
# else
#  define sys_num(reg) (reg.orig_ax)
#  define sys_1(T, reg) sys_cast(T, reg.ebx)
#  define sys_2(T, reg) sys_cast(T, reg.ecx)
#  define sys_3(T, reg) sys_cast(T, reg.edx)
#  define sys_4(T, reg) sys_cast(T, reg.esi)
#  define sys_5(T, reg) sys_cast(T, reg.edi)
#  define sys_6(T, reg) sys_cast(T, reg.ebp)
# endif
#endif
#include <err.h>

#include "advice.h"

void
kern_run(const char *filename, char *const *argv, char *const *envp)
{
#if HAS_PTRACE
	pid_t child = fork();
	if (child < 0) {
		err(1, "fork failed");
	}

	if (child == 0) {
		ptrace(PTRACE_TRACEME);
		kill(getpid(), SIGSTOP);
		execve(filename, argv, envp);
		err(1, "failed to exec");
	}

	int status, syscall;
	struct user_regs_struct in, out;

	for (;;) {
		if (waitpid(child, &status, 0) < 0 || WIFEXITED(status)) {
			break;
		}

		ptrace(PTRACE_GETREGS, child, NULL, &in);
		switch (sys_num(in)) {
		case SYS_close:
			before_close(sys_1(int, in));
			break;
		}
		ptrace(PTRACE_SYSCALL, child, NULL, NULL);

		if (waitpid(child, &status, 0) < 0 || WIFEXITED(status)) {
			break;
		}

		ptrace(PTRACE_GETREGS, child, NULL, &out);
		switch (sys_num(in)) {
		case SYS_close:
			after_close(sys_num(out),
					sys_1(int, in));
			break;
		case SYS_accept:
			after_accept(sys_num(out),
					sys_1(int, in),
					sys_2(struct sockaddr *, in),
					sys_3(socklen_t *, in));
			break;
#	ifdef HAS_SYS_ACCEPT4
		case SYS_accept4:
			after_accept4(sys_num(out),
					sys_1(int, in),
					sys_2(struct sockaddr *, in),
					sys_3(socklen_t *, in),
					sys_4(int, in));
			break;
#	endif
		case SYS_read:
			after_read(sys_num(out),
					sys_1(int, in),
					sys_2(void *, in),
					sys_3(size_t, in));
			break;
		case SYS_readv:
			after_readv(sys_num(out),
					sys_1(int, in),
					sys_2(const struct iovec *, in),
					sys_3(int, in));
			break;
		case SYS_recvfrom:
			after_recvfrom(sys_num(out),
					sys_1(int, in),
					sys_2(void *, in),
					sys_3(size_t, in),
					sys_4(int, in),
					sys_5(struct sockaddr *, in),
					sys_6(socklen_t *, in));
			break;
		case SYS_recvmsg:
			after_recvmsg(sys_num(out),
					sys_1(int, in),
					sys_2(struct msghdr *, in),
					sys_3(int, in));
			break;
#	ifdef HAS_RECVMMSG
		case SYS_recvmmsg:
			after_recvmmsg(sys_num(out),
					sys_1(int, in),
					sys_2(struct mmsghdr *, in),
					sys_3(unsigned int, in),
					sys_4(int, in),
					sys_5(struct timespec *, in));
			break;
#	endif
		}
		ptrace(PTRACE_SYSCALL, child, NULL, NULL);
	}

#else
	(void)filename;
	(void)argv;
	(void)envp;
	errx(1, "ptrace not implemented");
#endif
}

