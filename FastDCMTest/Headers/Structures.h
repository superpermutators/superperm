//  Structures.h

#ifdef _WIN32
__declspec(align(4))
#endif
struct string
{
unsigned short perms;				//	Number of distinct permutations
unsigned short waste;				//	Number of wasted digits
unsigned short pos;					//	Current position in string.
unsigned short rootDepth;			//	Number of digits treated as fixed
unsigned short lastN1;				//	Integer encoding of last n-1 digits in string, prior to digits[pos].
unsigned char digits[MSL];			//	Digits of string are in bits 0-2; permutation counts in other bits
unsigned char oneCycleBins[NVAL+1];	//	The numbers of 1-cycles that have 0 ... N unvisited permutations
unsigned char marker;
}
#ifndef _WIN32
__attribute__ ((aligned (4)))		//	This seems to be necessary, beyond the usual alignment on 2-byte boundaries C guarantees
#endif
;

//	Bit masks for the pieces of data in each digits[] entry

#define PERMUTATION_SHIFT (3)
#define PERMUTATION_LSB (1<<PERMUTATION_SHIFT)
#define DIGIT_BITS (PERMUTATION_LSB-1)
#define PERMUTATION_BITS (~(DIGIT_BITS))

struct maxPermsLoc
{
unsigned int where;
unsigned int perms;
};

struct stringStatus
{
unsigned int keep;
unsigned int prefixSum;
};

