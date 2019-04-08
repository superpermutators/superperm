//
//  ChaffinMethod.c
//
//	A program based on Benjamin Chaffin's algorithm for finding minimal superpermutations.

/*

From the original version:
--------------------------

This script is used to show that minimal superpermutations on 5 symbols have 153 characters
(it can also compute the length of minimal superpermutations on 3 or 4 symbols).
More specifically, it computes the values in the table from the following blog post:
http://www.njohnston.ca/2014/08/all-minimal-superpermutations-on-five-symbols-have-been-found/

Author: Nathaniel Johnston (nathaniel@njohnston.ca), based on an algorithm by Benjamin Chaffin
Version: 1.00
Last Updated: Aug. 13, 2014

This version:
-------------

This version aspires to give a result for n=6 before the death of the sun,
but whether it can or not is yet to be confirmed.

Author: Greg Egan
Version: 2.7
Last Updated: 8 April 2019

Usage:

	ChaffinMethod n [oneExample] [noRepeats]

Computes strings (starting with 123...n) that contain the maximum possible number of distinct permutations on n symbols while wasting w
characters, for all values of w from 1 up to the point where all permutations are visited (i.e. these strings become
superpermutations).  The default is to find ALL such strings; if the "oneExample" option is specified, then only a single
example is found.  The "noRepeats" option explicitly rules out strings that contain any permutation more than once.

The strings for each value of w are written to files of the form:

Chaffin_<n>_W_<w>.txt

If the program is halted for some reason, when it is run again it will read back any files it finds with names of this form,
and restart computations for the w value of the last such file that it finds.
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

//	Constants
//	---------

//	Logical truth values

#define TRUE (1==1)
#define FALSE (1==0)

//	Smallest and largest values of n accepted

//	(Note that if we went higher than n=7, DBITS would need to increase, and we would also need to change some variables
//	from 32-bit to 64-bit ints. However, at this point it seems extremely unlikely that it would ever be practical to
//	run this code with n=8.)

#define MIN_N 3
#define MAX_N 7

//	Number of bits allowed for each digit in integer representation of permutations

#define DBITS 3

//	Macros
//	------

#define CHECK_MEM(p) if ((p)==NULL) {printf("Insufficient memory\n"); exit(EXIT_FAILURE);};

//	Structure definitions
//	---------------------

struct digitScore
{
int digit;
int score;
};

//	Global variables
//	----------------

int n;				//	The number of symbols in the permutations we are considering
int fn;				//	n!
int nmbits;			//	(n-1)*DBITS
int maxInt;			//	Highest integer representation of an n-digit sequence we can encounter, plus 1
int maxIntM;		//	Highest integer representation of an (n-1)-digit sequence we can encounter, plus 1
int maxW;			//	Largest number of wasted characters we allow for
char *curstr;		//	Current string
int max_perm;		//	Maximum number of permutations visited by any string seen so far
int *mperm_res;		//	For each number of wasted characters, the maximum number of permutations that can be visited
int *mperm_ruledOut;		//	For each number of wasted characters, the smallest number of permutations currently ruled out
int *nBest;			//	For each number of wasted characters, the number of strings that achieve mperm_res permutations
int *bestLen;		//	For each number of wasted characters, the lengths of the strings that visit mperm_res permutations
int **bestStrings;	//	For each number of wasted characters, a list of all strings that visit mperm_res permutations
int tot_bl;			//	The total number of wasted characters we are allowing in strings, in current search
char *unvisited;	//	Flags set FALSE when we visit a permutation, indexed by integer rep of permutation
char *valid;		//	Flags saying whether integer rep of digit sequence corresponds to a valid permutation
int *ldd;			//	For each digit sequence, n - (the longest run of distinct digits, starting from the last)
char *nextDigits;	//	For each (n-1)-length digit sequence, possible next digits in preferred order
int oneExample=FALSE;	//	Option that when TRUE limits search to a single example
int allExamples=TRUE;
int noRepeats=FALSE;
int allowRepeats=TRUE;
int done=FALSE;			//	Global flag we can set for speedy fall-through of recursion once we know there is nothing else we want to do
char outputFileName[256];

//	Function definitions
//	--------------------

void fillStr(int pos, int pfound, int partNum, int leftPerm);
void fillStr2(int pos, int pfound, int partNum, char *swap, int *bestStr, int len);
int fac(int k);
void makePerms(int n, int **permTab);
void writeCurrentString(int newFile, int size);
void clearFlags(int tperm0);
void readBackFile(FILE *fp, int w);
int compareDS(const void *ii0, const void *jj0);

//	Main program
//	------------

int main(int argc, const char * argv[])
{
if (argc>=2 && argv[1][0]>='0'+MIN_N && argv[1][0]<='0'+MAX_N)
	{
	n = argv[1][0]-'0';
	for (int i=2;i<argc;i++)
		{
	 	if (strcmp(argv[i],"oneExample")==0) oneExample=TRUE;
	 	else if (strcmp(argv[i],"noRepeats")==0) noRepeats=TRUE;
	 	else
	 		{
	 		printf("Unknown option %s\n",argv[i]);
	 		exit(EXIT_FAILURE);
	 		};
	 	};
	}
else
	{
	printf("Please specify n from %d to %d on the command line\n",MIN_N,MAX_N);
	exit(EXIT_FAILURE);
	};
	
allExamples = !oneExample;
allowRepeats = !noRepeats;
	
fn=fac(n);

//	Storage for current string

CHECK_MEM( curstr = (char *)malloc(2*fn*sizeof(char)) )

//	Storage for things associated with different numbers of wasted characters

maxW = fn;

CHECK_MEM( nBest = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( mperm_res = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( mperm_ruledOut = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( bestLen = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( bestStrings = (int **)malloc(maxW*sizeof(int *)) )

//	The value of mperm_ruledOut[w] is the lowest permutation count that we are currently certain cannot be achieved
//	with w wasted characters.

for (int i=0;i<maxW;i++) mperm_ruledOut[i]=fn+1;

//	Compute number of bits we will shift final digit

nmbits = DBITS*(n-1);

//	We represent permutations as p_1 + b*p_2 + b^2*p_3 + .... b^(n-1)*p_n, where b=2^(DBITS)
//	maxInt is the highest value this can take (allowing for non-permutations as well), plus 1
//	maxIntM is the equivalent for (n-1)-digit sequences

maxIntM = n;
for (int k=0;k<n-2;k++) maxIntM = (maxIntM<<DBITS)+n;
maxInt = (maxIntM<<DBITS)+n;
maxInt++;
maxIntM++;

//	Generate a table of all permutations of n symbols

int **permTab;
CHECK_MEM( permTab = (int **)malloc(n*sizeof(int *)) )
makePerms(n,permTab+n-1);
int *p0 = permTab[n-1];

//	Set up flags that say whether each number is a valid permutation or not,
//	and whether we have visited a given permutation.

CHECK_MEM( valid = (char *)malloc(maxInt*sizeof(char)) )
CHECK_MEM( unvisited = (char *)malloc(maxInt*sizeof(char)) )

for (int i=0;i<maxInt;i++) valid[i]=FALSE;
for (int i=0;i<fn;i++)
	{
	int tperm=0;
	for (int j0=0;j0<n;j0++)
		{
		tperm+=(p0[n*i+j0]<<(j0*DBITS));
		};
	valid[tperm]=TRUE;
	};
	
//	For each number d_1 d_2 d_3 ... d_n as a digit sequence, what is
//	the length of the longest run d_j ... d_n in which all the digits are distinct.

CHECK_MEM( ldd = (int *)malloc(maxInt*sizeof(int)) )

//	Loop through all n-digit sequences

static int dseq[MAX_N];
for (int i=0;i<n;i++) dseq[i]=1;
int more=TRUE;
while (more)
	{
	int tperm=0;
	for (int j0=0;j0<n;j0++)
		{
		tperm+=(dseq[j0]<<(j0*DBITS));
		};
		
	if (valid[tperm]) ldd[tperm]=0;
	else
		{
		int ok=TRUE;
		for (int l=2;l<=n;l++)
			{
			for (int i=0;i<l && ok;i++)
			for (int j=i+1;j<l;j++)
				{
				if (dseq[n-1-i]==dseq[n-1-j])
					{
					ok=FALSE;
					break;
					};
				};

			if (!ok)
				{
				ldd[tperm] = n-(l-1);
				break;
				};
			};
		};
		
	for (int h=n-1;h>=0;h--)
		{
		if (++dseq[h]>n)
			{
			if (h==0)
				{
				more=FALSE;
				break;
				};
			dseq[h]=1;
			}
		else break;
		};	
	};
	
//	Set up a table of the next digits to follow from a given (n-1)-digit sequence

struct digitScore *sortDS;
CHECK_MEM( sortDS = (struct digitScore *)malloc((n-1)*sizeof(struct digitScore)) )
CHECK_MEM( nextDigits = (char *)malloc(maxIntM*(n-1)*sizeof(char)) )

//	Loop through all (n-1)-digit sequences

for (int i=0;i<n-1;i++) dseq[i]=1;
more=TRUE;
while (more)
	{
	int part=0;
	for (int j0=0;j0<n-1;j0++)
		{
		part+=(dseq[j0]<<(j0*DBITS));
		};
		
	//	Sort potential next digits by the ldd score we get by appending them
	
	int q=0;
	for (int d=1;d<=n;d++)
	if (d != dseq[n-2])
		{
		int t = (d<<nmbits) + part;
		sortDS[q].digit = d;
		sortDS[q].score = ldd[t];
		q++;
		};
		
	qsort(sortDS,n-1,sizeof(struct digitScore),compareDS);
	
	for (int z=0;z<n-1;z++)	nextDigits[(n-1)*part+z] = sortDS[z].digit;
	
	for (int h=n-2;h>=0;h--)
		{
		if (++dseq[h]>n)
			{
			if (h==0)
				{
				more=FALSE;
				break;
				};
			dseq[h]=1;
			}
		else break;
		};	
	};
	
mperm_res[0] = n;		//	With no wasted characters, we can visit n permutations
max_perm = n;			//	Any new maximum (for however many wasted characters) must exceed that;
						//	we don't reset this within the loop, as the true maximum will increase as we increase tot_bl

//	Set up the zero-wasted-characters string that visits n permutations:  1 2 3 ... n 1 2 3 (n-1)

bestLen[0] = 2*n-1;
nBest[0] = 1;
CHECK_MEM( bestStrings[0] = (int *)malloc(bestLen[0]*nBest[0]*sizeof(int)) )
for (int i=0;i<n;i++) bestStrings[0][i]=i+1;
for (int i=n;i<2*n-1;i++) bestStrings[0][i]=i-n+1;
						
//	Fill the first n entries of the string with [1...n], and compute the
//	associated integer, as well as the partial integer for [2...n]

int tperm0=0;
for (int j0=0;j0<n;j0++)
	{
	curstr[j0] = j0+1;
	tperm0 += (j0+1)<<(j0*DBITS);
	};
int partNum0 = tperm0>>DBITS;

//	Check for any pre-existing files

int resumeFrom = 1;
int didResume = FALSE;

for (tot_bl=1; tot_bl<maxW; tot_bl++)
	{
	sprintf(outputFileName,"Chaffin_%d_W_%d%s.txt",n,tot_bl,oneExample?"_OE":"");
	FILE *fp = fopen(outputFileName,"ra");
	if (fp==NULL)
		{
		sprintf(outputFileName,"Chaffin_%d_W_%d.txt",n,tot_bl);
		fp = fopen(outputFileName,"ra");
		if (fp==NULL) break;
		};
	
	printf("Reading pre-existing file %s ...\n",outputFileName);
	bestLen[tot_bl] = 0;
	nBest[tot_bl] = 0;
	while (TRUE)
		{
		int c = fgetc(fp);
		if (c==EOF) break;
		if (c=='\n') nBest[tot_bl]++;
		if (nBest[tot_bl]==0) bestLen[tot_bl]++;
		};
	fclose(fp);
	fp = fopen(outputFileName,"ra");
	readBackFile(fp, tot_bl);
	fclose(fp);
	
	mperm_res[tot_bl] = bestLen[tot_bl] - tot_bl - (n-1);
	
	printf("Found %d strings of length %d, implying %d permutations, in file %s\n",nBest[tot_bl],bestLen[tot_bl],mperm_res[tot_bl],outputFileName);
	
	resumeFrom = tot_bl;
	didResume = TRUE;
	};
						
//	tot_bl is the total number of wasted characters we are allowing in strings;
//	we loop through increasing the value

int expectedInc = 2*(n-4);			//	We guess that max_perm will increase by at least this much at each step

for (tot_bl=resumeFrom; tot_bl<maxW; tot_bl++)
	{
	sprintf(outputFileName,"Chaffin_%d_W_%d%s.txt",n,tot_bl,oneExample?"_OE":"");
	
	//	Gamble on max_perm increasing by at least expectedInc; if it doesn't, we will decrement it and retry
	
	int old_max = mperm_res[tot_bl-1];
	max_perm = old_max + expectedInc;

	if (allExamples)
		{
		if (max_perm > fn) max_perm = fn;
		}
	else
		{
		if (max_perm >= fn) max_perm = fn-1;
		};
	
	if (didResume && tot_bl==resumeFrom)
		{
		max_perm = mperm_res[resumeFrom];
		if (allExamples) nBest[tot_bl]=0;
		}
	else nBest[tot_bl]=0;
	
	//	Recursively fill in the string

	while (max_perm>0)
		{
		clearFlags(tperm0);
		bestLen[tot_bl]=max_perm+tot_bl+n-1;
		done=FALSE;
		fillStr(n,1,partNum0,TRUE);
		if (nBest[tot_bl] > 0) break;
		
		//	We searched either for matches to max_perm (allExamples) or strings that did better than max_perm (oneExample), and came up empty
		
		printf("Backtracking, reducing max_perm from %d to %d\n",max_perm,max_perm-1);
		if (allExamples) mperm_ruledOut[tot_bl] = max_perm;
		else mperm_ruledOut[tot_bl] = max_perm+1;
		max_perm--;
		};
		
	if (max_perm - old_max < expectedInc)
		{
		printf("Reduced default increment in max_perm from %d to ",expectedInc);
		expectedInc = max_perm - old_max;
		if (expectedInc <= 0) expectedInc = 1;
		printf("%d\n",expectedInc);
		};

	
	//	Record maximum number of permutations visited with this many wasted characters

	mperm_res[tot_bl] = max_perm;

	printf("%d wasted characters: at most %d permutations, in %d characters, %d examples\n",
		tot_bl,max_perm,bestLen[tot_bl],nBest[tot_bl]);

	if (max_perm >= fn)
		{
		printf("\n-----\nDONE!\n-----\nMinimal superpermutations on %d symbols have %d wasted characters and a length of %d.\n\n",
			n,tot_bl,fn+tot_bl+n-1);
		break;
		};
		
	//	Read back list of best strings
	
	FILE *fp = fopen(outputFileName,"ra");
	if (fp==NULL)
		{
		printf("Unable to open file %s to read\n",outputFileName);
		exit(EXIT_FAILURE);
		};
	
	readBackFile(fp, tot_bl);
	fclose(fp);
	};

return 0;
}

// this function recursively fills the string

void fillStr(int pos, int pfound, int partNum, int leftPerm)
{
if (done) return;

int j1, newperm;
int tperm;
int alreadyWasted = pos - pfound - n + 1;	//	Number of character wasted so far
int spareW = tot_bl - alreadyWasted;		//	Maximum number of further characters we can waste while not exceeding tot_bl

//	If we can only match the current max_perm by using an optimal string for our remaining quota of wasted characters,
//	we try using those strings (swapping digits to make them start from the permutation we just visited).

if	(allExamples && leftPerm && spareW < tot_bl && mperm_res[spareW] + pfound - 1 == max_perm)
	{
	for (int i=0;i<nBest[spareW];i++)
		{
		int len = bestLen[spareW];
		int *bestStr = bestStrings[spareW] + i*len;
		char *swap = curstr + pos - n - 1;
		fillStr2(pos,pfound,partNum,swap,bestStr+n,len-n);
		};
	return;
	};
	
//	Loop to try each possible next digit we could append
//	These have been sorted into increasing order of ldd[tperm]
	
char *nd = nextDigits + (n-1)*partNum;
for	(int z=0; z<n-1; z++)
	{
	j1 = nd[z];
	tperm = partNum + (j1<<nmbits);
	
	//	ldd[tperm] tells us the minimum number of further characters we would need to waste
	//	before visiting another permutation.
	
	int spareW0 = spareW - ldd[tperm];
	if (spareW0<0) return;
	
	curstr[pos] = j1;

	// Check to see if this contributes a new permutation or not
	
	int vperm = valid[tperm];
	newperm = vperm && unvisited[tperm];

	// now go to the next level of the recursion
	
	if (newperm)
		{
		if (pfound+1>max_perm)
			{
			writeCurrentString(TRUE,pos+1);
			nBest[tot_bl]=1;
			bestLen[tot_bl]=pos+1;
			max_perm = pfound+1;
			if (oneExample && max_perm+1 >= mperm_ruledOut[tot_bl])
				{
				done=TRUE;
				return;
				};
			}
		else if (pfound+1==max_perm)
			{
			writeCurrentString(nBest[tot_bl]==0,pos+1);
			nBest[tot_bl]++;
			};

		unvisited[tperm]=FALSE;
		fillStr(pos+1, pfound+1, tperm>>DBITS, TRUE);
		unvisited[tperm]=TRUE;
		}
	else if	(spareW > 0)
		{
		if (vperm)
			{
			if (allowRepeats)
				{
				int d = mperm_res[spareW-1] + pfound - max_perm;
				if	(
					(oneExample && d > 0) || (allExamples && d >= 0)
					)
					{
					fillStr(pos+1, pfound, tperm>>DBITS, TRUE);
					};
				};
			}
		else
			{
			if (spareW0 >=tot_bl)
				{
				printf("spareW0=%d tot_bl=%d\n",spareW0,tot_bl);
				exit(EXIT_FAILURE);
				};
			int d = mperm_res[spareW0] + pfound - max_perm;
			if	(
				(oneExample && d > 0) || (allExamples && d >= 0)
				)
				{
				fillStr(pos+1, pfound, tperm>>DBITS, FALSE);
				}
			else return;
			};
		};
	};
}

//	Version that fills in the string when we are following a previously computed best string
//	rather than trying all digits.

void fillStr2(int pos, int pfound, int partNum, char *swap, int *bestStr, int len)
{
if (len<=0) return;		//	No more digits left in the template we are following

int j1, newperm;
int tperm;
int alreadyWasted = pos - pfound - n + 1;

j1 = swap[*bestStr];	//	Get the next digit from the template, swapped to make it start at our chosen permutation

// there is never any benefit to having 2 of the same character next to each other
	
if	(j1 != curstr[pos-1])
	{
	curstr[pos] = j1;
	tperm = partNum + (j1<<nmbits);

	// Check to see if this contributes a new permutation or not
	
	int vperm = valid[tperm];
	newperm = vperm && unvisited[tperm];

	// now go to the next level of the recursion
	
	if (newperm)
		{
		if (pfound+1>max_perm)
			{
			printf("Reached a point in the code that should be impossible!\n");
			exit(EXIT_FAILURE);
			}
		else if (pfound+1==max_perm)
			{
			writeCurrentString(nBest[tot_bl]==0,pos+1);
			nBest[tot_bl]++;
			};

		unvisited[tperm]=FALSE;
		fillStr2(pos+1, pfound+1, tperm>>DBITS, swap, bestStr+1, len-1);
		unvisited[tperm]=TRUE;

	// the quantity alreadyWasted = pos - pfound - n + 1 is the number of already-used blanks
	
		}
	else if	(alreadyWasted < tot_bl)
		{
		if	(((!vperm) || allowRepeats) && mperm_res[tot_bl - (alreadyWasted+1)] + pfound >= max_perm)
			{
			fillStr2(pos+1, pfound, tperm>>DBITS, swap, bestStr+1, len-1);
			};
		};
	};
}

// this function computes the factorial of a number

int fac(int k)
{
if (k <= 1) return 1;
else return k*fac(k-1);
}

//	Generate all permutations of 1 ... n;
//	generates lists for lower values of n
//	in the process.

void makePerms(int n, int **permTab)
{
int fn=fac(n);
int size=n*fn;
int *res;

CHECK_MEM( res=(int *)malloc(size*sizeof(int)) )

if (n==1)
	{
	res[0]=1;
	}
else
	{
	makePerms(n-1, permTab-1);
	int *prev=*(permTab-1);
	int p=0;
	int pf=fac(n-1);
	for (int k=0;k<pf;k++)
		{
		for (int q=n-1;q>=0;q--)
			{
			int r=0;
			for (int s=0;s<n;s++)
				{
				if (s==q) res[p++]=n;
				else res[p++]=prev[k*(n-1)+(r++)];
				};
			};
		};
	};

*permTab = res;
}

void writeCurrentString(int newFile, int size)
{
FILE *fp;
fp = fopen(outputFileName,newFile?"wa":"aa");
if (fp==NULL)
	{
	printf("Unable to open file %s to %s\n",outputFileName,newFile?"write":"append");
	exit(EXIT_FAILURE);
	};
for (int k=0;k<size;k++) fprintf(fp,"%c",'0'+curstr[k]);
fprintf(fp,"\n");
fclose(fp);
}

void clearFlags(int tperm0)
{
for (int i=0; i<maxInt; i++) unvisited[i] = TRUE;
unvisited[tperm0]=FALSE;
}

void readBackFile(FILE *fp, int w)
{
CHECK_MEM( bestStrings[w] = (int *)malloc(bestLen[w]*nBest[w]*sizeof(int)) )

int ptr=0;
for (int i=0;i<nBest[w];i++)
	{
	for (int j=0;j<bestLen[w];j++)
		{
		bestStrings[w][ptr++] = fgetc(fp)-'0';
		};
	fgetc(fp);
	};
}

//	Compare two digitScore structures for quicksort()

int compareDS(const void *ii0, const void *jj0)
{
struct digitScore *ii=(struct digitScore *)ii0, *jj=(struct digitScore *)jj0;
if (ii->score < jj->score) return -1;
if (ii->score > jj->score) return 1;
if (ii->digit < jj->digit) return -1;
if (ii->digit > jj->digit) return 1;
return 0;
}
