
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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

/** *plist -> list of found permutations
	size -> how many there are
	radix -> the radix thing
	*buffer -> line buffer for file
	length -> how long the line buffer is
**/
char* formatplist(struct permutable *plist, const int size, const uint8_t radix, char* buffer, const int length) {
    /*	variables
		index -> which line of permutations are we on? (this is useful due to how this formats things)
		i -> temporary index of *plist
		a -> offset between numbers
		c -> used to determine buffer offset
		ptr -> current buffer pointer, used to poke things into the buffer.
    */
	int i, currptr, lineptr = 0;
	uint8_t index;
	/* fill the buffer with spaces */
	for (i = 0; i < length-1; i++) {
		buffer[i] = ' ';
	} buffer[i] = 0;
	printf("%d\n", size);

    for (index = radix; index > 0; index--) {
		for (i = index - 1; i <= size; i += radix) { /* flip through *plist at specific intervals */
			currptr = plist[i].offset + lineptr;
			printf("%d\t", i);

			if ((currptr + radix) >= length) { /* if there isn't room in the buffer, quit. */
				if ((plist[i].offset + 1) >= length) { /* how did you manage that?? */
					puts("Something happened that should've been impossible..");
					return NULL;
				} else if ((plist[i].offset + 2) >= length) {
					buffer[plist[i].offset + 1] = 0;
					puts("No space for newline...");
					return buffer;
				}
				puts("NO MORE ROOM!");
				break;
			}

			strncpy(buffer + currptr, plist[i].permutation, radix);
		}
		puts("");
		lineptr = currptr + radix;
		buffer[lineptr++] = '\n';
	}
	puts("Finished with this one.");
	return buffer;
}

/**	*super -> string with superperm in it
	size -> how long it is
	radix -> the radix thing
	*output_file -> output file
**/
void demutator(const char* super, const int size, const uint8_t radix, FILE* output_file) {
    /*	load up some constants
		BM -> bitmask
		BUFF_LEN -> *buffer length
    */
	const int BUFF_LEN = size*radix + 1;
	const uint64_t BM = (1 << radix) - 1;
	/*	variables
		c -> temporary character
		*buffer -> line buffer (for output file) should be bigger than we need.
		base -> base pointer for *super
		offset -> offset for *super + base
		cbm -> changeable bitmask
		bit_cbm -> temporary buffer for selecting bit from cbm
		*plist -> data structure which holds an offset and the permutation starting at the offset
	*/
	int index = -1, ERR = 0, aug_size = (size+1)-radix;
	char c, *buffer = (char*)malloc(BUFF_LEN); buffer[BUFF_LEN - 1] = 0;
	register int base = 0, offset = 0;
	register uint64_t cbm = 0, bit_cbm = 0;
	struct permutable *plist = (struct permutable*)malloc(aug_size * sizeof(struct permutable));

	/* go through *super and search for all permutations */
	for (base = 0; base <= aug_size; base++) { /* stops short of buffer length */
		cbm = BM; /* setup flags */

		for (offset = 0; offset < radix; offset++) { /* find a permutation */
			c = super[base + offset];
			/* make sure c is a numeral between 1 and the radix */
			if ((c < '1')||(c > ('0' + radix))) { ERR = -1; break; }

			/* check to make sure this digit wasn't already found */
			bit_cbm = (1 << (c - '1'));
			if (((cbm ^= bit_cbm)&bit_cbm) || (c == 0)) break;
		}

		if (ERR) break;
		if (cbm == 0) { /* if bitmask is cleared, add permutation to *plist */
			if (++index == aug_size) {
				puts("PERMUTATION ARRAY FILLED");
				break; /* exit if we've filled up the buffer */
			}
			plist[index] = (struct permutable){ base,  strncpy((char*)calloc(radix + 1, 1), super + base, radix) };
		}
		if ((c == 0) || ((base + offset) == size)) break; /* if null byte or end, exit. */
	}
	if (ERR) {
        /* if there was an error, print the offset of *super where it occured */
        printf("ERROR @ index: %d  size: %d  base: %d offset: %d\n", index, size, base, offset);
		fprintf(output_file, "\n\nERROR @ index: %d  size: %d  base: %d  offset: %d\n\n", index, size, base, offset);
	} else {
		/* format the *plist into whatever and concat into the output file */
		fprintf(output_file, "%s\n", formatplist(plist, index, radix, buffer, BUFF_LEN));
	}

    /* free all structures */
	for (; index >= 0; index--) free(plist[index].permutation);
	free(buffer);
	free(plist);
}

int main() {
	FILE *pfile = NULL, *outfile = NULL;
	char *linebuffer, *filebuffer;
	uint8_t radix;
	unsigned short temp;
	int length, top, size;
	register int pointer = 0, y = 0;

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
	fscanf(pfile, "%hu", &temp);
	radix = temp;
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
	while (sscanf((char*)filebuffer + pointer, "%s", linebuffer) != EOF) {
		y = strlen(linebuffer);
		demutator(linebuffer, y++, radix, outfile);
		fprintf(outfile, "%s\n\n", linebuffer);
		pointer += y;
	}

	/* cleanup */
	free(filebuffer);
	free(linebuffer);

	fclose(pfile);
	fclose(outfile);
	puts("\ndone.");
	return 0;
}
