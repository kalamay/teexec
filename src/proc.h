#ifndef TEEXEC_PROC_H
#define TEEXEC_PROC_H

#include <sys/types.h>

const char *
proc_path(void);

const char *
proc_find(const char *p, char buf[static 4096]);

#endif

