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
Version: 2.13
Last Updated: 6 May 2019

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

//	If GET_OCP_DATA is TRUE, we gather data on where 1-cycle tracking first starts pruning
//	If GET_OCP_DATA is FALSE, we USE data gathered when it was TRUE that has been recorded in the array ocpThreshold[]

#define GET_OCP_DATA FALSE

#if GET_OCP_DATA

	#define PRINT_OCP_DATA printf("*** 1-cycle pruning first effective for lowestW=%d ***\n",lowestW);

	#define MONITOR_OCP \
		prunedOCP++; \
		if (tot_bl<lowestW) \
			{ \
			lowestW=tot_bl; \
			PRINT_OCP_DATA \
			};

#else

	#define MONITOR_OCP prunedOCP++;
	
#endif


//	Structure definitions
//	---------------------

struct digitScore
{
int digit;
int score;
int fullNum;
int nextPart;
int nextPerm;
};

//	Global variables
//	----------------

int n;				//	The number of symbols in the permutations we are considering
int fn;				//	n!
int nm;				//	n-1
int nmbits;			//	(n-1)*DBITS
int maxInt;			//	Highest integer representation of an n-digit sequence we can encounter, plus 1
int maxIntM;		//	Highest integer representation of an (n-1)-digit sequence we can encounter, plus 1
int maxW;			//	Largest number of wasted characters we allow for
char *curstr;		//	Current string
int *dvals;			//	Value of discriminant at each level
int max_perm;		//	Maximum number of permutations visited by any string seen so far
int *mperm_res;		//	For each number of wasted characters, the maximum number of permutations that can be visited
int *mperm_ruledOut;		//	For each number of wasted characters, the smallest number of permutations currently ruled out
int *successor1;	//	For each permutation, its weight-1 successor
int *successor2;	//	For each permutation, its weight-2 successor
int *nBest;			//	For each number of wasted characters, the number of strings that achieve mperm_res permutations
int *bestLen;		//	For each number of wasted characters, the lengths of the strings that visit final mperm_res permutations
char **bestStrings;	//	For each number of wasted characters, a list of all strings that visit final mperm_res permutations
int *klbLen;		//	For each number of wasted characters, the lengths of the strings that visit known-lower-bound permutations
char **klbStrings;	//	For each number of wasted characters, a list of all strings that visit known-lower-bound permutations
int tot_bl;			//	The total number of wasted characters we are allowing in strings, in current search
char *unvisited;	//	Flags set FALSE when we visit a permutation, indexed by integer rep of permutation
char *valid;		//	Flags saying whether integer rep of digit sequence corresponds to a valid permutation
int *ldd;			//	For each digit sequence, n - (the longest run of distinct digits, starting from the last)
struct digitScore *nextDigits;	//	For each (n-1)-length digit sequence, possible next digits in preferred order

int noc;				//	Number of 1-cycles
int nocThresh;			//	Threshold for unvisited 1-cycles before we try new bounds		
int *oneCycleCounts;	//	Number of unvisited permutations in each 1-cycle
int *oneCycleIndices;	//	The 1-cycle to which each permutation belongs
int oneCycleBins[MAX_N+1];	//	The numbers of 1-cycles that have 0 ... n unvisited permutations

int oneExample=FALSE;	//	Option that when TRUE limits search to a single example
int allExamples=TRUE;
int noRepeats=FALSE;
int allowRepeats=TRUE;
int fallBackTo;			//	Level we fall back to when we are deeper in the tree than we need to be
unsigned long int nodeCount=0;
char outputFileName[256], summaryFileName[256];

//	Monitoring 1-cycle tracking

int ocpTrackingOn, ocpTrackingOff;
long int prunedOCP=0;

#if GET_OCP_DATA
int lowestW;
#else
//	For n=0,1,2,3,4,5,6,7
int ocpThreshold[]={1000,1000,1000,1000, 6, 24, 120, 720};
#endif

//	Function definitions
//	--------------------

void fillStr(int pos, int pfound, int partNum, int leftPerm);
void fillStr2(int pos, int pfound, int partNum, char *remapDigits, char *bestStr, int len);
int fac(int k);
void makePerms(int n, int **permTab);
void writeCurrentString(int newFile, int size);
void maybeUpdateLowerBound(int tperm, int size, int w, int p);
void clearFlags(int tperm0);
void readBackFile(FILE *fp, int w);
int compareDS(const void *ii0, const void *jj0);
void rClassMin(int *p, int n);
int pruneOnPerms(int w, int d0);
void printDigits(int t);

//	Main program
//	------------

int main(int argc, const char * argv[])
{
FILE *fp;

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

sprintf(summaryFileName,"ChaffinMethodMaxPerms_%d.txt",n);
	
fn=fac(n);

//	Storage for current string

CHECK_MEM( curstr = (char *)malloc(2*fn*sizeof(char)) )

//	Values of discriminant (sign says whether to go deeper)

CHECK_MEM( dvals = (int *)malloc(2*fn*sizeof(int)) )

//	Storage for things associated with different numbers of wasted characters

maxW = fn;

CHECK_MEM( nBest = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( mperm_res = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( mperm_ruledOut = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( bestLen = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( bestStrings = (char **)malloc(maxW*sizeof(char *)) )
CHECK_MEM( klbLen = (int *)malloc(maxW*sizeof(int)) )
CHECK_MEM( klbStrings = (char **)malloc(maxW*sizeof(char *)) )

//	The value of mperm_res[w] is the highest permutation count that we are current certain can be achieved
//	with w wasted characters.

for (int i=0;i<maxW;i++) mperm_res[i]=n;

//	The value of mperm_ruledOut[w] is the lowest permutation count that we are currently certain cannot be achieved
//	with w wasted characters.

for (int i=0;i<maxW;i++) mperm_ruledOut[i]=fn+1;

//	Storage for known-lower-bound strings

for (int i=0;i<maxW;i++)
	{
	CHECK_MEM( klbStrings[i] = (char *)malloc(2*fn*sizeof(char)))
	klbLen[i] = 0;
	};

//	Compute number of bits we will shift final digit

nm = n-1;
nmbits = DBITS*nm;

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
//
//	Also, find the weight-1 and weight-2 successors of each permutation

CHECK_MEM( valid = (char *)malloc(maxInt*sizeof(char)) )
CHECK_MEM( unvisited = (char *)malloc(maxInt*sizeof(char)) )
CHECK_MEM( successor1 = (int *)malloc(maxInt*sizeof(int)) )
CHECK_MEM( successor2 = (int *)malloc(maxInt*sizeof(int)) )

for (int i=0;i<maxInt;i++) valid[i]=FALSE;
for (int i=0;i<fn;i++)
	{
	int tperm=0, tperm1=0, tperm2=0;
	for (int j0=0;j0<n;j0++)
		{
		tperm+=(p0[n*i+j0]<<(j0*DBITS));
		
		//	Left shift digits by one to get weight-1 successor
		
		tperm1+=(p0[n*i+(j0+1)%n]<<(j0*DBITS));
		
		//	Left shift digits by 2 and swap last pair
		
		int k0;
		if (j0==n-1) k0=0;
		else if (j0==n-2) k0=1;
		else k0=(j0+2)%n;
		
		tperm2+=(p0[n*i+k0]<<(j0*DBITS));
		};
	valid[tperm]=TRUE;
	successor1[tperm]=tperm1;
	successor2[tperm]=tperm2;
/*
		printDigits(tperm);
		printDigits(tperm1);
		printDigits(tperm2);
		printf("----\n");
*/
	};
	
//	For each number d_1 d_2 d_3 ... d_n as a digit sequence, what is
//	the length of the longest run d_j ... d_n in which all the digits are distinct.

//	Also, record which 1-cycle each permutation belongs to

CHECK_MEM( ldd = (int *)malloc(maxInt*sizeof(int)) )

noc = fac(n-1);
nocThresh = noc/2;
CHECK_MEM( oneCycleCounts = (int *)malloc(maxInt*sizeof(int)) )
CHECK_MEM( oneCycleIndices = (int *)malloc(maxInt*sizeof(int)) )

//	Loop through all n-digit sequences

static int dseq[MAX_N], dseq2[MAX_N];
for (int i=0;i<n;i++) dseq[i]=1;
int more=TRUE;
while (more)
	{
	int tperm=0;
	for (int j0=0;j0<n;j0++)
		{
		tperm+=(dseq[j0]<<(j0*DBITS));
		};
		
	if (valid[tperm])
		{
		ldd[tperm]=0;
		for (int i=0;i<n;i++) dseq2[i]=dseq[i];
		rClassMin(dseq2,n);
		int r=0;
		for (int j0=0;j0<n; j0++)
			{
			r+=(dseq2[j0]<<(j0*DBITS));
			};
		oneCycleIndices[tperm]=r;
		}
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

CHECK_MEM( nextDigits = (struct digitScore *)malloc(maxIntM*(n-1)*sizeof(struct digitScore)) )
int dsum = n*(n+1)/2;

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
	struct digitScore *nd = nextDigits+(n-1)*part;
		
	//	Sort potential next digits by the ldd score we get by appending them
	
	int q=0;
	for (int d=1;d<=n;d++)
	if (d != dseq[n-2])
		{
		int t = (d<<nmbits) + part;
		nd[q].digit = d;
		int ld = nd[q].score = ldd[t];
		
		//	The full number n-digit number we get if we append the chosen digit to the previous n-1
		
		nd[q].fullNum = t;
		
		//	The next (n-1)-digit partial number that follows (dropping oldest of the current n)
		
		int p = nd[q].nextPart = t>>DBITS;
		
		//	If there is a unique permutation after 0 or 1 wasted characters, precompute its number
		
		if (ld==0) nd[q].nextPerm = t;		//	Adding the current chosen digit gets us there
		else if (ld==1)						//	After the current chosen digit, a single subsequent choice gives a unique permutation
			{
			int d2 = dsum-d;
			for (int z=1;z<=n-2;z++) d2-=dseq[z];
			nd[q].nextPerm = (d2<<nmbits) + p;
			}
		else nd[q].nextPerm = -1;
		q++;
		};
		
	qsort(nd,n-1,sizeof(struct digitScore),compareDS);
	
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
CHECK_MEM( bestStrings[0] = (char *)malloc(bestLen[0]*nBest[0]*sizeof(char)) )
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
	fp = fopen(outputFileName,"rt");
	if (fp==NULL)
		{
		sprintf(outputFileName,"Chaffin_%d_W_%d.txt",n,tot_bl);
		fp = fopen(outputFileName,"rt");
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
	fp = fopen(outputFileName,"rt");
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

#if GET_OCP_DATA
lowestW=maxW;
#endif

//	Set up 1-cycle information

for (int i=0;i<maxInt;i++) oneCycleCounts[i]=n;
oneCycleCounts[oneCycleIndices[tperm0]]=n-1;

for (int b=0;b<n-1;b++) oneCycleBins[b]=0;
oneCycleBins[n]=noc-1;
oneCycleBins[n-1]=1;

for (tot_bl=resumeFrom; tot_bl<maxW; tot_bl++)
	{
	//	Rule out increasing by more than n when we add one more wasted character
	
	mperm_ruledOut[tot_bl] = mperm_res[tot_bl-1] + n + 1;

	sprintf(outputFileName,"Chaffin_%d_W_%d%s.txt",n,tot_bl,oneExample?"_OE":"");
	
	#if GET_OCP_DATA
		ocpTrackingOn = TRUE;
	#else
		ocpTrackingOn = tot_bl >= ocpThreshold[n];
		if (ocpTrackingOn) printf("[ocpTrackingOn]\n");
	#endif
	ocpTrackingOff = !ocpTrackingOn;
	
	//	Gamble on max_perm increasing by at least expectedInc; if it doesn't, we will decrement it and retry
	
	int old_max = mperm_res[tot_bl-1];
	max_perm = old_max + expectedInc;
	
	printf("[Starting search for w=%d, with initial max_perm of %d, mperm_ruledOut=%d]\n",tot_bl,max_perm,mperm_ruledOut[tot_bl]);
	
	//	Look at lower bound we might have obtained from previous calculation
	
	nBest[tot_bl]=0;
	if (klbLen[tot_bl] > 0 && mperm_res[tot_bl] >= max_perm)
		{
		for (int k=0;k<klbLen[tot_bl];k++) curstr[k]=klbStrings[tot_bl][k];
		writeCurrentString(TRUE,klbLen[tot_bl]);
		max_perm = mperm_res[tot_bl];
		nBest[tot_bl]=1;
		bestLen[tot_bl]=klbLen[tot_bl];
		printf("[Using max_perm of %d from previous calculations]\n",max_perm);
		};
		
	if (oneExample && klbLen[tot_bl]>0 && mperm_res[tot_bl]+1 >= mperm_ruledOut[tot_bl])
		{
		printf("[Extending strings from previous calculations already gave us an answer:]\n");
		}
	else
		{
		if (allExamples)
			{
			nBest[tot_bl]=0;
			if (max_perm > fn) max_perm = fn;
			}
		else
			{
			if (max_perm >= fn) max_perm = fn-1;
			};
		
		if (didResume && tot_bl==resumeFrom)
			{
			max_perm = mperm_res[resumeFrom];
			};
		
		//	Recursively fill in the string

		while (max_perm>0)
			{
			clearFlags(tperm0);
			bestLen[tot_bl]=max_perm+tot_bl+n-1;
			fallBackTo=2*fn;
			fillStr(n,1,partNum0,TRUE);
			if (nBest[tot_bl] > 0) break;
			
			//	We searched either for matches to max_perm (allExamples) or strings that did better than max_perm (oneExample), and came up empty
			
			if (allExamples) mperm_ruledOut[tot_bl] = max_perm;
			else mperm_ruledOut[tot_bl] = max_perm+1;
			max_perm--;
			
			printf("Backtracking, reducing max_perm from %d to %d, mperm_ruledOut=%d (%lu calls)\n",
				max_perm+1,max_perm,mperm_ruledOut[tot_bl],nodeCount);
			};

		if (max_perm - old_max < expectedInc)
			{
			printf("Reduced default increment in max_perm from %d to ",expectedInc);
			expectedInc = max_perm - old_max;
			if (expectedInc <= 0) expectedInc = 1;
			printf("%d\n",expectedInc);
			};

		};
	
	//	Record maximum number of permutations visited with this many wasted characters

	mperm_res[tot_bl] = max_perm;

	printf("%d wasted characters: at most %d permutations, in %d characters, %d examples (%lu calls)\n\n",
		tot_bl,max_perm,bestLen[tot_bl],nBest[tot_bl],nodeCount);
		
	fp=fopen(summaryFileName,"at");
	if (fp!=NULL)
		{
		fprintf(fp,"%d\t%d\n",tot_bl,max_perm);
		fclose(fp);
		};
		
	if (max_perm >= fn)
		{
		printf("\n-----\nDONE!\n-----\nMinimal superpermutations on %d symbols have %d wasted characters and a length of %d.\n\n",
			n,tot_bl,fn+tot_bl+n-1);
		break;
		};
		
	//	Read back list of best strings
	
	fp = fopen(outputFileName,"rt");
	if (fp==NULL)
		{
		printf("Unable to open file %s to read\n",outputFileName);
		exit(EXIT_FAILURE);
		};
	
	readBackFile(fp, tot_bl);
	fclose(fp);
	};
	
#if GET_OCP_DATA
PRINT_OCP_DATA
#endif
printf("OCP tracking pruned the search %ld times\n",prunedOCP);

return 0;
}

// this function recursively fills the string

void fillStr(int pos, int pfound, int partNum, int leftPerm)
{
if (pos > fallBackTo) return; else fallBackTo = 2*fn;
nodeCount++;

int tperm, ld;
int alreadyWasted = pos - pfound - n + 1;	//	Number of character wasted so far
int spareW = tot_bl - alreadyWasted;		//	Maximum number of further characters we can waste while not exceeding tot_bl

//	If we can only match the current max_perm by using an optimal string for our remaining quota of wasted characters,
//	we try using those strings (remapping digits to make them start from the permutation we just visited).

if	(allExamples && leftPerm && spareW < tot_bl && mperm_res[spareW] + pfound - 1 == max_perm)
	{
	for (int i=0;i<nBest[spareW];i++)
		{
		int len = bestLen[spareW];
		char *bestStr = bestStrings[spareW] + i*len;
		char *remapDigits = curstr + pos - n - 1;
		fillStr2(pos,pfound,partNum,remapDigits,bestStr+n,len-n);
		};
	return;
	};
	
//	Loop to try each possible next digit we could append.
//	These have been sorted into increasing order of ldd[tperm], the minimum number of further wasted characters needed to get a permutation.
	
struct digitScore *nd = nextDigits + nm*partNum;

//	To be able to fully exploit foreknowledge that we are heading for a visited permutation after 1 wasted character, we need to ensure
//	that we still traverse the loop in order of increasing waste.
//
//	For example, for n=5 we might have 1123 as the last 4 digits, with the choices:
//
//		1123 | add 4 -> 1234 ld = 1
//		1123 | add 5 -> 1235 ld = 1
//
//  The affected choices will always be the first two in the loop, and
//	we only need to swap them if the first permutation is visited and the second is not.

int swap01 = (nd->score==1 && (!unvisited[nd->nextPerm]) && unvisited[nd[1].nextPerm]);

//	Also, it is not obligatory, but useful, to swap the 2nd and 3rd entries (indices 1 and 2) if we have (n-1) distinct digits in the
//	current prefix, with the first 3 choices ld=0,1,2, but the 1st and 2nd entries lead to a visited permutation.  This will happen
//	if we are on the verge of looping back at the end of a 2-cycle.
//
//		1234 | add 5 -> 12345 ld = 0 (but 12345 has already been visited)
//		1234 | add 1 -> 12341 ld = 1 (but 23415 has been visited already)
//		1234 | add 2 -> 12342 ld = 2

int swap12 = FALSE;					//	This is set later if the conditions are met

int deferredRepeat=FALSE;			//	If we find a repeated permutation, we follow that branch last
int deltaMaxPerm=0;					//	Amount max_perm is increased by a new string

for	(int y=0; y<nm; y++)
	{
	int z;
	if (swap01)
		{
		if (y==0) z=1; else if (y==1) {z=0; swap01=FALSE;} else z=y;
		}
	else if (swap12)
		{
		if (y==1) z=2; else if (y==2) {z=1; swap12=FALSE;} else z=y; 
		}
	else z=y;
	
	struct digitScore *ndz = nd+z;
	ld = ndz->score;
	
	//	ld tells us the minimum number of further characters we would need to waste
	//	before visiting another permutation.
	
	int spareW0 = spareW - ld;
	
	//	Having taken care of ordering issues, we can treat a visited permutation after 1 wasted character as an extra wasted character
	
	if (ld==1 && !unvisited[ndz->nextPerm]) spareW0--;
		
	if (spareW0<0) break;
	
	curstr[pos] = ndz->digit;
	tperm = ndz->fullNum;
	
	int vperm = (ld==0);
	if (vperm && unvisited[tperm])
		{
		if (pfound+1>max_perm)
			{
			writeCurrentString(TRUE,pos+1);
			nBest[tot_bl]=1;
			bestLen[tot_bl]=pos+1;
			deltaMaxPerm = pfound+1-max_perm;
			max_perm = pfound+1;
			printf("[Found a string that increased max_perm to %d]\n",max_perm);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);
			if (oneExample && max_perm+1 >= mperm_ruledOut[tot_bl])
				{
				printf("[Search is done]\n");
				fallBackTo=-1;
				return;
				};
			}
		else if (pfound+1==max_perm)
			{
			writeCurrentString(nBest[tot_bl]==0,pos+1);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);
			nBest[tot_bl]++;
			};

		unvisited[tperm]=FALSE;
		if (ocpTrackingOn)
			{
			int prevC=0, oc=0;
			oc=oneCycleIndices[tperm];
			prevC = oneCycleCounts[oc]--;
			oneCycleBins[prevC]--;
			oneCycleBins[prevC-1]++;
		
			dvals[pos+1]=10000;
			fillStr(pos+1, pfound+1, ndz->nextPart, TRUE);
		
			oneCycleBins[prevC-1]--;
			oneCycleBins[prevC]++;
			oneCycleCounts[oc]=prevC;
			}
		else
			{
			dvals[pos+1]=10000;
			fillStr(pos+1, pfound+1, ndz->nextPart, TRUE);
			};
		unvisited[tperm]=TRUE;
		}
	else if	(spareW > 0)
		{
		if (vperm)
			{
			if (allowRepeats) deferredRepeat=TRUE;
			swap12 = !unvisited[nd[1].nextPerm];
			}
		else
			{
			int d = pruneOnPerms(spareW0, pfound - max_perm);
			if	(
				(oneExample && d > 0) || (allExamples && d >= 0)
				)
				{
				dvals[pos+1]=d;
				fillStr(pos+1, pfound, ndz->nextPart, FALSE);
				}
			else break;
			};
		};
	};
	
//	If we encountered a choice that led to a repeat visit to a permutation, we follow (or prune) that branch now.
//	It will always come from the FIRST choice in the original list, as that is where any valid permutation must be.
	
if (deferredRepeat)
	{
	int d = pruneOnPerms(spareW-1, pfound - max_perm);
	if	(
		(oneExample && d > 0) || (allExamples && d >= 0)
		)
		{
		curstr[pos] = nd->digit;
		dvals[pos+1]=d;
		fillStr(pos+1, pfound, nd->nextPart, TRUE);
		};
	};

if (deltaMaxPerm)
	{
	printf("[deltaMaxPerm=%d]\n",deltaMaxPerm);
	for (int i=n+1;i<=pos;i++) dvals[i]-=deltaMaxPerm;
	for (int i=n+1;i<pos;i++)
		{
		if ((oneExample && dvals[i]<=0) || (allExamples && dvals[i] <0))
			{
			fallBackTo = i-1;
			printf("[Fall back from level %d to level %d]\n",pos,fallBackTo);
			break;
			};
		};
	};
}

//	Version that fills in the string when we are following a previously computed best string
//	rather than trying all digits.

void fillStr2(int pos, int pfound, int partNum, char *remapDigits, char *bestStr, int len)
{
if (len<=0) return;		//	No more digits left in the template we are following

int j1;
int tperm;
int alreadyWasted = pos - pfound - n + 1;

j1 = remapDigits[*bestStr];	//	Get the next digit from the template, remapped to make it start at our chosen permutation

// there is never any benefit to having 2 of the same character next to each other
	
if	(j1 != curstr[pos-1])
	{
	curstr[pos] = j1;
	tperm = partNum + (j1<<nmbits);

	// Check to see if this contributes a new permutation or not
	
	int vperm = valid[tperm];

	// now go to the next level of the recursion
	
	if (vperm && unvisited[tperm])
		{
		if (pfound+1>max_perm)
			{
			printf("Reached a point in the code that should be impossible!\n");
			exit(EXIT_FAILURE);
			}
		else if (pfound+1==max_perm)
			{
			writeCurrentString(nBest[tot_bl]==0,pos+1);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);
			nBest[tot_bl]++;
			};
			
		unvisited[tperm]=FALSE;
		if (ocpTrackingOn)
			{
			int prevC=0, oc=0;
			oc=oneCycleIndices[tperm];
			prevC = oneCycleCounts[oc]--;
			oneCycleBins[prevC]--;
			oneCycleBins[prevC-1]++;
		
			fillStr2(pos+1, pfound+1, tperm>>DBITS, remapDigits, bestStr+1, len-1);
		
			oneCycleBins[prevC-1]--;
			oneCycleBins[prevC]++;
			oneCycleCounts[oc]=prevC;
			}
		else
			{
			fillStr2(pos+1, pfound+1, tperm>>DBITS, remapDigits, bestStr+1, len-1);
			};
		unvisited[tperm]=TRUE;
		}
	else if	(alreadyWasted < tot_bl)
		{
		if	(((!vperm) || allowRepeats) && pruneOnPerms(tot_bl - (alreadyWasted+1), pfound - max_perm) >=0)
			{
			fillStr2(pos+1, pfound, tperm>>DBITS, remapDigits, bestStr+1, len-1);
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
fp = fopen(outputFileName,newFile?"wt":"at");
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
CHECK_MEM( bestStrings[w] = (char *)malloc(bestLen[w]*nBest[w]*sizeof(char)) )

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

//	Cycle a string of integers so the lowest comes first

void rClassMin(int *p, int n)
{
int min=p[0], km=0;
for (int k=1;k<n;k++) if (p[k]<min) {min=p[k]; km=k;};

for (int r=0;r<km;r++)
	{
	int tmp=p[0];
	for (int i=1;i<n;i++)
		{
		p[i-1]=p[i];
		};
	p[n-1]=tmp;
	};
}

//	With w characters available to waste, can we visit enough new permutations to match or increase max_perm?
//
//	We have one upper bound on the new permutations in mperm_res[w], and another we can calculate from the numbers of 1-cycles with various
//	counts of unvisited permutations.

//	We add the smaller of these bounds to d0, which is count of perms we've already seen, minus max_perm
//	(or if calculating, we return as soon as the sign of the sum is determined)

int pruneOnPerms(int w, int d0)
{
int res = d0 + mperm_res[w];
if (ocpTrackingOff || res < 0) return res;
int res0 = d0;
w++;				//	We have already subtracted waste characters needed to reach first permutation, so we get the first 1-cycle for free

//	oneCycleBins[b] contains count of how many 1-cycles have b unvisited permutations, where b is from 0 to n.

for (int b=n;b>0;b--)
	{
	int ocb = oneCycleBins[b];
	if (w<=ocb)
		{
		res0+=w*b;
		if (res0 >= res) return res;
		else
			{
			MONITOR_OCP
			return res0;
			};
		}
	else
		{
		res0+=ocb*b;
		w-=ocb;
		};
	if (res0 >= res) return res;
	if (res0 > 0)
		{
		MONITOR_OCP
		return res0;
		};
	};
MONITOR_OCP
return res0;
}

void printDigits(int t)
{
printf("t=%d\n",t);
for (int k=0;k<n;k++)
	{
	printf("%c",'0'+(t&7));
	t=t>>DBITS;
	};
printf("\n");
}

//	Given the state of the unvisited[] flags (plus we have arrived at tperm, not yet flagged)
//	how many permutations can we get by following a single weight-2 edge, and then as many weight-1 edges
//	as possible before we hit a permutation already visited.

void maybeUpdateLowerBound(int tperm, int size, int w, int p)
{
int unv[MAX_N+1];

unvisited[tperm]=FALSE;

//	Follow weight-2 edge

int t=successor2[tperm];

//	Follow successive weight-1 edges

int nu=0, okT=0;
while (unvisited[t])
	{
	unv[nu++]=t;		//	Record, so we can unroll
	unvisited[t]=FALSE;	//	Mark as visited
	okT = t;			//	Record the last unvisited permutation integer
	t=successor1[t];
	};
	
int m = p + nu;
if (nu > 0 && m > mperm_res[w+1])
	{
	printf("[Updated lower bound for w=%d to %d]\n",w+1,m);
	mperm_res[w+1] = m;
	
	curstr[size] = curstr[size-(n-1)];
	curstr[size+1] = curstr[size-n];
	for (int j=0;j<nu-1;j++) curstr[size+2+j] = curstr[size-(n-2)+j];

	for (int k=0;k<size+nu+1;k++) klbStrings[w+1][k]=curstr[k];
	klbLen[w+1] = size+nu+1;
	
	maybeUpdateLowerBound(okT, klbLen[w+1], w+1, m);
	};

for (int i=0;i<nu;i++) unvisited[unv[i]]=TRUE;
unvisited[tperm]=TRUE;
}
