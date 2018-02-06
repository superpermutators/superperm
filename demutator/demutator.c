/* "EW, C!" */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct permutable { /* super-important structure */;
	int offset;
	char* permutation;
};

int factorial(int x) {
	if (x == 1)
		return 1;
	else
		return x * factorial(x - 1);
}

char* formatplist(struct permutable *plist, int size, int radix, char* buffer, int length) {
    /* variables
     index -> which line of permutations are we on? (this is useful due to how this formats things)
     i -> temporary index of *plist
     a -> offset, used to make sure we print the correct # of spaces
     c -> used to determine buffer offset, more like a "whole length" offset rather than a per-number offset
     ptr -> current buffer pointer, used to poke things into the buffer.
    */
	int index = radix, i, a, c , ptr = 0;

nachste: /* goto-ing is going on */
    c = ptr; /* save last secondary offset */
	for (i = index - 1; i < size; i += radix) { /* flip through *plist at specific intervals */
		for (a = (plist[i].offset + c) - ptr; a > 0; a--) {
			buffer[ptr++] = ' ';
			/* if we somehow iterate the pointer to a value that's not able to be buffered given its offset, quit */
			if (ptr >= length) return NULL;
		}
		ptr += radix; /* step ahead (radix) spaces to make room for the permutation we're going to add */
		if (ptr >= length) return NULL; /* if there isn't room in the buffer, quit. */
		strncpy(buffer + (ptr - radix), plist[i].permutation, radix);
	}
	buffer[ptr++] = '\n'; /* tack on newline */
	if (ptr >= length) return NULL; /* if there isn't enough space for anything else, quit. */
	if (--index) goto nachste; /* goto-ing commence! (until index is 0) */
	buffer[ptr - 1] = 0; /* tack on zero byte to mark end of string */
	return buffer;
}

void demutator(const char* super, const int size, const int radix, FILE* output_file) {
    /* load up some constants
     bm -> bitmask
     len -> *buffer length
    */
	const int bm = (1 << radix) - 1, len = size*radix*3;
	/* variables
	 c -> temporary character
	 *buffer -> line buffer (for output file)
	 cbm -> changeable bitmask
	 base -> base pointer for *super
	 offset -> offset for *super + base
	 mag -> temporary buffer for selecting bit from cbm
	 *plist -> data structure which holds an offset and the permutation starting at the offset
	*/
	int index = -1;
	char c, *buffer = (char*)malloc(len);
	register int cbm = 0, base = 0, offset = 0, mag = 0;
	struct permutable *plist = (struct permutable*)malloc(size * sizeof(struct permutable));

	/* go through *super and search for all permutations */
	for (base = 0; base <= size*radix - radix; base++) {
		cbm = bm;
		for (offset = 0; offset < radix; offset++) {
			c = super[base + offset];
			/* make sure c is a numeral between 1 and the radix */
			if ((c < '1')||(c > ('0' + radix))) break;
			mag = (1 << (c - '1'));
			if (((cbm ^= mag)&mag) || (c == 0)) break; /* check to make sure this digit wasn't already found */
		}
		if (c == 0) break; /* if null byte, exit. */
		if (cbm == 0) { /* if bitmask is cleared, add permutation to *plist */
			plist[++index] = (struct permutable){ base,  strncpy((char*)calloc(radix + 1, 1), super + base, radix) };
		}
	}
	/* format the *plist into whatever and concat into the output file */
	if (index == size - 1)
		fprintf(output_file, "%s\n", formatplist(plist, size, radix, buffer, len));
	else
        /* if there was an error, print the offset of *super where it occured */
		fprintf(output_file, "\n\n%d\n\n", index);

    /* free all structures */
	for (; index >= 0; index--) free(plist[index].permutation);
	free(buffer);
	free(plist);
}

int main() {
	FILE *pfile = NULL, *outfile = NULL;
	char *linebuffer, *filebuffer;
	int length, top, radix, size; /* based on machine, radix may be restricted to 8 or 16 (int size) */
	register int a = 0, y = 0;

	/* Open data file */
	pfile = fopen("data.txt", "r");

	if (pfile == NULL) {
		puts("No file");
		return -1;
	}

	outfile = fopen("out.txt", "w");

	if (outfile == NULL) {
		puts("Error making output file");
		return -2;
	}

	/* first number contains radix */
	fscanf(pfile, "%d", &radix);
	size = factorial(radix);
	/* offset in bytes from first number */
	top = ftell(pfile) + 2;
	/* constant bitmask for numeral checking */

	/* calculate (radix factorial) * radix for maximum buffer size */
	linebuffer = (char*)calloc((size * radix) + 1, 1);

	/* go to end of buffer */
	while (feof(pfile) == 0)
		fgetc(pfile);

	/* difference between the end and the offset */
	length = ftell(pfile) - top;
	/* reset the file pointer to after the first number */
	fseek(pfile, top, SEEK_SET);

	/* setup virtual file and copy from disk */
	filebuffer = (char*)calloc(length, 1);
	for (y = 0; y < length; y++) {
		if ((filebuffer[y] = fgetc(pfile)) < 0)
			filebuffer[y] = 0;
	}

	/* split buffer into lines and calculate permutations */
	while (sscanf((char*)filebuffer + a, "%s", linebuffer) != EOF) {
		demutator(linebuffer, size, radix, outfile);
		fprintf(outfile, "%s\n\n", linebuffer);
		a += strlen(linebuffer) + 1;
	}

	/* cleanup */
	free(filebuffer);
	free(linebuffer);

	fclose(pfile);
	fclose(outfile);
	puts("\ndone.");
	return 0;
}
