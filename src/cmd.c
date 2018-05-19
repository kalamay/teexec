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

static const struct opt *optset = NULL;
static struct option optlong[256];
static char optshort[countof(optlong)*2 + 2];
static int optcount = 0, opthelp = -1, opthelpch = 0;

static void
conv(struct option *dst, const struct opt *src)
{
	dst->name = src->name;
	dst->has_arg = HAS_VAR(src) ?
		(IS_REQ(src) ? required_argument : optional_argument) : no_argument;
	dst->flag = NULL;
	dst->val = src->key;
}

static void
opt_load(const struct opt *o)
{
	if (optset == o) { return; }

	char *p = optshort;
	*p++ = ':';

	optset = o;
	optcount = 0;
	opthelp = -1;
	opthelpch = 'h';

	for (; o->name; optcount++, o++) {
		if (optcount == (int)countof(optlong)) {
			errx(1, "too many options");
		}
		conv(&optlong[optcount], o);
		if (HAS_SHORT(o)) {
			*p++ = o->key;
			if (HAS_VAR(o)) { *p++ = ':'; }
		}
		if (strcmp(o->name, "help") == 0) {
			opthelp = optcount;
			opthelpch = o->key;
		}
		else if (o->key == 'h') {
			opthelpch = 0;
		}
	}

	if (opthelp < 0 && optcount < (int)countof(optlong)) {
		optlong[optcount].name = "help";
		optlong[optcount].has_arg = no_argument;
		optlong[optcount].flag = NULL;
		optlong[optcount].val = opthelpch;
		*p++ = optlong[optcount].val;
		opthelp = optcount++;
	}

	*p++ = '\0';
	memset(&optlong[optcount], 0, sizeof(optlong[optcount]));
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

static size_t
opt_max(const struct opt *o)
{
	size_t max = 0;
	for (; o->name; o++) {
		size_t len = opt_len(o);
		if (len > max) { max = len; }
	}
	return max;
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

		size_t max = opt_max(cmd->opts);
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
	opt_load(cmd->opts);

	int idx = 0;
	int ch = getopt_long(argc, argv, optshort, optlong, &idx);
	if (ch == opthelpch && (ch || idx == opthelp)) {
		cmd_help(cmd);
		exit(0);
	}
	optcur = &optset[idx];
	switch (ch) {
	case '?': errx(1, "invalid option: %s", argv[optind-1]);
	case ':': errx(1, "missing argument for option: %s", argv[optind-1]);
	}
	return ch;
}

