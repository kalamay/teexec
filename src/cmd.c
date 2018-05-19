#include "cmd.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <err.h>

#define IS_REQ(opt)    ((opt)->var[0] != '?')
#define HAS_VAR(opt)   ((opt)->var != NULL)
#define HAS_SHORT(opt) (isprint((opt)->key) && (!HAS_VAR(opt) || IS_REQ(opt)))

#define CRST "\033[0m"
#define CHDR "\033[1;34m"
#define CEXE "\033[32m"
#define CARG "\033[1m"
#define CVAR "\033[4;33m"

#define HDR(s) CHDR s ":" CRST
#define EXE(s) CEXE s CRST
#define ARG(s) CARG s CRST
#define VAR(s) CVAR s CRST

static void
conv(struct option *dst, const struct opt *src)
{
	dst->name = src->name;
	dst->has_arg = HAS_VAR(src) ?
		(IS_REQ(src) ? required_argument : optional_argument) : no_argument;
	dst->flag = NULL;
	dst->val = src->key;
}

void
cmd_usage(const struct cmd *cmd)
{
	fprintf(stderr, HDR("usage") " " EXE("%s"), cmd->name);

	bool first = true;
	for (const struct opt *o = cmd->opts; o->name; o++) {
		if (!HAS_VAR(o) && HAS_SHORT(o)) {
			if (first) {
				fprintf(stderr, " [-");
				first = false;
			}
			fprintf(stderr, "%c", o->key);
		}
	}
	if (!first) {
		fputc(']', stderr);
	}

	for (const struct opt *o = cmd->opts; o->name; o++) {
		if (!HAS_VAR(o)) {
			if (!HAS_SHORT(o)) { fprintf(stderr, " [" ARG("--%s") "]", o->name); }
		}
		else {
			if (HAS_SHORT(o))  { fprintf(stderr, " [" ARG("-%c"), o->key); }
			else               { fprintf(stderr, " [" ARG("--%s="), o->name); }
			if (IS_REQ(o))     { fprintf(stderr, VAR("%s") "]", o->var); }
			else               { fprintf(stderr, "[" VAR("%s") "]]", o->var+1); }
		}
	}

	if (cmd->postargs) {
		fprintf(stderr, " -- %s", cmd->postargs);
	}

	fputc('\n', stderr);
}

static size_t
opt_len(const struct opt *o)
{
	size_t len = strlen(o->name);
	if (HAS_VAR(o)) {
		/* Account for the '=' character followed by the variable. */
		len += strlen(o->var) + 1;
		if (!IS_REQ(o)) {
			/* Account for skipping the '?' and adding '[' and ']'. */
			len++;
		}
	}
	return len;
}

void
cmd_help(const struct cmd *cmd)
{
	flockfile(stderr);
	cmd_usage(cmd);

	if (cmd->about) {
		fprintf(stderr, HDR("about") "\n  %s\n", cmd->about);
	}

	if (cmd->opts->name) {
		fprintf(stderr, "\n" HDR("options") "\n");

		size_t max = 0;
		for (const struct opt *o = cmd->opts; o->name; o++) {
			size_t len = opt_len(o);
			if (len > max) { max = len; }
		}

		for (const struct opt *o = cmd->opts; o->name; o++) {
			if (HAS_SHORT(o)) { fprintf(stderr, "  " ARG("-%c") ",", o->key); }
			else              { fprintf(stderr, "     "); }
			fprintf(stderr, ARG("--%s"), o->name);
			if (HAS_VAR(o)) {
				if (IS_REQ(o)) { fprintf(stderr, "=" VAR("%s"), o->var); }
				else           { fprintf(stderr, "=[" VAR("%s") "]", o->var+1); }
			}
			for (size_t n = opt_len(o); n < max; n++) { fputc(' ', stderr); }
			fprintf(stderr, "  %s\n", o->usage);
		}
	}

	if (cmd->extra) {
		fprintf(stderr, "\n%s\n", cmd->extra);
	}
	funlockfile(stderr);
}

int
cmd_getopt(int argc, char *const *argv, const struct cmd *cmd)
{
	static struct option copy[256];
	static const struct opt *set = NULL;
	static char optshort[countof(copy)*2 + 2];
	static int count = 0, help = -1, helpch = 0;

	const struct opt *o = cmd->opts;

	if (set != o) {
		char *p = optshort;
		*p++ = ':';

		set = o;
		count = 0;
		help = -1;
		helpch = 'h';

		for (; o->name; count++, o++) {
			if (count == (int)countof(copy)) { errx(1, "too many options"); }
			conv(&copy[count], o);
			if (HAS_SHORT(o)) {
				*p++ = o->key;
				if (HAS_VAR(o)) { *p++ = ':'; }
			}
			if (strcmp(o->name, "help") == 0) {
				help = count;
				helpch = o->key;
			}
			else if (o->key == 'h') {
				helpch = 0;
			}
		}

		if (help < 0 && count < (int)countof(copy)) {
			copy[count].name = "help";
			copy[count].has_arg = no_argument;
			copy[count].flag = NULL;
			copy[count].val = helpch;
			*p++ = copy[count].val;
			help = count++;
		}

		*p++ = '\0';
		memset(&copy[count], 0, sizeof(copy[count]));
	}

	int idx = 0;
	int ch = getopt_long(argc, argv, optshort, copy, &idx);
	if (ch == helpch && (ch || idx == help)) {
		cmd_help(cmd);
		exit(0);
	}
	optcur = &set[idx];
	switch (ch) {
	case '?': errx(1, "invalid option: %s", argv[optind-1]);
	case ':': errx(1, "missing argument for option: %s", argv[optind-1]);
	}
	return ch;
}

