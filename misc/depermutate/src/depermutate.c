#include <config.h>

#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#define EXECNAME demutate
#include "global.h"


struct {
	int show_position: 1;
} flags = {0};


void usage()
{
	puts("\
Usage: " S(EXECNAME) " [options...] [FILE]\n\
Find all permutations in each superpermutation from FILE.\n\
Give '-' for STDIN. FILE defaults to STDIN.\n\
\n\
Options:\n\
  -h, --help           Show this message.\n\
      --numbered       Display position of permutation in string.\n\
");
}


int parse_args(int argc, char * const *argv, FILE **fp_in)
{
	int opt;
	char *sp;

	const char *optstring = "h";
	const struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "numbered", no_argument, NULL, 1 },
		{ 0 }
	};

	*fp_in = NULL;

	while ((opt = getopt_long(argc, argv, optstring, longopts, NULL)) \
	       != -1) {
		switch(opt) {
		case 'h':
			usage();
			return -1; // exit 0
		case 1:
			flags.show_position = 1;
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
		if (*fp_in == NULL) {
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

	if (*fp_in == NULL)
		*fp_in = stdin;

	return 0;
}


typedef struct jobinfo {
	FILE *fp;
	size_t span;
	size_t length;
} *jobinfo_t;


jobinfo_t new_jobinfo()
{
	jobinfo_t jobinfo;

	if ((jobinfo = calloc(1, sizeof(struct jobinfo))) == NULL)
		return NULL;

	if ((jobinfo->fp = tmpfile()) == NULL) {
		free(jobinfo);
		return NULL;
	}

	return jobinfo;
}


void free_jobinfo(jobinfo_t jobinfo)
{
	if (jobinfo == NULL)
		return;

	fclose(jobinfo->fp);
	free(jobinfo);
}


jobinfo_t get_jobinfo(FILE *fp)
{
	int c;
	jobinfo_t jobinfo;
	unsigned char table[256], *p;

	p = memset(table, 0, 64);
	memset(p + 64, 0, 64);
	memset(p + 128, 0, 64);
	memset(p + 192, 0, 64);

	if ((jobinfo = new_jobinfo()) == NULL) {
		PRINT_ERROR("failed to create jobinfo: %s\n",
			    strerror(errno));
		return NULL;
	}

retry:
	while ((c = fgetc(fp)) != EOF) {
		if (c == '\n')
			break;

		if (fputc(c, jobinfo->fp) == EOF) {
			if (ferror(jobinfo->fp)) {
				PRINT_ERROR("failed writing to file: %s\n",
					    strerror(errno));
				free_jobinfo(jobinfo);
				return NULL;
			}

			break;
		}

		jobinfo->length++;

		if (table[(unsigned char)c] == 0) {
			jobinfo->span++;
			table[(unsigned char)c] = 1;
		}
	}

	if (jobinfo->length == 0) {
		if (c == EOF) {
			/* end of file */
			free_jobinfo(jobinfo);
			return NULL;
		}

		goto retry;
	}

	if (ferror(fp)) {
		PRINT_ERROR("failed reading from file: %s\n",
			    strerror(errno));
		free_jobinfo(jobinfo);
		return NULL;
	}

	rewind(jobinfo->fp);

	return jobinfo;
}


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

void frepputc(char c, size_t n, FILE *fp)
{
	for (; n; n--)
		fputc(c, fp);
}
*/

int main(int argc, char *argv[])
{
	int ret;
	FILE *fp;
	jobinfo_t jobinfo;

	if ((ret = parse_args(argc, argv, &fp)) < 0)
		return ~ret;

	while ((jobinfo = get_jobinfo(fp)) != NULL) {
		int c;
		size_t span, position = 0;
		fpos_t next;
		char buffer[jobinfo->span + 1], *b;
		unsigned char table[256], *p;

		do {
			fgetpos(jobinfo->fp, &next);

			b = memset(buffer, 0, jobinfo->span + 1);

			p = memset(table, 0, 64);
			memset(p + 64, 0, 64);
			memset(p + 128, 0, 64);
			memset(p + 192, 0, 64);

			for (span = jobinfo->span; span; span--) {
				c = fgetc(jobinfo->fp);
				if (c == EOF)
					break;

				*b++ = c;

				if (p[(unsigned char)c])
					break;
				p[c] = 1;
			}

			if (span == 0) {
				// print permutation
				if (flags.show_position)
					printf("%ld,", position);

				puts(buffer);
			}

			fsetpos(jobinfo->fp, &next);

			position++;
		} while ((c = fgetc(jobinfo->fp)) != EOF);

		free_jobinfo(jobinfo);
	}

	if (fp != stdin)
		fclose(fp);

	return 0;
}
