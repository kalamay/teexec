#ifndef TEEXEC_CMD_H
#define TEEXEC_CMD_H

struct opt {
	char key;
	char *name;
	char *var;
	char *usage;
};

struct cmd {
	const char *name;
	const struct opt *opts;
	const char *postargs;
	const char *about;
	const char *extra;
};

const struct opt *optcur;

void
cmd_usage(const struct cmd *cmd);

void
cmd_help(const struct cmd *cmd);

int
cmd_getopt(int argc, char *const *argv, const struct cmd *cmd);

#endif

