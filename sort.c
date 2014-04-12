/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "text.h"
#include "util.h"

struct keydef {
	unsigned start_column;
	unsigned end_column;
	unsigned start_char;
	unsigned end_char;
};

struct kdlist {
	struct keydef keydef;
	struct kdlist *next;
};

static struct kdlist *head = NULL;
static struct kdlist *curr = NULL;

static void addkeydef(char *);
static void freelist(void);
static int linecmp(const char **, const char **);
static char *next_nonblank(char *);
static char *next_blank(char *);
static int parse_keydef(struct keydef *, char *);
static char *columns(char *, const struct keydef *);

static bool rflag = false;
static bool uflag = false;
static bool nflag = false;

static struct linebuf linebuf = EMPTY_LINEBUF;

static void
usage(void)
{
	enprintf(2, "usage: %s [-nru] [-k def]... [file...]\n", argv0);
}

int
main(int argc, char *argv[])
{
	long i;
	FILE *fp;

	ARGBEGIN {
	case 'n':
		nflag = true;
		break;
	case 'r':
		rflag = true;
		break;
	case 'u':
		uflag = true;
		break;
	case 'k':
		addkeydef(EARGF(usage()));
		break;
	default:
		usage();
	} ARGEND;

	addkeydef("1");

	if(argc == 0) {
		getlines(stdin, &linebuf);
	} else for(; argc > 0; argc--, argv++) {
		if(!(fp = fopen(argv[0], "r"))) {
			enprintf(2, "fopen %s:", argv[0]);
			continue;
		}
		getlines(fp, &linebuf);
		fclose(fp);
	}
	qsort(linebuf.lines, linebuf.nlines, sizeof *linebuf.lines,
			(int (*)(const void *, const void *))linecmp);

	for(i = 0; i < linebuf.nlines; i++) {
		if(!uflag || i == 0 || linecmp((const char **)&linebuf.lines[i],
					(const char **)&linebuf.lines[i-1])) {
			fputs(linebuf.lines[i], stdout);
		}
	}

	freelist();
	return EXIT_SUCCESS;
}

static void
addkeydef(char *def)
{
	struct kdlist *node;

	node = malloc(sizeof(*node));
	if(!node)
		enprintf(2, "malloc:");
	if(!head)
		head = node;
	if(parse_keydef(&node->keydef, def))
		enprintf(2, "parse_keydef:");
	if(curr)
		curr->next = node;
	node->next = NULL;
	curr = node;
}

static void
freelist(void)
{
	struct kdlist *node;
	struct kdlist *tmp;

	for(node = head; node; node = tmp) {
		tmp = node->next;
		free(node);
	}
}

static int
linecmp(const char **a, const char **b)
{
	char *s1, *s2;
	int res = 0;
	struct kdlist *node;

	for(node = head; node && res == 0; node = node->next) {
		s1 = columns((char *)*a, &node->keydef);
		s2 = columns((char *)*b, &node->keydef);

		/* don't consider modifiers if it's the default key
		 * definition that was implicitly added */
		/* if -u is given, don't use default */
		if(uflag && !(node == head) && !node->next)
			res = 0;
		else if(!(node == head) && !node->next)
			res = strcmp(s1, s2);
		else if(nflag)
			res = strtoul(s1, 0, 10) - strtoul(s2, 0, 10);
		else
			res = strcmp(s1, s2);

		free(s1);
		free(s2);
	}
	return rflag ? -res : res;
}

static int
parse_keydef(struct keydef *kd, char *s)
{
	char *rest = s;
	kd->start_column = 1;
	kd->start_char = 1;
	/* 0 means end of line */
	kd->end_column = 0;
	kd->end_char = 0;

	kd->start_column = strtoul(rest, &rest, 10);
	if(!kd->start_column)
		enprintf(2, "starting column cannot be 0\n");
	if(*rest == '.')
		kd->start_char = strtoul(rest+1, &rest, 10);
	if(*rest == ',') {
		kd->end_column = strtoul(rest+1, &rest, 10);
		if(kd->end_column < kd->start_column)
			enprintf(2, ",%u is too small\n", kd->end_column);
	}
	if(*rest == '.')
		kd->end_char = strtoul(rest+1, &rest, 10);
	if(*rest != '\0')
		return -1;
	return 0;
}

static char *
next_nonblank(char *s)
{
	for(; *s && isblank(*s); s++);
	return s;
}

static char *
next_blank(char *s)
{
	for(; *s && !isblank(*s); s++);
	return s;
}

static char *
columns(char *line, const struct keydef *kd)
{
	char *rest;
	char *start, *end;
	unsigned i;
	for(rest = line, i = 0; i < kd->start_column; i++) {
		if(i != 0)
			rest = next_blank(rest);
		rest = next_nonblank(rest);
	}
	for(i = 1; i < kd->start_char && !isblank(*rest); i++, rest++);
	start = rest;

	if(kd->end_column) {
		for(rest = line, i = 0; i < kd->end_column; i++) {
			if(i != 0)
				rest = next_blank(rest);
			rest = next_nonblank(rest);
		}
		if(kd->end_char) {
			for(i = 1; i < kd->end_char && *rest && !isblank(*rest); i++, rest++);
		} else {
			rest = next_blank(rest);
		}
		end = rest;
	} else {
		end = rest + strlen(rest);
	}
	return strndup(start, end - start);
}
