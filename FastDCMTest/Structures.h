//  Structures.h

struct string
{
unsigned short perms;				//	Number of distinct permutations
unsigned short waste;				//	Number of wasted digits
unsigned short lastN1;				//	Integer encoding of last n-1 digits in string, prior to digits[pos].
unsigned short pos;					//	Current position in string.
unsigned short rootDepth;			//	Number of digits treated as fixed
unsigned char digits[MSL];			//	Digits of string are in bits 0-2
} __attribute__ ((aligned (4)));	//	This seems to be necessary, beyond the usual alignment on 2-byte boundaries C guarantees

//	Bit masks for the pieces of data in each digits[] entry

#define DIGIT_BITS (0x07)
#define PERMUTATION_BITS (0xF8)
#define PERMUTATION_LSB (0x08)

struct maxPermsLoc
{
unsigned int where;
unsigned int perms;
};

struct stringStatus
{
unsigned int unfinished;
unsigned int prefixSum;
};

