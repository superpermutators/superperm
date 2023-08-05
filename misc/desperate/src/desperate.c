#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#define EXECNAME desperate
#include "env.h"


struct {
	int numbered;
} flags = {0};

/* on the stack because I love you */
unsigned char character_set[256], *csetp;
unsigned char set_length;
FILE *fp;


/*
size_t strnspn(const char *s1, const char *s2, const size_t n)
{
	size_t i, accept_length;
	unsigned char table[256], *p, *s;

	if (n == 0)
		return 0;

	accept_length = strlen(s2);

	if (__glibc_unlikely(accept_length == 1)) {
		s = (char *)s1;
		for (i = 0; (i < n) && (*s++ != *s2); i++);
		return i;
	}

	p = memset(table, 0, 64);
	memset(p + 64, 0, 64);
	memset(p + 128, 0, 64);
	memset(p + 192, 0, 64);

	s = (unsigned char *)s2;
	for (i = 0; i < accept_length; i++)
		p[*s++] = 1;

	s = (unsigned char *)s1;
	for (i = 0; (i < n) && p[*s]; i++);
	return i;
}
*/


int desperate()
{
	int c;
	unsigned char start, i;
	size_t position;

	unsigned char buffer[set_length];

	memset(buffer, 0, set_length);
	start = i = 0;
	position = 0;

	/* find spans which are set_length in ...length. */
	while ((c = fgetc(fp)) != EOF) {
		if (!c || (c == 0x000A)) {
			/* null byte or LF, reset everything. */
			while (start != i) {
				csetp[buffer[start++]] = 1;
				if (start == set_length)
					start = 0;
			}
			start = i = 0;
			position = 0;
			continue;
		}

		c = (unsigned char)c;

		buffer[i++] = c;
		if (i == set_length)
			i = 0;

		if (csetp[c] == 0) {
			/* duplicate, unput characters up to c */
			while (buffer[start] != c) {
				csetp[buffer[start++]] = 1;
				if (start == set_length)
					start = 0;

				position++;
			}
			goto shift;
		} else if (i == start) {
			unsigned char sp = start;

			/* found one, print it. */
			if (flags.numbered)
				printf("%ld,", position);

			do {
				fputc(buffer[sp++], stdout);
				if (sp == set_length)
					sp = 0;
			} while (sp != i);
			fputc('\n', stdout);

			/* unput the first character */
			csetp[buffer[start]] = 1;
shift:
			if (++start == set_length)
				start = 0;

			position++;
		}

		csetp[c] = 0;
	}

	return 0;
}


// XXX: implement multi-byte characters if we want more than 253 elements.
void usage()
{
	puts("\
Usage: " S(EXECNAME) " [options...] SET [FILE]\n\
Given the string SET, find and display all permutations within the\n\
superpermutation(s) in FILE matching the characters in SET.\n\
FILE may be explicitly set to '-' for STDIN, which is the default value.\n\
\n\
To process multiple superpermutations in one execution, separate them with\n\
the NULL byte, \\x00, or the Line Feed character, \\x0A. The special\n\
characters \\x00 and \\x0A must therefore not be used in SET. SET must be\n\
given as raw bytes; escaped sequences will not be interpreted.\n\
\n\
Options:\n\
  -h, --help           Show this message.\n\
      --numbered       Also display the position of each permutation.\n\
");
}


int invalid_sset(const char *sset)
{
	if (sset == NULL)
		return 1;

	if (strlen(sset) == 0)
		return 1;

	if (strchr(sset, 0x000A))
		return 1;

	return 0;
}


int parse_args(int argc, char * const *argv, char **sset, FILE **fp_in)
{
	int opt;
	char *sp;

	const char *optstring = "h";
	const struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "numbered", no_argument, NULL, 1 },
		{ 0 }
	};

	*sset = NULL;
	*fp_in = NULL;

	while ((opt = getopt_long(argc, argv, optstring, longopts, NULL)) \
	       != -1) {
		switch(opt) {
		case 'h':
			usage();
			return -1; // exit 0
		case 1:
			flags.numbered = 1;
			break;
		case '?':
			__attribute__((fallthrough));
		default:
			if ((sp = strchr(optstring, optopt)) != NULL) {
				PRINT_ERROR("option '%c' requires an " \
					    "argument\n", optopt);
			} else {
				PRINT_ERROR("unknown option '%c'\n", optopt);
			}
			usage();
			return -3; // exit 2
		}
	}

	for (; optind < argc; optind++) {
		if (*sset == NULL) {
			*sset = argv[optind];
		} else if (*fp_in == NULL) {
			if (strcmp(argv[optind], "-") == 0) {
				*fp_in = stdin;
			} else {
				*fp_in = fopen(argv[optind], "r");
			}

			if (*fp_in == NULL) {
				PRINT_ERROR("failed to open file: %s\n",
					    strerror(errno));
				return -2; // exit 1
			}
		} else {
			PRINT_ERROR("no candidate for positional argument " \
				    "'%s'\n", argv[optind]);
			usage();
			return -3; // exit 2
		}
	}

	if (invalid_sset(*sset)) {
		PRINT_ERROR("a valid set of characters must be provided.\n");
		usage();
		return -3; // exit 2
	}

	if (*fp_in == NULL)
		*fp_in = stdin;

	return 0;
}


void exit_hndl()
{
	if (fp && (fp != stdin))
		fclose(fp);
}


int init(int argc, char * const *argv)
{
	int ret;
	char *sset;

	if ((ret = parse_args(argc, argv, &sset, &fp)) < 0)
		return ret;

	/* stolen from strchr */
	csetp = memset(character_set, 0, 64);
	memset(csetp + 64, 0, 64);
	memset(csetp + 128, 0, 64);
	memset(csetp + 192, 0, 64);

	set_length = 0;
	while (*sset) {
		csetp[(unsigned char)*sset++] = 1;
		set_length++;
	}

	return 0;
}


int main(int argc, char *argv[])
{
	int ret;

	if (atexit(exit_hndl)) {
		PRINT_ERROR("initialization failure.\n");
		return 1;
	}

	if ((ret = init(argc, argv)) < 0)
		return ~ret;

	return desperate();
}
