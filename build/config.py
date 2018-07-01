#!/usr/bin/env python

import os
from subprocess import Popen, PIPE, check_output
from tempfile import NamedTemporaryFile

CC = ['cc', '-D_GNU_SOURCE', '-O', '-Werror', '-Wno-unused-result',
		'-ldl', '-x', 'c', '-', '-o', '/dev/stdout']
DEVNULL = open(os.devnull, 'w')

def print_flag(name, val="1"):
	print("""#ifndef HAS_%s
# define HAS_%s %s
#endif""" % (name, name, val))

def compiles(c):
	p = Popen(CC, stdin=PIPE, stdout=DEVNULL, stderr=DEVNULL)
	p.communicate(input=c)
	return p.returncode == 0

def capture(c):
	with NamedTemporaryFile(mode='w+b') as f:
		p = Popen(CC, stdin=PIPE, stdout=f, stderr=DEVNULL)
		p.communicate(input=c)
		if p.returncode == 0:
			f.file.close()
			os.chmod(f.name, 0700)
			return check_output([f.name, f.name])
		else:
			return None

def has_function(name, args, hdr):
	return compiles("""
		#include <%s>
		int main(void) { %s(%s); }
	""" % (hdr, name, ",".join(["0"] * args)))

def has_sock_flags():
	return compiles("""
		#include <sys/socket.h>
		int main(void) { socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0); }
	""")

def check_define(name, hdr, define):
	val = capture("""
		#include <stdio.h>
		#include <%s>
		int main(void) { printf("%%d", %s); }
	""" % (hdr, define))
	if val:
		print_flag(name, val)

def has_accept4():
	return has_function("accept4", 4, "sys/socket.h")

def has_tee():
	return has_function("tee", 4, "fcntl.h")

def has_splice():
	return has_function("splice", 6, "fcntl.h")

def has_recvmmsg():
	return has_function("recvmmsg", 5, "sys/socket.h")

def has_read_chk():
	return has_function("__read_chk", 4, "unistd.h")

def has_recv_chk():
	return has_function("__recv_chk", 5, "sys/socket.h")

def has_recvfrom_chk():
	return has_function("__recvfrom_chk", 7, "sys/socket.h")

if has_sock_flags():   print_flag("SOCK_FLAGS")
if has_accept4():      print_flag("ACCEPT4")
if has_tee():          print_flag("TEE")
if has_splice():       print_flag("SPLICE")
if has_recvmmsg():     print_flag("RECVMMSG")
if has_read_chk():     print_flag("READ_CHK")
if has_recv_chk():     print_flag("RECV_CHK")
if has_recvfrom_chk(): print_flag("RECVFROM_CHK")
check_define("SYS_ACCEPT4", "sys/syscall.h", "SYS_accept4")
check_define("PTRACE", "sys/ptrace.h", "PTRACE_GETREGSET")

