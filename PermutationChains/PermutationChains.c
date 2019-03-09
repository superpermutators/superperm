//
//  PermutationChains.c
//
//  Created by Greg Egan on 24 February 2019.
//
//	V1.1	25 February 2019	Added option for Robin Houston's non-standard kernels
//	V1.2	 3 March 2019		Allow use of full stabiliser subgroup for non-standard kernels
//	V1.3	 9 March 2019		Sped up symmPairs with fullSymm by excluding 2-cycles that would connect paired trees

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*

This program searches for superpermutations with a certain kind of structure.

The algorithm used is based on the approach described by Bogdan Coanda here:

	https://groups.google.com/d/msg/superpermutators/KNhmzQy99ic/obl6pCt5HwAJ

but any errors are entirely my own.

See the accompanying file Readme.txt for a user's guide.

Contents:

Macros and parameters
Data structure definitions
Global variables
Basic utility functions
Symmetry operations
Loop operations
Search operations
Nonstandard kernel specification
Main program

*/

//	=====================
//	Macros and parameters
//	=====================

//	Largest and smallest n value permitted

#define MIN_N 5
#define MAX_N 9

#define TRUE (1==1)
#define FALSE (1==0)

//	To print debugging information to the console,
//	set DEBUG_PC to TRUE, and then switch debug on/off as necessary to narrow
//	in on any problem.
//
//	(If debug is left at TRUE for an entire search, the output can run into gigabytes ...)

#define DEBUG_PC FALSE
#if DEBUG_PC
static int debug=FALSE;
#endif

//	Memory allocation check/fail

#define CHECK_MEM(p) if ((p)==NULL) {printf("Insufficient memory\n"); exit(EXIT_FAILURE);};

//	Macros for handling bit sets spread over several unsigned integers

typedef unsigned long int PIECE;
#define bytesPerPiece (sizeof(PIECE))
#define bitsPerPiece (8*bytesPerPiece)

//	Zero a sequence of pieces

#define ZERO(p,n) for (int zp=0;zp<n;zp++) *(p+zp)=0;

//	Set a bit in a block of pieces

#define SETBIT(p,b) *(p+(b)/bitsPerPiece) |= (((PIECE)1)<<((b)%bitsPerPiece));

//	Get a bit in a block of pieces

#define GETBIT(p,b) (((*(p+(b)/bitsPerPiece)) & (((PIECE)1)<<((b)%bitsPerPiece))) ? 1 : 0)

//	==========================
//	Data structure definitions
//	==========================

//	Lists of neighbours for each vertex in the 2-cycle graph
//	--------------------------------------------------------

//	There is a single such structure for the whole 2-cycle graph.

struct neighbours
{
int **vsi;				//	Lists of single-intersection vertex numbers for each vertex
int *nvsi;				//	Number of single-intersection neighbours for each vertex
int **vdi;				//	Lists of double-intersection vertex numbers for each vertex
int *nvdi;				//	Number of double-intersection neighbours for each vertex
};

struct loop;			//	Referred to by struct oneCycle, so provisional definition required

//	Data associated with each 1-cycle
//	---------------------------------

struct oneCycle
{
int ocNumber;		//	The 1-cycle's index number in the list.

//	A list of 2-cycle index numbers associated with a weight-2 edge starting from each of
//	the n permutations in this 1-cycle.
//
//	The first permutation in this list is the canonical order for the 1-cycle,
//	and then successive ones are left-shifted.

int *twoCycleNumbers;

//	A list of status codes for each of the n permutations in this 1-cycle:
//
//	0: we are free to put a 2-edge here (but have not yet done so)
//	1: we have actually put a 2-edge here
//	2: we are blocked from putting a 2-edge here, because we have done so to one of its cyclic neighbours

int *edgeStatus;

//	The loop object that owns this 1-cycle

struct loop *ownerLoop;
};

//	Data associated with each loop of permutations
//	----------------------------------------------

struct loop
{
//	flag saying we have temporarily assigned this loop to a larger loop

int tempFlag;

//	parentLoop is NULL if there is no larger loop into which this has been incorporated;
//	otherwise it points upwards in the tree of loops.

struct loop *parentLoop;

//	Links in linked list of sibling loops, i.e. those sharing a common parentLoop

struct loop *nextSib, *prevSib;

//	Link to one child; from there, others are found in linked list of its siblings.

struct loop *firstChild;

//	Number of (loop) children this loop contains; a single 1-cycle isn't counted as a child loop.

int childCount;

//	Count of free edges in any of the 1-cycles ultimately belonging to this loop

int freeCount;

//	For the very lowest level loop, the 1-cycle it contains;
//	otherwise NULL

struct oneCycle *oc;

//	Links in linked list of all top-level loops.  This is independent of the nextSib/prevSib
//	linked list (and is only used by top-level loops, which have no siblings in the loop hierarchy.)

struct loop *nextTop, *prevTop;
};

//	================
//	Global variables
//	================

//	Options set on the command line
//	-------------------------------

static int n;						//	The number of digits we are permuting

//	How much to print to console

static int verbose=FALSE;			//	Give lots of details of initial setup
static int showSols=FALSE;			//	Show details of each solution as it's found
static int trackPartial=FALSE;		//	Show each new largest partial solution

//	Choice of kernel

static int ffc=FALSE;				//	Use the first 4-cycle as the kernel (default is first 3-cycle)
static int nsk=FALSE;				//	Use a non-standard kernel

static int nskCount=0;				//	Number of entries in non-standard kernel specifier
static int *nskSpec=NULL;			//	Non-standard kernel specifier.
									//	This consists of a sequence of digits from 1 to n-1
									//	giving the number of one-cycles to traverse, joined by weight-2 edges,
									//	before then using a weight-3 edge.  A code of -1 indicates a weight-4
									//	edge.

//	Choice of anchor points on kernel

static int lastAnchor=FALSE;		//	Only anchor at the last 2-cycle (and its orbit)
static int fixedPointAnchor=FALSE;	//	Anchor at fixed points of the orbit on the kernel

//	Choice of orbit type

static int stabiliser=FALSE;		//	Use the full stabiliser group of the kernel
static int limStab=FALSE;			//	Use the stabiliser of a subset of the kernel
static int symmPairs=FALSE;			//	Use a 2-fold symmetry that preserves the kernel
static int littleGroup=FALSE;		//	Use a subgroup of the stabiliser of order n-2
static int blocks=FALSE;			//	Use cyclic blocks of size n-2 (not orbits of any symmetry)

static int useOrbits=FALSE;			//	Set TRUE if any orbits option is chosen

//	Details of how we handle orbits

static int fixedPoints=FALSE;		//	Include single-point orbits
static int fullSymm=FALSE;			//	Require the solution to consist entirely of orbits

//	Maybe filter solutions

static int treesOnly=FALSE;			//	Filter solutions to only output strict trees in the 2-cycle graph

//	File names
//	----------

#define MAX_FILENAME 512
static char nskString[MAX_FILENAME];
static char optionsDescription[MAX_FILENAME];
static char superPermsOutputFile[MAX_FILENAME];
static char twoCyclesOutputFile[MAX_FILENAME];
static char flagsFile[MAX_FILENAME];

static int superPermsOutputFileOpened=FALSE;
static int twoCyclesOutputFileOpened=FALSE;

//	Flag data parameters
//	--------------------

//	piecesPerBitString is the number of pieces needed to cover a set of flags for all
//	2-cycles; this is set once n is known.
//
//	piecesPerRecord is the number of pieces in the full record for each 2-cycle, which includes
//	more than one flag set; this is set once n is known.

static int piecesPerBitString=0;
static int piecesPerRecord=0;

//	Permutations, 1-cycles, 2-cycles etc.
//	-------------------------------------

//	Factorials of n, n-1, n-2, n-3, n-4

static int fn, fn1, fn2, fn3, fn4;

//	A list of permutation lists

static int **permTab;

//	Lists of permutations on n, n-1, n-2, n-3 symbols

static int *p0=NULL, *p1=NULL, *p2=NULL, *p3=NULL;

//	The weight-1, weight-2, weight-3 and weight-4 successors of each permutation

static int *w1s=NULL, *w2s=NULL, *w3s=NULL, *w4s=NULL;
static int **w1234s=NULL;

//	Number of 1-cycles

static int n1C;

//	List of all 1-cycles

static int *oneCycles=NULL;

//	Number of 2-cycles

static int nTC;

//	List of all 2-cycles

static int *twoCycles=NULL;

//	Number of standard 2-cycles in the kernel

static int nSTC;

//	Standard 2 cycles as digits

static int *STC=NULL;

//	List of 2-cycle index numbers, for the nSTC standard 2-cycles in the kernel

static int *stcIndex=NULL;

//	Inverse of this:  for each 2-cycle, its position in the kernel, or -1

static char *stcInverse=NULL;

//	Flags for when the two-cycles in the kernel are incomplete

static char *stcIncomplete=NULL;

//	Offsets and one-cycle counts for the incomplete 2-cycles in the kernel

static int *stcOffs=NULL, *stcNOC=NULL;

//	The loop that takes in the wole kernel

struct loop *stcLoop=NULL;

//	A list of permutations in the kernel that are followed by edges of weight 3 or 4

static int *kernelW34Perms=NULL;

//	The associated weights

static int *kernelW34Weights=NULL;

//	Incidence information
//	---------------------

//	Flag sets for each 2-cycle, describing their incidence relations with other 2-cycles

static PIECE *tcFlags=NULL;

//	Adjacency matrix for 2-cycles graph

static char *tcMat=NULL;

//	Lists of single and double intersection neigbours for each 2-cycle

static struct neighbours tcN;

//	List of the 1-cycles in each 2-cycle

static int *oneForTwo=NULL;

//	For each 2-cycle and each 1-cycle it contains, specify which permutation the 2-cycle
//	has its weight-2 edge from.

static int *oneForTwoStartPerms=NULL;

//	For each 1-cycle, the index numbers of the permutations it contains

static int *permsForOne=NULL;

//	Non-standard kernel details
//	---------------------------

static int nskNOC=0;				//	Count of 1-cycles covered by kernel
static int nskNOP=0;				//	Count of permutations covered by kernel
static int nskScore=0;				//	Score for the kernel	
static int *nskPerms=NULL;			//	List of all permutations
static int *nskAuto=NULL;			//	Automorphism for symmetry of non-standard kernels
static int nskPalindrome=FALSE;		//	Is the kernel palindromic?

//	Orbit details
//	-------------

//	The total number of orbits created

static int nOrbits=0;

//	For each 2-cycle, the index numbers of the various orbits it belongs to
//	(there can be more than one if we allow multiple orbit schemes at the same time)

static int **orbitNumbersTC=NULL;

//	For each 2-cycle, the number of different orbits it belongs to

static int *numOrbitsTC=NULL;

//	For each orbit, the number of 2-cycles it contains

static int *orbitSizes=NULL;

//	For each orbit, the list of 2-cycles it contains

static int **orbits=NULL;

//	For each orbit, a flag specifying whether it has been classified OK to use

static char *orbitOK=NULL;

//	The number of orbits classified as OK to use

static int nOrbitsOK=0;

//	A list of all orbits classied as OK to use
	
static int *okOrbits=NULL;

//	Solution in progress
//	--------------------

//	An array of oneCycle structures, for every 1-cycle

static struct oneCycle *oneCycleTable=NULL;

//	A pool of loop structures, preallocated and used as needed

static struct loop *loopTable=NULL;

//	A dummy loop which acts as a header in the linked list of top-level loops

static struct loop loopHeader;

//	The total number of preallocated loops in loopTable

static int loopTableSize=0;

//	The number of loops that are currently in use in loopTable.

static int loopTableUsed=0;

//	The current number of top-level loops

static int topLevelLoopCount=0;

//	A list of the 2-cycles making contact with a loop, and a count of them

static int *loopContacts=NULL, nLoopContacts=0;

//	A list of any 2-cycles making multiple contact with a loop, and a count of them

static int *multiContacts=NULL, nMultiContacts=0;

//	A stack of 2-cycles excluded for having multiple contacts with a loop;
//	these are recorded explicitly so the exclusions are easy to unwind when the loop
//	is dissolved.

static int *exclusionsStack=NULL, nExclusionsStack=0;

//	The number of 2-cycles still available to be used in the solution

static int PCpoolSize=0;

//	An array of exclusion counts for each 2-cycle;
//	when this is non-zero, the 2-cycle can no longer be added to the solution.

static int *PCexclusions=NULL;

//	An array of contact counts for each 2-cycle;
//	(these are used in temporary calculations, not maintained long-term)

static int *PCcontacts=NULL;

//	The number of orbits still available to be used in the solution

static int PCavailOrbits=0;

//	An array of exclusion counts for each orbit;
//	when this is non-zero, the orbit can no longer be added to the solution.

static int *PCorbitExclusions=NULL;

//	For each orbit, a count of the number of slots that have been used;
//	i.e. individual 2-cycles that belong to the orbit.

static int *PCorbitSlots=NULL;

//	For each orbit, a count of "blocks" placed on it;
//	this is used to block orbits from being re-used by individual 2-cycles,
//	as distinct from the exclusions on whole orbits in PCorbitExclusions

static int *PCorbitBlocks=NULL;

//	The total number of such blocks

static int nPCorbitBlocks=0;

//	The number of 2-cycles a complete solution should contain

static int PCsolTarget=0;

//	The size of the current solution in progress

static int PCsolSize=0;

//	The size of the largest partial solution seen so far

static int PClargestSolSeen=0;

//	The number of complete solutions found

static int PCsolCount=0;

//	The number of provisional solutions found;
//	this will count any that fail to contain the correct number of connected
//	components, or (if "treesOnly" is selected) fail to be trees

static int PCprovCount=0;

//	A list of 2-cycles in the solution

static int *PCsol=NULL;

//	A list of orbit numbers that these 2-cycles came from, or -1 if individual 2-cycles

static int *PCorbit=NULL;

//	Lists of offsets and counts, to allow for incomplete 2-cycles

static int *PCoffs=NULL, *PCcount=NULL;

//	=======================
//	Basic utility functions
//	=======================

//	Factorial

int factorial(int n)
{
if (n==0) return 1; else return n*factorial(n-1);
}

//	Compare two integer sequences (for qsort, bsearch)
//	The size needs to be set in nCompareInt

static int nCompareInt;
int compareInt(const void *ii0, const void *jj0)
{
int *ii=(int *)ii0, *jj=(int *)jj0;
for (int k=0;k<nCompareInt;k++)
	{
	if (ii[k] < jj[k]) return -1;
	else if (ii[k] > jj[k]) return 1;
	};
return 0;
}

//	Compare single integers (for qsort, bsearch)

int compare1(const void *ii0, const void *jj0)
{
int *ii=(int *)ii0, *jj=(int *)jj0;
if (*ii < *jj) return -1;
if (*ii > *jj) return 1;
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

//	Generate all permutations of 1 ... n;
//	generates lists for lower values of n
//	in the process.
//
//	We sort each table into lexical order after constructing it.

void makePerms(int n, int **permTab)
{
int fn=factorial(n);
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
	int pf=factorial(n-1);
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
	nCompareInt=n;
	qsort(res,fn,n*sizeof(int),compareInt);
	};

*permTab = res;
}

//	Print a single integer sequence

void printInt(FILE *out, int *ii, int n, char *eos)
{
fprintf(out,"{");
for (int i=0;i<n;i++) fprintf(out,"%d%s",ii[i],i==n-1?"":",");
fprintf(out,"}%s",eos);
}

//	Print a block of integer sequences

void printBlock(FILE *out, int *b, int n, int s)
{
fprintf(out,"{");
for (int i=0;i<s;i++) printInt(out,b+i*n,n,i==s-1?"}\n":",\n");
}

//	Print a block of integer sequences, numbering them from 0

void printBlockN(FILE *out,int *b, int n, int s)
{
for (int i=0;i<s;i++)
	{
	fprintf(out,"%4d ",i);
	printInt(out,b+i*n,n,"\n");
	};
}

//	Find an integer sequence in a sorted block

int searchBlock(int *base, int n, int s, int *target)
{
nCompareInt=n;
int *res = (int *) bsearch(target, base, s, n*sizeof(int), compareInt);
if (res) return (int)(res-base)/n;
else return -1;
}

//	Search the permutations

int searchPermutations(int *target, int n)
{
return searchBlock(p0, n, fn, target);
}

//	Search the 2-cycles

int searchTwoCycles(int *target, int n)
{
return searchBlock(twoCycles, n, nTC, target);
}

//	Search the 1-cycles

int searchOneCycles(int *target, int n)
{
return searchBlock(oneCycles, n, n1C, target);
}

//	Every 2-cycle comprises a list of (n-1) 1-cycles.
//	Our convention is that, m | q includes the n-1 cyclic classes of m q, m rot_1(q) m rot_2(q) etc.

void oneCycleList(int tc, int *res)
{
static int r1[MAX_N];
for (int r=0;r<n-1;r++)
	{
	r1[0] = *(twoCycles+tc*n);
	for (int z=0;z<n-1;z++) r1[1+z] = *(twoCycles+tc*n+1+(z+r)%(n-1));
	rClassMin(r1,n);
	res[r]=searchOneCycles(r1,n);
	if (res[r]<0)
		{
		printf("Can't find one-cycles for 2-cycle %d\n",tc);
		exit(EXIT_FAILURE);
		};
	};
}

//	The weight 1 successor of a permutation:  left rotation

void successor1(int *perm, int *s, int n)
{
for (int k=0;k<n;k++) s[k]=perm[(k+1)%n];
}

//	The weight 1 predecessor of a permutation:  right rotation

void predecessor1(int *perm, int *s, int n)
{
for (int k=0;k<n;k++) s[k]=perm[(k+n-1)%n];
}

//	The weight 2 successor of a permutation:
//	two left rotations then swap last two elements

void successor2(int *perm, int *s, int n)
{
for (int k=0;k<n;k++) s[k]=perm[(k+2)%n];
int tmp=s[n-1];
s[n-1]=s[n-2];
s[n-2]=tmp;
}

//	The weight 2 predecessor of a permutation:
//	two right rotations then swap first two elements

void predecessor2(int *perm, int *s, int n)
{
for (int k=0;k<n;k++) s[k]=perm[(k+n-2)%n];
int tmp=s[0];
s[0]=s[1];
s[1]=tmp;
}

//	The (non-unique) weight 3 successor of a permutation, which occurs in the standard superpermutation:
//	three left rotations, then reverse the last 3 elements

void successor3(int *perm, int *s, int n)
{
for (int k=0;k<n-3;k++) s[k]=perm[k+3];
for (int k=n-3;k<n;k++) s[k]=perm[n-1-k];
}

//	The (non-unique) weight 4 successor of a permutation, which occurs in the standard superpermutation:
//	four left rotations, then reverse the last 4 elements

void successor4(int *perm, int *s, int n)
{
for (int k=0;k<n-4;k++) s[k]=perm[k+4];
for (int k=n-4;k<n;k++) s[k]=perm[n-1-k];
}

//	Given a permutation index number, find the 2-cycle where this permutation appears at the start
//	of a 1-cycle (i.e. just after an edge of weight 2) and the offset from the notional
//	start of the 2-cycle, which is the first 1-cycle in oneForTwo[]

int twoCycleFromPerm(int perm, int *offs)
{
static int pred[MAX_N], pred2[MAX_N], oc[MAX_N];

//	Get the weight-2 predecessor of this permutation

predecessor2(p0+n*perm,pred,n);

//	This will be one of the permutations in the 2-cycle followed by a weight-2 edge

for (int k=0;k<n;k++)
	{
	pred2[k]=pred[k];
	oc[k]=p0[n*perm+k];
	};

//	Put a copy in canonical form to find the 2-cycle

rClassMin(pred2+1,n-1);
int tc = searchTwoCycles(pred2,n);
if (tc<0)
	{
	printf("Unable to find the 2-cycle containing permutation #%d\n",perm);
	exit(EXIT_FAILURE);
	};
	
//	Find the offset from the start of the 2-cycle in oneForTwo[]

rClassMin(oc,n);
int oci = searchOneCycles(oc,n);
if (oci<0)
	{
	printf("Unable to find the 1-cycle containing permutation #%d\n",perm);
	exit(EXIT_FAILURE);
	};
	
*offs=-1;
for (int z=0;z<n-1;z++)
	{
	if (oneForTwo[tc*(n-1)+z]==oci)
		{
		*offs=z;
		break;
		};
	};

if (*offs < 0)
	{
	printf("Unable to find the 1-cycle #%d in the 2-cycle #%d, though both supposedly contain permutation #%d\n",
		oci,tc,perm);
	};

return tc;
}

//	Intersection type between two 2-cycles
//	We identify each m|q (m is "missing", q is modulo rotations) with the 2-cycle which has a weight-2 permutation graph edge starting from
//	m|rot(q) for any rotation of q.
//
//	Returns:
//
//	0 if the 2-cycles are entirely disjoint
//	1 if the 2-cycles have a single 1-cycle in common
//	2 if the 2-cycles have two 1-cycles in common
//	3 if the 2-cycles are identical
//
//	The q in m|q is assumed to have been rotated by rClassMin so the minimum entry comes first


int intType(int *pm1, int *pm2, int n)
{
nCompareInt=n;

if (compareInt(pm1,pm2)==0) return 3;
int m1=pm1[0], m2=pm2[0];
if (m1==m2) return 0;

//	Locate m1 in q2 and m2 in q1

int km1=-1, km2=-1;
for (int k=0;k<n-1;k++) if (pm1[1+k]==m2) {km2=k; break;};
for (int k=0;k<n-1;k++) if (pm2[1+k]==m1) {km1=k; break;};
if (km1<0 || km2<0 || pm1[1+km2]!=m2 || pm2[1+km1]!=m1)
	{
	printf("Error in intType()\n");
	printInt(stdout,pm1,n,"\n");
	printInt(stdout,pm2,n,"\n");
	exit(EXIT_FAILURE);
	};

for (int offset=0;offset<n-2;offset++)
	{
	int match=TRUE;
	for (int d=0;d<n-2;d++)
		{
		if (pm1[1+(km2+1+d)%(n-1)]!=pm2[1+(km1+1+(offset+d)%(n-2))%(n-1)])
			{
			match=FALSE;
			break;
			};
		};
	if (match)
		{
		if (offset==0) return 2;
		else return 1;
		};
	};
return 0;
}

//	Check that all the two-cycles in a set are mutually disjoint

int selfDisj(int *tcnums, int nt)
{
for (int a=0;a<nt;a++)
for (int b=a+1;b<nt;b++)
	{
	if (GETBIT(tcFlags+(tcnums[a])*piecesPerRecord,tcnums[b])==0) return FALSE;
	};
return TRUE;
}

//	Check two sets of 2-cycles to see if they are disjoint

int disjSets(int *tcnums1, int nt1, int *tcnums2, int nt2)
{
for (int a=0;a<nt1;a++)
for (int b=0;b<nt2;b++)
	{
	if (GETBIT(tcFlags+(tcnums1[a])*piecesPerRecord,tcnums2[b])==0) return FALSE;
	};
return TRUE;
}

//	Split a list of 2-cycles, given as index numbers, into connected components

int splitCC(int *tcl, int tcn, int *ccOut, int *ccOffs, int *ccSize)
{
static char *used=NULL;
if (used==NULL)
	{
	CHECK_MEM( used=(char *)malloc(nTC*sizeof(char)) )
	};
for (int z=0;z<tcn;z++) used[z]=FALSE; 

int cc=0;
int offs=0;
int poolSize=tcn;
while (poolSize!=0)
	{
	//	Start a new connected component with the first unused 2-cycle
	
	int csize=0;
	
	int f;
	for (f=0;f<tcn;f++) if (!used[f]) break;
	if (f==tcn) break;
	
	//	Put that unused 2-cycle into the new CC
	
	ccOffs[cc]=offs;

	used[f]=TRUE;
	ccOut[offs]=tcl[f];
	
	offs++;
	poolSize--;
	csize++;
	
	while (poolSize!=0)
		{
		int addedNew=FALSE;
		int g;
		for (g=0;g<tcn;g++)
		if (!used[g])
			{
			int conn=FALSE;
			for (int r=0;r<csize;r++)
				{
				if (tcMat[ccOut[ccOffs[cc]+r]*nTC+tcl[g]]!=0)
					{
					conn=TRUE;
					break;
					};
				};
			if (conn)
				{
				used[g]=TRUE;
				ccOut[offs]=tcl[g];
				offs++;
				poolSize--;
				csize++;
				addedNew=TRUE;
				};
			};
		if (!addedNew) break;
		};
		
	ccSize[cc]=csize;
	cc++;
	};
return cc;
}

//	Check a collection of 2-cycles (given as index numbers) to see if they comprise a single, connected tree:
//	* There are no double intersections between the 2-cycles
//	* The number of edges between them (the number of single intersections) total one more than the number of 2-cycles
//	* No 2-cycle appears twice in the list

int isTree(int *tcnums, int nlist, int n)
{
int si=0, di=0;
for (int a=0;a<nlist-1;a++)
	{
	int tc1 = tcnums[a];
	for (int b=a+1;b<nlist;b++)
		{
		int tc2 = tcnums[b];
		if (tc1==tc2) return FALSE;
		if (GETBIT(tcFlags+tc1*piecesPerRecord+piecesPerBitString,tc2)) si++;
		else if (GETBIT(tcFlags+tc1*piecesPerRecord+2*piecesPerBitString,tc2)) di++;
		};
	};
return di==0 && si==nlist-1;
}

int setupNeighbours(struct neighbours *nn, char *imat, int nmat, int verbose)
{
//	For each vertex number, get a list of its single- and double-intersection neighbours

CHECK_MEM( nn->vsi = (int **)malloc(nmat*sizeof(int *)) )
CHECK_MEM( nn->nvsi = (int *)malloc(nmat*sizeof(int)) )

CHECK_MEM( nn->vdi = (int **)malloc(nmat*sizeof(int *)) )
CHECK_MEM( nn->nvdi = (int *)malloc(nmat*sizeof(int)) )

int maxSI = 0, maxDI = 0, minSI = nmat, minDI = nmat;
for (int i=0;i<nmat;i++)
	{
	nn->nvsi[i]=0;
	nn->vsi[i]=NULL;
	nn->nvdi[i]=0;
	nn->vdi[i]=NULL;
	int count1=0, count2=0;
	for (int j=0;j<nmat;j++)
		{
		if (imat[i*nmat+j]==1) count1++;
		if (imat[i*nmat+j]==2) count2++;
		};
		
	if (count1!=0)
		{
		nn->nvsi[i]=count1;
		
		nn->vsi[i]=(int *)malloc(nn->nvsi[i]*sizeof(int));
		CHECK_MEM( nn->vsi[i] )
		
		count1=0;
		for (int j=0;j<nmat;j++)
			{
			if (imat[i*nmat+j]==1)
				{
				nn->vsi[i][count1++]=j;
				};
			};
		if (count1 < minSI) minSI = count1;
		if (count1 > maxSI) maxSI = count1;
		};
		
	if (count2!=0)
		{
		nn->nvdi[i]=count2;
		
		nn->vdi[i]=(int *)malloc(nn->nvdi[i]*sizeof(int));
		CHECK_MEM( nn->vdi[i] )
	
		count2=0;
		for (int j=0;j<nmat;j++)
			{
			if (imat[i*nmat+j]==2)
				{
				nn->vdi[i][count2++]=j;
				};
			};
		if (count2 < minDI) minDI = count2;
		if (count2 > maxDI) maxDI = count2;
		};
	};
	
printf("The number of single-intersection neighbours ranges from %d to %d\n",minSI,maxSI);
printf("The number of double-intersection neighbours ranges from %d to %d\n",minDI,maxDI);

if (verbose)
	{
	for (int i=0;i<nmat;i++)
	if (nn->nvsi[i]!=0)
		{
		printf("SI neighbours of %d are: ",i);
		for (int j=0;j<nn->nvsi[i];j++) printf("%d ",nn->vsi[i][j]);
		printf("\n");
		};
	for (int i=0;i<nmat;i++)
	if (nn->nvdi[i]!=0)
		{
		printf("DI neighbours of %d are: ",i);
		for (int j=0;j<nn->nvdi[i];j++) printf("%d ",nn->vdi[i][j]);
		printf("\n");
		};
	};
	
return maxSI;
}

void freeNeighbours(struct neighbours *nn, int nmat)
{
for (int i=0;i<nmat;i++)
	{
	free(nn->vsi[i]);
	free(nn->vdi[i]);
	};
free(nn->vsi);
free(nn->vdi);
free(nn->nvsi);
free(nn->nvdi);
}

//	Given a list of 2-cycle index numbers, offsets and counts in the solution arrays
//	output the corresponding superpermutation

void printSuperPerm(FILE *f)
{
static int *ps=NULL;
if (ps==NULL)
	{
	CHECK_MEM( ps = (int *)malloc(fn*sizeof(int)) )
	};
	
//	First, mark every permutation as having a default successor of weight 1

for (int i=0;i<fn;i++) ps[i]=1;

//	Next, modify this according to the list of 2-cycles

for (int i=0;i<PCsolSize;i++)
	{
	int tc=PCsol[i];
	for (int j0=0;j0<PCcount[i];j0++)
		{
		int j = (PCoffs[i]+j0) % (n-1);
		int oc = oneForTwo[tc*(n-1)+j];
		int s = oneForTwoStartPerms[tc*(n-1)+j];
		int p = permsForOne[oc*n+s];
		ps[p]=2;
		};
	};
	
//	Finally, include the weight-3 and weight-4 edges from the kernel

for (int i=0;i<nSTC-1;i++)
	{
	ps[kernelW34Perms[i]] = kernelW34Weights[i];
	};

//	Now start at permutation 0 and following the successors

for (int i=1;i<=n;i++) fprintf(f,"%d",i);

int p=0;
for (int k=0;k<fn-1;k++)
	{
	int w = ps[p];
	p = w1234s[w][p];
	for (int z=0;z<w;z++) fprintf(f,"%d",p0[p*n+n-w+z]);
	};
fprintf(f,"\n");
}

//	===================
//	Symmetry operations
//	===================

//	Act with a general automorphism on a 2-cycle

//	Each automorphism is described by (n+1) integers, with the first being +1/-1 with -1 for reversal,
//	and the remaining n integers being a digit map, giving the substitute digits for 1...n.

int actAutoTC(int tc, int *autoMorphism)
{
static int swapped[MAX_N], rev[MAX_N];

//	Use the automorphism permutation to swap the digits

for (int z=0;z<n;z++)
	{
	int d=*(twoCycles+n*tc+z);
	swapped[z]=autoMorphism[d];
	};
	
//	If specified, reverse the (last n-1) digits and put in canonical cyclic order
	
if (autoMorphism[0]<0)
	{
	rev[0]=swapped[0];
	for (int z=1;z<n;z++) rev[z]=swapped[n-z];
	rClassMin(rev+1,n-1);
	return searchTwoCycles(rev,n);
	};
	
//	Unswapped version
	
rClassMin(swapped+1,n-1);
return searchTwoCycles(swapped,n);
}

//	Act with a general automorphism on a permutation

int actAutoP(int p, int *autoMorphism)
{
static int swapped[MAX_N], rev[MAX_N];

//	Use the automorphism permutation to swap the digits

for (int z=0;z<n;z++)
	{
	int d=*(p0+n*p+z);
	swapped[z]=autoMorphism[d];
	};
	
//	If specified, reverse all the digits
	
if (autoMorphism[0]<0)
	{
	for (int z=0;z<n;z++) rev[z]=swapped[n-1-z];
	return searchPermutations(rev,n);
	};
	
//	Unswapped version
	
return searchPermutations(swapped,n);
}

//	For n=5, swap 1 and 3, and reverse the last n-1 digits

int fiveSymm(int tc, int n)
{
static int swapped[MAX_N], rev[MAX_N];
for (int z=0;z<n;z++)
	{
	int d=*(twoCycles+n*tc+z);
	     if (d==1) swapped[z]=3;
	else if (d==3) swapped[z]=1;
	else swapped[z]=d;
	};
rev[0]=swapped[0];
for (int z=1;z<n;z++) rev[z]=swapped[n-z];
rClassMin(rev+1,n-1);
return searchTwoCycles(rev,n);
}

//	For n=7, first 3-cycle, swap 1 and 2, swap 3 and 5, and reverse the last n-1 digits
//	For n=7, first 4-cycle, swap 1 and 3 and reverse the last n-1 digits

int sevenSymm(int tc, int n)
{
static int swapped[MAX_N], rev[MAX_N];
for (int z=0;z<n;z++)
	{
	int d=*(twoCycles+n*tc+z);
	if (ffc)
		{
			 if (d==1) swapped[z]=3;
		else if (d==3) swapped[z]=1;

		else swapped[z]=d;
		}
	else
		{
			 if (d==1) swapped[z]=2;
		else if (d==2) swapped[z]=1;
		else if (d==3) swapped[z]=5;
		else if (d==5) swapped[z]=3;
		else swapped[z]=d;
		};
	};
rev[0]=swapped[0];
for (int z=1;z<n;z++) rev[z]=swapped[n-z];
rClassMin(rev+1,n-1);
return searchTwoCycles(rev,n);
}

//	For n=8, swap:  1 and 4, 2 and 3, and 5 and 6, and reverse the last n-1 digits
//
//	This corresponds to eightSymm[4,#]& in Mathematica notebook

int eightSymm(int tc, int n)
{
static int swapped[MAX_N], rev[MAX_N];
for (int z=0;z<n;z++)
	{
	int d=*(twoCycles+n*tc+z);
	if (ffc)
		{
			 if (d==1) swapped[z]=4;
		else if (d==2) swapped[z]=3;
		else if (d==3) swapped[z]=2;
		else if (d==4) swapped[z]=1;
		else swapped[z]=d;
		}
	else
		{
			 if (d==1) swapped[z]=4;
		else if (d==4) swapped[z]=1;
		else if (d==2) swapped[z]=3;
		else if (d==3) swapped[z]=2;
		else if (d==5) swapped[z]=6;
		else if (d==6) swapped[z]=5;
		else swapped[z]=d;
		};
	};
rev[0]=swapped[0];
for (int z=1;z<n;z++) rev[z]=swapped[n-z];
rClassMin(rev+1,n-1);
return searchTwoCycles(rev,n);
}

int nineSymm(int tc, int n)
{
static int swapped[MAX_N], rev[MAX_N];
for (int z=0;z<n;z++)
	{
	int d=*(twoCycles+n*tc+z);
	if (ffc)
		{
			 if (d==1) swapped[z]=5;
		else if (d==2) swapped[z]=4;
		else if (d==4) swapped[z]=2;
		else if (d==5) swapped[z]=1;
		else swapped[z]=d;
		}
	else
		{
		printf("Not yet supported\n");
		exit(EXIT_FAILURE);
		};
	};
rev[0]=swapped[0];
for (int z=1;z<n;z++) rev[z]=swapped[n-z];
rClassMin(rev+1,n-1);
return searchTwoCycles(rev,n);
}

int findSymm(int tc, int n)
{
if (nsk && nskPalindrome) return actAutoTC(tc,nskAuto);
if (n==5) return fiveSymm(tc,n);
else if (n==7) return sevenSymm(tc,n);
else if (n==8) return eightSymm(tc,n);
else if (n==9) return nineSymm(tc,n);
else
	{
	printf("n=%d not yet handled by findSymm()\n",n);
	exit(EXIT_FAILURE);
	};
return -1;
}

int getBlockEntry(int qvalence, int a)
{
static int c[MAX_N], r[MAX_N], t[MAX_N], rt[MAX_N], s[MAX_N];
for (int i=0;i<n;i++) c[i]=twoCycles[qvalence*n+i];

int q=c[0];
if (q<1 || q>=n)
	{
	return qvalence;
	};
	
int qp=q+a;
if (qp>=n) qp-=(n-1);

int npos=-1, qpos=-1;
for (int k=1;k<n;k++)
	{
	if (c[k]==n) npos=k;
	else if (c[k]==qp) qpos=k;
	};
	
if (npos<1)
	{
	printf("Can't find n=%d in: ",n);
	printInt(stdout,c,n,"\n");
	exit(EXIT_FAILURE);
	};
if (qpos<1)
	{
	printf("Can't find q+%d=%d in: ",a,qp);
	printInt(stdout,c,n,"\n");
	exit(EXIT_FAILURE);
	};
	
//	Translate the digits {1,...,n-1}\{q,q+1} into {1,...,n-3}

int count=0;
for (int i=1;i<n;i++)
	{
	if (i!=q && i!=qp)
		{
		count++;
		t[i]=count;
		rt[count]=i;
		}
	else {t[i]=i; rt[i]=i;};
	};
t[n]=-1;
t[qp]=0;
t[q]=-2;
rt[0]=qp;
	
if (count != n-3)
	{
	printf("Failed to reach translated digit count of %d, reached %d instead, in: ",n-3,count);
	printInt(stdout,c,n,"\n");
	exit(EXIT_FAILURE);
	};
	
for (int i=0;i<n;i++) r[i]=t[c[i]];

//	Rotate last n-1 digits to put n (translated to -1) first

rClassMin(r+1,n-1);

//	Rotate last n-2 digits to put q+1 (translated to 0) first

rClassMin(r+2,n-2);

//	The remaining digits should be some permutation of 1,...,n-3.

//	Now translate back

for (int i=0;i<n;i++)
	{
	if (r[i]==-1) s[i]=n;
	else if (r[i]==-2) s[i]=q;
	else s[i]=rt[r[i]];
	};
rClassMin(s+1,n-1);
	
int res=searchTwoCycles(s,n);
return res;
}

//	Larger orbits sorted first

int compareOrbitSizes(const void *ii0, const void *jj0)
{
int *ii=(int *)ii0, *jj=(int *)jj0;
if (orbitSizes[*ii] < orbitSizes[*jj]) return 1;
if (orbitSizes[*ii] > orbitSizes[*jj]) return -1;
return 0;
}


//	===============
//	Loop operations
//	===============

//	Initialise a new loop as completely empty
//	* no parent
//	* no siblings
//	* no children
//	* no 1-cycle

void initLoop0(struct loop *lp)
{
lp->tempFlag = FALSE;
lp->parentLoop = NULL;
lp->nextSib = lp->prevSib = lp;
lp->firstChild = NULL;
lp->childCount = 0;
lp->oc = NULL;
lp->freeCount = 0;
}

//	Initialize a new loop from a single 1-cycle
//	* no parent
//	* no siblings
//	* no children
//	* contains a single 1-cycle

void initLoopOC(struct loop *lp, struct oneCycle *oc)
{
lp->tempFlag = FALSE;
lp->parentLoop = NULL;
lp->nextSib = lp->prevSib = lp;
lp->firstChild = NULL;
lp->childCount = 0;
lp->oc = oc;
oc->ownerLoop = lp;
lp->freeCount = n;
}

//	Add a loop to a specified parent, and propagate the added freeCount up the tree

void addToLoop(struct loop *lp, struct loop *childLoop)
{
if (lp==childLoop)
	{
	printf("addToLoop() called trying to add loop to itself\n");
	exit(EXIT_FAILURE);
	};

childLoop->parentLoop = lp;

if (lp->firstChild)
	{
//	Existing children, so link into their sibling list

	childLoop->prevSib = lp->firstChild;
	childLoop->nextSib = lp->firstChild->nextSib;
	lp->firstChild->nextSib = childLoop;
	childLoop->nextSib->prevSib = childLoop;
	lp->childCount++;
	}
else
	{
	lp->firstChild = childLoop;
	lp->childCount = 1;
	childLoop->prevSib = childLoop->nextSib = childLoop;
	};
	
//	Add the freeCount contribution up the tree
	
struct loop *x = lp;
while (x)
	{
	x->freeCount += childLoop->freeCount;
	x = x->parentLoop;
	};
}

//	Dissolve a loop, freeing all the children

void dissolveLoop(struct loop *lp)
{
if (lp->firstChild)
	{
	struct loop *c0 = lp->firstChild, *c=c0, *d;
	while (TRUE)
		{
		c->parentLoop = NULL;
		d = c->nextSib;
		c->nextSib = c;
		c->prevSib = c;
		if (d==c0) break;
		c = d;
		};
	};
lp->firstChild = NULL;
lp->childCount = 0;
}

//	Propagate an increment in a 1-cycle's freeCount

void incFreeOC(struct oneCycle *oc)
{
struct loop *x = oc->ownerLoop;
while (x)
	{
	x->freeCount++;
	x = x->parentLoop;
	};
return;
}

//	Propagate a decrement in a 1-cycle's freeCount

int decFreeOC(struct oneCycle *oc)
{
struct loop *x = oc->ownerLoop, *y=NULL;
while (x)
	{
	x->freeCount--;
	y = x;
	x = x->parentLoop;
	};
return (y->freeCount > 0 || y->tempFlag);
}

//	Get the top loop associated with a 1-cycle

struct loop *topLoop(struct oneCycle *oc)
{
struct loop *x = oc->ownerLoop, *y=NULL;
while (x)
	{
	y = x;
	x = x->parentLoop;
	};
return y;
}

//	Get the top loop associated with a loop

struct loop *topLoopLoop(struct loop *lp)
{
struct loop *x = lp, *y=NULL;
while (x)
	{
	y = x;
	x = x->parentLoop;
	};
return y;
}

#define LINK_LOOP(x) \
	x->nextTop = loopHeader.nextTop; \
	loopHeader.nextTop = x; \
	x->prevTop = &loopHeader; \
	x->nextTop->prevTop = x;

void printLoop(struct loop *lp, int lev, int i, int n)
{
int levsp=3*lev;
for (int k=0;k<levsp;k++) printf(" ");
printf("Loop %d of %d at level %d, freeCount=%d",i,n,lev,lp->freeCount);
if (lp->childCount>0) printf(", children=%d",lp->childCount);
if (lp->oc) printf(", oc=%d",lp->oc->ocNumber);
printf("\n");
int ii=0;
if (lp->firstChild)
	{
	struct loop *c0 = lp->firstChild, *c=c0;
	while (TRUE)
		{
		printLoop(c,lev+1,++ii,lp->childCount);
		c = c->nextSib;
		if (c==c0) break;
		if (ii > lp->childCount)
			{
			printf("Sibling list has not come full circle after expected %d children\n",lp->childCount);
			exit(EXIT_FAILURE);
			};
		};
	};
if (ii != lp->childCount)
	{
	printf("childCount = %d, but %d loops in linked list\n",lp->childCount,ii);
	exit(EXIT_FAILURE);
	};
}

void checkLoopTable()
{
printf("\n%d loop table entries: \n",topLevelLoopCount);
struct loop *a=loopHeader.nextTop;
int ii=0;
while (a != &loopHeader)
	{
	printLoop(a, 0, ++ii, topLevelLoopCount);
	a = a->nextTop;
	};
if (ii != topLevelLoopCount)
	{
	printf("topLevelLoopCount = %d, but %d loops in linked list\n",topLevelLoopCount,ii);
	exit(EXIT_FAILURE);
	};
}

//	=================
//	Search operations
//	=================

void excludeOrbitsTC(int tc)
{
if (useOrbits)
	{
	for (int k=0;k<numOrbitsTC[tc];k++)
		{
		int norb = orbitNumbersTC[tc][k];
		if (PCorbitExclusions[norb]++ == 0) PCavailOrbits--;
		#if DEBUG_PC
		if (debug)
			printf("Exclusion count for orbit %d raised to %d (by 2-cycle %d), PCavailOrbits=%d\n",norb,PCorbitExclusions[norb],tc,PCavailOrbits);
		#endif
		};
	};
}

void unexcludeOrbitsTC(int tc)
{
if (useOrbits)
	{
	for (int k=0;k<numOrbitsTC[tc];k++)
		{
		int norb = orbitNumbersTC[tc][k];
		if (--PCorbitExclusions[norb] == 0) PCavailOrbits++;
		#if DEBUG_PC
		if (debug)
			printf("Exclusion count for orbit %d lowered to %d (by 2-cycle %d), PCavailOrbits=%d\n",norb,PCorbitExclusions[norb],tc,PCavailOrbits);
		#endif
		};
	};
}

//	Exclude a 2-cycle from the permutation chains.
//
//	This is an unused 2-cycle that is being ruled out of the solution,
//	not one that is being used and flagged to stop re-use.
//
//	If we return FALSE, a one-cycle that has not been incorporated into the chains has its freeCount drop to zero,
//	so the solution has become unviable.  We rewind the actions performed within the routine, so there is no
//	need to call unexcludeTCfromPC() after such a return.

int excludeTCfromPC(int t)
{
if (PCexclusions[t]++ == 0)
	{
	//	Was not already excluded, so remove from pool and update 1-cycles
	
	#if DEBUG_PC
	if (debug)
		printf("Excluding 2-cycle %d from pool, leaving %d in pool\n",t,PCpoolSize);
	#endif
	int *oft = oneForTwo+t*(n-1), *ofp = oneForTwoStartPerms+t*(n-1);
	for (int i=0;i<n-1;i++)
		{
		int oc=oft[i];
		int p=ofp[i];
		struct oneCycle *oco = oneCycleTable+oc;
		oco->edgeStatus[p]=2;
		#if DEBUG_PC
		if (debug)
			{
			printf("excludeTCfromPC(%d) Excluded edge %d in 1-cycle %d\n",
				t,p,oc);
			printf("excludeTCfromPC(%d) Free count for 1-cycle %d decreased to %d\n",
				t,oc,oco->ownerLoop->freeCount-1);
			};
		#endif
		if (!decFreeOC(oco))
			{
			#if DEBUG_PC
			if (debug)
				{
				printf("Unwinding unviable exclusion of 2-cycle %d, because loop owning 1-cycle %d hit freeCount 0\n",t,oco->ocNumber);
				checkLoopTable();
				};
			#endif
			for (int j=i;j>=0;j--)
				{
				oc=oft[j];
				p=ofp[j];
				oco = oneCycleTable+oc;
				oco->edgeStatus[p]=0;
				incFreeOC(oco);
				#if DEBUG_PC
				if (debug)
					printf("excludeTCfromPC(%d) Free count for 1-cycle %d increased to %d by freeing edge %d\n",
						t,oc,oco->ownerLoop->freeCount,p);
				#endif
				};
			PCexclusions[t]--;
			return FALSE;
			};
		};
		
	PCpoolSize--;
	excludeOrbitsTC(t);
	};
return TRUE;
}

//	Unexclude a 2-cycle from the permutation chains.
//
//	This only full unexcludes if the exclusion count drops to zero;
//	in that case, the 2-cycle is free and back in the pool for potential use.

void unexcludeTCfromPC(int t)
{
if (--PCexclusions[t] == 0)
	{

	//	Exclusion count dropped to zero, so add back into pool and update 1-cycles
	
	PCpoolSize++;
	#if DEBUG_PC
	if (debug)
		printf("UnExcluding 2-cycle %d back into pool, leaving %d in pool\n",t,PCpoolSize);
	#endif
	int *oft = oneForTwo+t*(n-1), *ofp = oneForTwoStartPerms+t*(n-1);
	for (int i=n-2;i>=0;i--)
		{
		int oc=oft[i];
		int p=ofp[i];
		struct oneCycle *oco = oneCycleTable+oc;
		oco->edgeStatus[p]=0;
		incFreeOC(oco);
		#if DEBUG_PC
		if (debug)
			printf("unexcludeTCfromPC(%d) Free count for 1-cycle %d increased to %d, by freeing edge %d\n",
				t,oc,oco->ownerLoop->freeCount,p);
		#endif
		};
		
	unexcludeOrbitsTC(t);
	};
}

//	Traverse a loop, gathering up a list of all 2-cycles that make contact with it

int traverseLoopCount=0;

int traverseLoopContacts(struct loop *lp)
{
//	If we're not at a leaf, traverse child branches, quitting traversal if we are finished

struct loop *c0 = lp->firstChild;
if (c0)
	{
	struct loop *c=c0;
	while (TRUE)
		{
		if (c->freeCount>0)
			{
			if (traverseLoopContacts(c)) return TRUE;
			};
		c = c->nextSib;
		if (c==c0) return FALSE;
		};
	};

int fc=lp->freeCount;
if (fc==0) return FALSE;
	
//	At a leaf, so find a free edge in the 1-cycle

struct oneCycle *ocm = lp->oc;
for (int p=0;p<n;p++)
if (ocm->edgeStatus[p]==0)
	{
	int t=ocm->twoCycleNumbers[p];
	#if DEBUG_PC
	if (debug)
	if (PCexclusions[t]!=0)
		{
		printf("Mismatch between exclusion of 2-cycle %d, and edge status of 0 in 1-cycle %d\n",t,ocm->ocNumber);
		exit(EXIT_FAILURE);
		};
	#endif
	if (++PCcontacts[t]==2) multiContacts[nMultiContacts++]=t;
	loopContacts[nLoopContacts++]=t;
	if (--traverseLoopCount == 0) return TRUE;
	if (--fc == 0) return FALSE;
	};
return FALSE;
}

void unwindExclusionsStack()
{
int t0;
while ((t0=exclusionsStack[--nExclusionsStack])>=0)
	{
	#if DEBUG_PC
	if (debug)
		printf("unwindExclusionsStack() Unexcluding two-cycle %d, which had multiple contacts with loop\n",t0);
	#endif
	unexcludeTCfromPC(t0);
	};
}

//	Add a 2-cycle to the permutation chains.
//
//	If we return FALSE, a one-cycle that has not been incorporated into the chains has its freeCount drop to zero,
//	so the solution has become unviable.  We rewind the actions performed within the routine, so there is no
//	need to call dropTCfromPC() after such a return.

void dropTCfromPC(int skipLoopStuff);

struct loop **topLoopList=NULL;
#define UNWIND_TMP for (int i=0;i<n-1;i++) topLoopList[i]->tempFlag = FALSE;

#define TIDY_CONTACTS \
if (nLoopContacts!=0) \
	{ \
	for (int i=0;i<nLoopContacts;i++) PCcontacts[loopContacts[i]]=0; \
	nLoopContacts=0; \
	}; \

int addTCtoPC(int t, int orbit, int symmPairNumber)
{
#if DEBUG_PC
if (debug)
	printf("Adding 2-cycle %d to solution ...\n",t);
#endif

int *oft = oneForTwo+t*(n-1);
for (int i=0;i<n-1;i++)
	{
	int oc=oft[i];
	topLoopList[i]=topLoop(oneCycleTable+oc);
	
	//	If we hit the same loop twice, unwind and quit
	
	if (topLoopList[i]->tempFlag)
		{
		#if DEBUG_PC
		if (debug)
			printf("Unwinding addition of 2-cycle %d, as it hits a top-level loop more than once\n",t);
		#endif
		for (int j=i-1;j>=0;j--) topLoopList[j]->tempFlag = FALSE;
		TIDY_CONTACTS
		return FALSE;
		};
	topLoopList[i]->tempFlag = TRUE;
	};

if (nPCorbitBlocks!=0)
	{
	for (int k=0;k<numOrbitsTC[t];k++)
		{
		int norb = orbitNumbersTC[t][k];
		if (PCorbitBlocks[norb]>0 && PCorbitSlots[norb]<=1)
			{
			#if DEBUG_PC
			if (debug)
				printf("Two-cycle %d would complete blocked orbit %d, so disallowing it\n",t,norb);
			#endif
			UNWIND_TMP
			TIDY_CONTACTS
			return FALSE;
			};
		};
	};

//	Adjust edge status and free perm counts for all 1-cycles affected by this 2-cycle

int *ofp = oneForTwoStartPerms+t*(n-1);
for (int i=0;i<n-1;i++)
	{
	int oc=oft[i];
	int p=ofp[i];
	struct oneCycle *oco = oneCycleTable+oc;
	int t0 = oco->twoCycleNumbers[(p+1)%n];
	if (!excludeTCfromPC(t0))
		{
		#if DEBUG_PC
		if (debug)
			printf("Unwinding unviable addition of 2-cycle %d to solution\n",t);
		#endif
		
		for (int j=i-1;j>=0;j--)
			{
			oc=oft[j];
			p=ofp[j];
			oco = oneCycleTable+oc;
			oco->edgeStatus[p]=0;
			incFreeOC(oco);
			#if DEBUG_PC
			if (debug)
				printf("addTCtoPC(%d) Free count for 1-cycle %d increased to %d by freeing edge %d\n",
					t,oc,oco->ownerLoop->freeCount,p);
			#endif
			t0 = oco->twoCycleNumbers[(p+1)%n];
			unexcludeTCfromPC(t0);
			};
		UNWIND_TMP
		TIDY_CONTACTS
		return FALSE;
		};
	
	oco->edgeStatus[p]=1;
	decFreeOC(oco);
	#if DEBUG_PC
	if (debug)
		printf("addTCtoPC(%d) Free count for 1-cycle %d reduced to %d, by excluding edge %d\n",
			t,oc,oco->ownerLoop->freeCount,p);
	#endif
	};
	
//	Add 2-cycle to solution

PCorbit[PCsolSize]=orbit;
PCoffs[PCsolSize]=0;
PCcount[PCsolSize]=n-1;
PCsol[PCsolSize++]=t;
PCpoolSize--;
PCexclusions[t]++;

UNWIND_TMP

#if DEBUG_PC
if (debug)
	printf("Continued adding 2-cycle %d to solution, leaving solution size %d, pool size %d\n",t,PCsolSize,PCpoolSize);
#endif


//	Swallow up all the top-level loops visited by this 2-cycle under a new one.

struct loop *tcl = &loopTable[loopTableUsed++];
initLoop0(tcl);

for (int i=0;i<n-1;i++) addToLoop(tcl,topLoopList[i]);
int done=topLevelLoopCount==n-1;
if (tcl->freeCount > 0 || done)
	{
	//	Check for 2-cycles that make multiple contacts with this loop

	exclusionsStack[nExclusionsStack++]=-1;
	
	if (!done)
		{
		int nmc=0;
		if (symmPairNumber>=0 && topLoopLoop(stcLoop)==tcl) symmPairNumber=-1;
		
		#if DEBUG_PC
		if (debug)
			{
			printf("addTCtoPC() called with symmPairNumber=%d, orbit=%d, loop owns stcLoop=%s\n",
				symmPairNumber, orbit, topLoopLoop(stcLoop)==tcl?"Yes":"No");
		
			int resPC=FALSE;
			for (int z=0;z<nTC;z++)
			if (PCcontacts[z]!=0)
				{
				resPC=TRUE;
				break;
				};
			if (resPC)
				{
				printf("Residual nonzero PCcontacts\n");
				if (symmPairNumber!=1)
					{
					exit(EXIT_FAILURE);
					};
				};
			};
		#endif
		
		if (symmPairNumber==1)
			{
			nmc=nMultiContacts;
			}
		else
			{
			nLoopContacts=0;
			nMultiContacts=0;
			};
		traverseLoopCount = tcl->freeCount;
		traverseLoopContacts(tcl);
		if (symmPairNumber!=0)
			{
			for (int i=0;i<nLoopContacts;i++) PCcontacts[loopContacts[i]]=0;
			nLoopContacts=0;
			};
		
		for (int i=nmc;i<nMultiContacts;i++)
			{
			int texcl=multiContacts[i];
			#if DEBUG_PC
			if (debug)
				printf("addTCtoPC() Excluding 2-cycle %d because it has multiple contacts with loop formed from 2-cycle %d (symmPairNumber=%d)\n",texcl,t,symmPairNumber);
			#endif
			if (excludeTCfromPC(texcl))
				{
				exclusionsStack[nExclusionsStack++]=texcl;
				}
			else
				{
				#if DEBUG_PC
				if (debug)
					printf("addTCtoPC() Abandoning exclusion of 2-cycle %d, and hence unwinding addition of 2-cycle %d\n",texcl,t);
				#endif
				unwindExclusionsStack();
				dissolveLoop(tcl);
				loopTableUsed--;
				dropTCfromPC(TRUE);
				TIDY_CONTACTS
				return FALSE;
				};
			};
		};
		
	//	Having successfully incorporated all these loops, we need to replace them in the list of top loops
	
	struct loop *c0 = tcl->firstChild, *c=c0;
	while (TRUE)
		{
		c->nextTop->prevTop = c->prevTop;
		c->prevTop->nextTop = c->nextTop;
		c = c->nextSib;
		if (c==c0) break;
		};
	LINK_LOOP(tcl)
	topLevelLoopCount -= n-2;
	}
else
	{
	#if DEBUG_PC
	if (debug)
		printf("Unwinding addition of 2-cycle %d, as its freeCount is zero\n",t);
	#endif
	dissolveLoop(tcl);
	loopTableUsed--;
	dropTCfromPC(TRUE);
	TIDY_CONTACTS
	return FALSE;
	};

if (useOrbits)
	{
	excludeOrbitsTC(t);
	for (int k=0;k<numOrbitsTC[t];k++)
		{
		int norb = orbitNumbersTC[t][k];
		PCorbitSlots[norb]--;
		#if DEBUG_PC
		if (debug)
			printf("Adding two-cycle %d brings orbit %d down to %d slots left\n",t,norb,PCorbitSlots[norb]);
		#endif
		};
	};
return TRUE;
}

//	Remove the latest 2-cycle from the permutation chains.

void dropTCfromPC(int skipLoopStuff)
{
int t=PCsol[--PCsolSize];
PCpoolSize++;
PCexclusions[t]--;

if (!skipLoopStuff)
	{
	struct loop *tcl = &loopTable[--loopTableUsed];
	tcl->nextTop->prevTop = tcl->prevTop;
	tcl->prevTop->nextTop = tcl->nextTop;
	topLevelLoopCount--;
	struct loop *c0 = tcl->firstChild, *c=c0;
	while (TRUE)
		{
		LINK_LOOP(c)
		topLevelLoopCount++;
		c = c->nextSib;
		if (c==c0) break;
		};
	dissolveLoop(tcl);
	
	unwindExclusionsStack();
	
	unexcludeOrbitsTC(t);
	if (useOrbits)
		{
		for (int k=0;k<numOrbitsTC[t];k++)
			{
			int norb = orbitNumbersTC[t][k];
			PCorbitSlots[norb]++;
			#if DEBUG_PC
			if (debug)
				printf("Dropping two-cycle %d brings orbit %d up to %d slots left\n",t,norb,PCorbitSlots[norb]);
			#endif
			};
		};
	};

#if DEBUG_PC
if (debug)
	printf("Removing 2-cycle %d from solution, leaving solution size %d, pool size %d\n",t,PCsolSize,PCpoolSize);
#endif

int *oft = oneForTwo+t*(n-1), *ofp = oneForTwoStartPerms+t*(n-1);
for (int i=n-2;i>=0;i--)
	{
	int oc=oft[i];
	int p=ofp[i];
	struct oneCycle *oco = oneCycleTable+oc;
	
	oco->edgeStatus[p]=0;
	incFreeOC(oco);
	#if DEBUG_PC
	if (debug)
		printf("dropTCfromPC(%d) Free count for 1-cycle %d increased to %d by freeing edge %d\n",
			t,oc,oco->ownerLoop->freeCount,p);
	#endif
	int t0 = oco->twoCycleNumbers[(p+1)%n];
	unexcludeTCfromPC(t0);
	};

}

void excludeMultiContacts(struct loop *tcl)
{
exclusionsStack[nExclusionsStack++]=-1;
nLoopContacts=0;
nMultiContacts=0;
traverseLoopCount = tcl->freeCount;
if (traverseLoopCount && PCsolSize<PCsolTarget) traverseLoopContacts(tcl);
for (int i=0;i<nLoopContacts;i++) PCcontacts[loopContacts[i]]=0;
nLoopContacts=0;

for (int i=0;i<nMultiContacts;i++)
	{
	int texcl=multiContacts[i];
	if (excludeTCfromPC(texcl))
		{
		exclusionsStack[nExclusionsStack++]=texcl;
		}
	else
		{
		printf("excludeMultiContacts() Abandoning exclusion of 2-cycle %d\n",texcl);
		exit(EXIT_FAILURE);
		};
	};
}

//	Add part of a 2-cycle to the solution
//
//	We assume this will only be called when adding the fixed kernel to the solution, so
//	we don't allow it be unwound, we just fail hard if there are any problems.

int addPartialTCtoPC(int t, int orbit, int offs, int noc)
{
#if DEBUG_PC
if (debug)
	printf("Adding part of 2-cycle %d to solution (offs=%d, 1-cycle count=%d) ...\n",t,offs,noc);
#endif

if (PCexclusions[t]!=0)
	{
	printf("addPartialTCtoPC(%d) attempting to add an excluded 2-cycle\n",t);
	exit(EXIT_FAILURE);
	};

int *oft = oneForTwo+t*(n-1);

for (int i0=0;i0<noc;i0++)
	{
	int i = (offs+i0)%(n-1);
	int oc=oft[i];
	topLoopList[i0]=topLoop(oneCycleTable+oc);
	
	//	If we hit the same loop twice, unwind and quit
	
	if (topLoopList[i0]->tempFlag)
		{
		printf("Addition of partial 2-cycle %d hits a top-level loop more than once\n",t);
		exit(EXIT_FAILURE);
		};
	topLoopList[i0]->tempFlag = TRUE;
	};

//	Adjust edge status and free perm counts for all 1-cycles affected by this (partial) 2-cycle

int *ofp = oneForTwoStartPerms+t*(n-1);
for (int i0=0;i0<n-1;i0++)
	{
	int i = (offs+i0)%(n-1);
	
	int oc=oft[i];
	int p=ofp[i];
	struct oneCycle *oco = oneCycleTable+oc;

	if (oco->edgeStatus[p]!=0)
		{
		printf("addPartialTCtoPC(%d) attempting to use the same edge twice\n",t);
		exit(EXIT_FAILURE);
		};

	oco->edgeStatus[p]=i0<noc?1:2;
	decFreeOC(oco);
	#if DEBUG_PC
	if (debug)
		printf("addPartialTCtoPC(%d) Free count for 1-cycle %d reduced to %d, by excluding edge %d\n",
			t,oc,oco->ownerLoop->freeCount,p);
	#endif
	};
	
//	Add partial 2-cycle to solution

PCorbit[PCsolSize]=orbit;
PCoffs[PCsolSize]=offs;
PCcount[PCsolSize]=noc;
PCsol[PCsolSize++]=t;
PCpoolSize--;
PCexclusions[t]++;

for (int i=0;i<noc;i++) topLoopList[i]->tempFlag = FALSE;

#if DEBUG_PC
if (debug)
	printf("Continued adding partial 2-cycle %d to solution, leaving solution size %d, pool size %d\n",t,PCsolSize,PCpoolSize);
#endif

//	Swallow up all the top-level loops visited by this 2-cycle under a new one.

struct loop *tcl = &loopTable[loopTableUsed++];
initLoop0(tcl);

for (int i=0;i<noc;i++) addToLoop(tcl,topLoopList[i]);
if (tcl->freeCount > 0 || topLevelLoopCount==noc)
	{
	//	Exclude any 2-cycles that make multiple contacts with this loop
	
	excludeMultiContacts(tcl);
			
	//	Having successfully incorporated all these loops, we need to replace them in the list of top loops
	
	struct loop *c0 = tcl->firstChild, *c=c0;
	while (TRUE)
		{
		c->nextTop->prevTop = c->prevTop;
		c->prevTop->nextTop = c->nextTop;
		c = c->nextSib;
		if (c==c0) break;
		};
	LINK_LOOP(tcl)
	topLevelLoopCount -= noc-1;
	}
else
	{
	printf("Abandoning addition of 2-cycle %d, as its freeCount is zero\n",t);
	exit(EXIT_FAILURE);
	};
return TRUE;
}

void showPCsol()
{
int count=0, soc=0, prevOrb=-1;
for (int k=0;k<PCsolSize;k++)
	{
	int orb=PCorbit[k];
	if (k==0) {prevOrb=orb;};
	if (orb==prevOrb) soc++;
	if (orb!=prevOrb)
		{
		if (prevOrb >= 0) printf("Orbit #: %d  used: %d\n",prevOrb,soc);
		else if (count%20!=0) printf("\n");
		soc=1;
		prevOrb=orb;
		};
	if (k==PCsolSize-1 && orb>=0) printf("Orbit #: %d  used: %d\n",prevOrb,soc);
		
	if (orb<0)
		{
		if (count%20==0) printf("2-cycle #: ");
		printf("%d ",PCsol[k]);
		if (count%20==19) printf("\n");
		count++;
		};
	};
printf("\n");
}

void handlePCsol()
{
PCprovCount++;

static int *tcnums=NULL, *tcnumsCC=NULL, *ccOffs=NULL, *ccSize=NULL;
if (tcnums==NULL)
	{
	CHECK_MEM( tcnums = (int *)malloc(nTC*sizeof(int)) )
	CHECK_MEM( tcnumsCC = (int *)malloc(nTC*sizeof(int)) )
	CHECK_MEM( ccOffs = (int *)malloc(nTC*sizeof(int)) )
	CHECK_MEM( ccSize = (int *)malloc(nTC*sizeof(int)) )
	};
for (int q=0;q<PCsolSize;q++) tcnums[q]=PCsol[q];

if (!nsk)
	{
	int cc = splitCC(tcnums,PCsolSize,tcnumsCC,ccOffs,ccSize);
	if (cc!=nSTC) return;

	for (int k=0;k<cc;k++)
		{
		qsort(tcnumsCC+ccOffs[k],ccSize[k],sizeof(int),compare1);
		int matches=0;
		for (int j=0;j<nSTC;j++)
			{
			if (bsearch(&stcIndex[j], tcnumsCC+ccOffs[k], ccSize[k], sizeof(int), compare1)) matches++;
			};
		if (matches!=1) return;
		if (treesOnly && !isTree(tcnumsCC+ccOffs[k],ccSize[k],n)) return;
		};
	};

qsort(tcnums,PCsolSize,sizeof(int),compare1);

printf("Found SOLUTION %d (size %d) among %d provisional solutions:\n",PCsolCount+1,PCsolSize,PCprovCount);
if (showSols) showPCsol();

FILE *f = fopen(twoCyclesOutputFile,twoCyclesOutputFileOpened?"aa":"wa");
if (f==NULL)
	{
	printf("Error opening file %s\n",twoCyclesOutputFile);
	exit(EXIT_FAILURE);
	};
if (!twoCyclesOutputFileOpened)
	{
	fprintf(f,"{\n");
	twoCyclesOutputFileOpened=TRUE;
	};
if (PCsolCount>0) fprintf(f,",\n");
fprintf(f,"{");
for (int q=0;q<PCsolSize;q++)
	{
	printInt(f,twoCycles+n*tcnums[q],n,q==PCsolSize-1?"":",\n");
	};
fprintf(f,"}\n");
fclose(f);

fopen(superPermsOutputFile,superPermsOutputFileOpened?"aa":"wa");
if (f==NULL)
	{
	printf("Error opening file %s\n",superPermsOutputFile);
	exit(EXIT_FAILURE);
	};
superPermsOutputFileOpened=TRUE;
printSuperPerm(f);
fclose(f);

PCsolCount++;
return;
}

//	Traverse a tree of loops, to find either a suitable 2-cycle, or a suitable full orbit of 2-cycles to add.

static int traverseLoopTwoCycle=0, lookForOrbit=FALSE;
int traverseLoopSearch(struct loop *lp)
{
//	If we're not at a leaf, traverse child branches, quitting traversal if we succeeded

struct loop *c0 = lp->firstChild;
if (c0)
	{
	struct loop *c=c0;
	while (TRUE)
		{
		if (c->freeCount>0)
			{
			int res = traverseLoopSearch(c);
			if (res>=0) return res;
			};
		c = c->nextSib;
		if (c==c0) return -1;
		};
	};

int fc=lp->freeCount;
if (fc==0) return -1;
	
//	At a leaf, so find a free edge in the 1-cycle

struct oneCycle *ocm = lp->oc;
for (int p=0;p<n;p++)
if (ocm->edgeStatus[p]==0)
	{
	traverseLoopTwoCycle=ocm->twoCycleNumbers[p];
	#if DEBUG_PC
	if (debug)
	if (PCexclusions[traverseLoopTwoCycle]!=0)
		{
		printf("Mismatch between exclusion of 2-cycle %d, and edge status of 0 in 1-cycle %d\n",traverseLoopTwoCycle,ocm->ocNumber);
		exit(EXIT_FAILURE);
		};
	#endif
	if (!lookForOrbit) return traverseLoopTwoCycle;		//	Found a free edge and just want a 2-cycle, so we're done

	//	Try to find a whole orbit we can add simultaneously

	for (int k=0;k<numOrbitsTC[traverseLoopTwoCycle];k++)
		{
		int norb = orbitNumbersTC[traverseLoopTwoCycle][k];
		if (PCorbitExclusions[norb]==0 && PCsolSize + orbitSizes[norb] <= PCsolTarget) return norb;
		};
	if (--traverseLoopCount == 0 || --fc==0) return -1;
	};
return -1;
}

//	Search for a solution via permutation chains

struct loop **minL=NULL;
void searchPC(int lev)
{
#if DEBUG_PC
if (debug)
	printf("Searching for solutions with lev=%d, pool size=%d, solution size=%d\n",lev,PCpoolSize,PCsolSize);
#endif

if (trackPartial && PCsolSize > PClargestSolSeen)
	{
	printf("-------------- PCsolSize=%d, PCpoolSize=%d, PCsolTarget=%d --------\n",
		PCsolSize,PCpoolSize,PCsolTarget);
	showPCsol();
	PClargestSolSeen = PCsolSize;
	};

if (PCsolSize==PCsolTarget)
	{
	handlePCsol();
	return;
	};
	
if (PCsolSize > PCsolTarget) return;
if (PCsolSize+PCpoolSize < PCsolTarget) return;
if (fullSymm && PCavailOrbits==0) return;

#if DEBUG_PC
if (debug)
	{
	printf("In searchPC()\n");
//	checkLoopTable();
	};
#endif

lookForOrbit = useOrbits && PCavailOrbits!=0;
int oneLoopEnough=((!lookForOrbit)||fullSymm);

//	Find the loop with the smallest freeCount

struct loop *lp = loopHeader.nextTop;
int nMinL=0;
int minFreeCount=fn1;
while (lp != &loopHeader)
	{
	int f = lp->freeCount;
	if (f==0) return;
	
	if (f < minFreeCount)
		{
		minFreeCount = f;
		nMinL = 1;
		minL[0] = lp;
		if (f==1 && oneLoopEnough) break;		//	A single loop with 1 free is all we need
		}
	else if (f == minFreeCount)
		{
		minL[nMinL++] = lp;
		};
	lp = lp->nextTop;
	};
	
traverseLoopTwoCycle=-1;
int res=-1;

for (int j=0;j<nMinL;j++)
	{
	traverseLoopCount=minFreeCount;
	if ((res = traverseLoopSearch(minL[j])) >= 0) break;
	if (fullSymm) return;
	};

if (res<0 && lookForOrbit)
	{
	//	Can't find a whole orbit that works with this loop, so have to fall back to a single 2-cycle
	
	lookForOrbit=FALSE;
	res = traverseLoopTwoCycle;
	};

if (res<0) return;

if (lookForOrbit)
	{
	//	Try to add a whole orbit
	
	int nOrbit=res;
	int nsize = orbitSizes[nOrbit];
	int completed=TRUE;
	int spn = (nsize==2 && fullSymm && (!fixedPoints));
	for (int z=0;z<nsize;z++)
		{
		int t=orbits[nOrbit][z];
		if (!addTCtoPC(t,nOrbit, spn?(z):(-1)))
			{
			//	We hit a problem, so we need to unwind any previous additions
			
			completed=FALSE;
			for (int y=z-1;y>=0;y--) dropTCfromPC(FALSE);
			break;
			};
		};
		
	if (PCorbitExclusions[nOrbit]++ == 0) PCavailOrbits--;
	if (completed)
		{
		//	We added the whole orbit with no problems, so continue the search ...
		
		searchPC(lev+1);
		
		//	Then remove the individual 2-cycles from the solution
		
		for (int y=nsize-1;y>=0;y--) dropTCfromPC(FALSE);
		};
		
	//	Branch where we DON'T add this orbit;

	PCorbitBlocks[nOrbit]++;
	nPCorbitBlocks++;
	searchPC(lev+1);
	nPCorbitBlocks--;
	PCorbitBlocks[nOrbit]--;
					
	if (--PCorbitExclusions[nOrbit] == 0) PCavailOrbits++;
	}
else
	{
	//	Branch to add a single 2-cycle / exclude it
	
	int t=res;
	
	#if DEBUG_PC
	if (debug)
		printf("Chose 2-cycle %d to add at level %d\n",t,lev);
	#endif
	
	if (addTCtoPC(t,-1,-1))
		{
		searchPC(lev+1);
		dropTCfromPC(FALSE);
		};

	#if DEBUG_PC
	if (debug)
		printf("Chose 2-cycle %d to exclude at level %d\n",t,lev);
	#endif

	if (minFreeCount > 1 && excludeTCfromPC(t))
		{
		searchPC(lev+1);
		unexcludeTCfromPC(t);
		};
	};

#if DEBUG_PC
if (debug)
	printf("Finished searching for solutions with lev=%d, pool size=%d, solution size=%d\n",lev,PCpoolSize,PCsolSize);
#endif
return;
}

//	================================
//	Nonstandard kernel specification
//	================================

//	Parse a non-standard kernel specification

int parseNSK(const char *str)
{
const char *nskChars = str+strlen("nsk");

nskCount=(int)strlen(nskChars);
nskNOC=0;
nskScore = 0;

if (nskCount==0) return FALSE; 

CHECK_MEM( nskSpec = (int *)malloc(nskCount*sizeof(int)) )

int nskOK = TRUE;
for (int k=0;k<nskCount;k++)
	{
	if (nskChars[k]>='1' && nskChars[k]<'0'+n)
		{
		nskSpec[k] = nskChars[k]-'0';
		nskNOC+=nskSpec[k];
		}
	else if (nskChars[k]==' ' || nskChars[k]=='-') nskSpec[k]=-1;
	else
		{
		printf("Illegal character '%c' in non-standard kernel specification, must be digit from 1 to n-1,\nor a space or - for a weight-4 edge (enclose whole option in quotes to use a space)\n",nskChars[k]);
		nskOK=FALSE;
		};
	};

if (nskOK)
	{
	strcpy(nskString,str);
	for (int z=0;z<strlen(nskString);z++) if (nskString[z]==' ') nskString[z]='-';
	
	nskScore = nskNOC - (n-2)*nskCount;
	if (nskScore % (n-2) != 0)
		{
		printf("Kernel score of %d is not a multiple of n-2=%d\n",nskScore,n-2);
		return FALSE;
		};
	};
return nskOK;
}

//	============
//	Main program
//	============

int main(int argc, const char * argv[])
{
//	Parse command line
//	------------------

//	Currently only accept one numerical argument, n, but this can be expanded

int numArgs[]={-1,-1,-1,-1}, countNA=0, minNA=1, maxNA=1;

//	Get parameters from the command line

int ok=FALSE;
n=-1;

for (int i=1;i<argc;i++)
	{
	//	Numerical arguments
	
	if (argv[i][0]>='0' && argv[i][0]<='9')
		{
		numArgs[countNA]=-1;
	 	if (sscanf(argv[i],"%d",&numArgs[countNA])!=1) numArgs[countNA]=-1;
		else countNA++;
		
	 	if (n<0 && countNA==1)
	 		{
	 		n=numArgs[0];
			if (n>MAX_N||n<MIN_N)
				{
				if (n>=0) printf("n = %d is out of range, must be from %d to %d\n",n,MIN_N,MAX_N);
				ok=FALSE;
				break;
				}
			else ok=TRUE;
			};
		}
		
//	How much to print to console

	else if (strcmp(argv[i],"verbose")==0) verbose=TRUE;
	else if (strcmp(argv[i],"showSols")==0) showSols=TRUE;
	else if (strcmp(argv[i],"trackPartial")==0) trackPartial=TRUE;
	
//	Choice of kernel
	
	else if (strcmp(argv[i],"ffc")==0) ffc=TRUE;
	else if (strncmp(argv[i],"nsk",strlen("nsk"))==0)
		{
		//	Parse specification for a nonstandard kernel
		
		if (parseNSK(argv[i])) nsk=TRUE;
		else ok=FALSE;
		}
	
//	Choice of anchor points on kernel
	
	else if (strcmp(argv[i],"lastAnchor")==0) lastAnchor=TRUE;
	else if (strcmp(argv[i],"fixedPointAnchor")==0) fixedPointAnchor=TRUE;
	
//	Choice of orbit type

	else if (strcmp(argv[i],"stabiliser")==0) stabiliser=TRUE;
	else if (strcmp(argv[i],"limStab")==0) limStab=TRUE;
	else if (strcmp(argv[i],"symmPairs")==0) symmPairs=TRUE;
	else if (strcmp(argv[i],"littleGroup")==0) littleGroup=TRUE;
	else if (strcmp(argv[i],"blocks")==0) blocks=TRUE;
	
//	Details of how we handle orbits

	else if (strcmp(argv[i],"fixedPoints")==0) fixedPoints=TRUE;
	else if (strcmp(argv[i],"fullSymm")==0) fullSymm=TRUE;
	
//	Maybe filter solutions
	
	else if (strcmp(argv[i],"treesOnly")==0) treesOnly=TRUE;
	else
		{
		ok=FALSE;
		break;
		};
	};
	
if (countNA<minNA || countNA>maxNA)
	{
	printf("Expected between %d and %d numerical arguments\n",minNA,maxNA);
	ok=FALSE;
	};
	
//	Only allow one particular choice of orbit

int noc=0;
if (stabiliser) noc++;
if (limStab) noc++;
if (symmPairs) noc++;
if (littleGroup) noc++;
if (blocks) noc++;

useOrbits = (noc==1);
if (noc>1)
	{
	printf("Selected %d different orbit types\n",noc);
	ok=FALSE;
	};

//	Options for orbits need some orbit type

if ((fixedPoints||fullSymm) && (!useOrbits))
	{
	printf("Chose an option applying to orbits without selecting any orbit type\n");
	ok=FALSE;
	};
	
if (littleGroup && ffc)
	{
	printf("littleGroup option only applies to 3-cycle kernel\n");
	exit(EXIT_FAILURE);
	};

if (!ok)
	{
	printf("Bad command line syntax, see Readme.txt for usage\n");
	exit(EXIT_FAILURE);
	};

//	Set up permutations, etc.
//	-------------------------

//	Generate lists of permutations on up to n symbols

CHECK_MEM( permTab = (int **)malloc(n*sizeof(int *)) )
makePerms(n,permTab+n-1);

fn = factorial(n);
fn1 = factorial(n-1);
fn2 = factorial(n-2);
fn3 = factorial(n-3);
fn4 = factorial(n-4);

p0=permTab[n-1];		//	Permutations on n symbols
p1=permTab[n-2];		//	Permutations on n-1 symbols
p2=permTab[n-3];		//	Permutations on n-2 symbols
p3=permTab[n-4];		//	Permutations on n-3 symbols

int ntmp[MAX_N];

//	Generate a list of successors of each permutation

CHECK_MEM( w1s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w2s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w3s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w4s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w1234s = (int **)malloc(5*sizeof(int *)) )

w1234s[1]=w1s;
w1234s[2]=w2s;
w1234s[3]=w3s;
w1234s[4]=w4s;

for (int i=0;i<fn;i++)
	{
	int *p = p0 + n*i;
	successor1(p, ntmp, n);
	w1s[i] = searchPermutations(ntmp,n);
	if (w1s[i]<0)
		{
		printf("Cannot locate weight-1 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
		
	successor2(p, ntmp, n);
	w2s[i] = searchPermutations(ntmp,n);
	if (w2s[i]<0)
		{
		printf("Cannot locate weight-2 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
		
	successor3(p, ntmp, n);
	w3s[i] = searchPermutations(ntmp,n);
	if (w3s[i]<0)
		{
		printf("Cannot locate weight-3 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
		
	successor4(p, ntmp, n);
	w4s[i] = searchPermutations(ntmp,n);
	if (w4s[i]<0)
		{
		printf("Cannot locate weight-4 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
	};

//	Generate a list of all 2-cycles

nTC = n * fn2;

CHECK_MEM( twoCycles = (int *)malloc(nTC*n*sizeof(int)) )

int nm=0;
for (int m=1;m<=n;m++)
	{
	int q=0;
	//	List the digits {1...n} \ {m}
	for (int r=1;r<=n;r++) if (r!=m) ntmp[q++]=r;
	
	for (int k=0;k<fn2;k++)
		{
		twoCycles[nm++]=m;
		twoCycles[nm++]=ntmp[n-2];
		for (int j=0;j<n-2;j++)
			{
			twoCycles[nm++]=ntmp[p2[k*(n-2)+j]-1];
			};
		rClassMin(twoCycles+nm-(n-1),n-1);
		};
		
	//	Sort each block with a fixed m| into lexical order
	
	nCompareInt=n;
	qsort(twoCycles+(m-1)*fn2*n,fn2,n*sizeof(int),compareInt);
	};
	
printf("Number of 2-cycles is n(n-2)! = %d\n",nTC);
if (verbose)
	{
	printf("2-cycles:\n");
	printBlockN(stdout,twoCycles,n,nTC);
	};

//	Generate a list of all 1-cycles

n1C = factorial(n-1);

CHECK_MEM( oneCycles = (int *)malloc(n*n1C*sizeof(int)) )
int *p1=permTab[n-2];
for (int i=0;i<n1C;i++)
	{
	int *oc=oneCycles+n*i, *p=p1+(n-1)*i;
	oc[0]=n;
	for (int z=0;z<n-1;z++) oc[z+1]=p[z];
	rClassMin(oc,n);
	};
nCompareInt=n;
qsort(oneCycles,n1C,n*sizeof(int),compareInt);

printf("Number of 1-cycles is (n-1)! = %d\n",n1C);
if (verbose)
	{
	printf("1-cycles:\n");
	printBlockN(stdout,oneCycles,n,n1C);
	};
	
//	List the permutations for each 1-cycle

CHECK_MEM( permsForOne = (int *)malloc(n1C*n*sizeof(int)) )

for (int i=0;i<n1C;i++)
	{
	int *op = oneCycles+i*n;
	int p = permsForOne[n*i] = searchPermutations(op,n);
	if (p<0)
		{
		printf("Failed to find permutation for 1-cycle #%d in permutations list\n",i);
		exit(EXIT_FAILURE);
		};
	int q=p;
	for (int k=1;k<n;k++)
		{
		q = w1s[q];
		permsForOne[n*i+k] = q;
		};
	if (p != w1s[q])
		{
		printf("Mismatch between 1-cycle #%d and expected closed loop of weight-1 successors\n",i);
		exit(EXIT_FAILURE);
		};
	};
	
//	List the 1-cycles for each 2-cycle, and the starting permutation for each 2-cycle
//	in that 1-cycle.

CHECK_MEM( oneForTwo = (int *)malloc(nTC*(n-1)*sizeof(int)) )
CHECK_MEM( oneForTwoStartPerms = (int *)malloc(nTC*(n-1)*sizeof(int)) )

for (int i=0;i<nTC;i++)
	{
	int m = twoCycles[i*n];
	oneCycleList(i,oneForTwo+i*(n-1));
	for (int j=0;j<n-1;j++)
		{
		int *ocdigits = oneCycles + n*oneForTwo[i*(n-1)+j];
		int k=0;
		while (ocdigits[k]!=m) k++;
		oneForTwoStartPerms[i*(n-1)+j]=k;
		};
	};
	
if (verbose)
	{
	printf("2-cycles and their component 1-cycles:\n");
	for (int i=0;i<nTC;i++)
		{
		printf("%d: ",i);
		printInt(stdout,twoCycles+i*n,n," : ");
		for (int j=0;j<n-1;j++)
			{
			int oc=oneForTwo[i*(n-1)+j];
			printf("(%d/%d) ",oc,oneForTwoStartPerms[i*(n-1)+j]);
			printInt(stdout,oneCycles+n*oc,n," ");
			};
		printf("\n");
		};
	};

//	Structure of storage for flags

int bitsNeeded = nTC;
piecesPerBitString = bitsNeeded/bitsPerPiece;
while (piecesPerBitString*bitsPerPiece < bitsNeeded) piecesPerBitString++;

//	Incidence relations
//	-------------------

//	Generate the intersection type matrix between all the 2-cycles.
//	We identify each m|q (m is "missing", q is modulo rotations) with the 2-cycle which has a weight-2
//	permutation graph edge starting from m|rot(q) for any rotation of q.
//
//	We store the information as sets of flags for each 2-cycle,	with flags for:
//
//	* disjoint
//	* intersections of type 1 
//	* intersections of type 2
//
//	We also store a matrix which gives the intersection type itself between any pair of 2-cycles

int nFlagSets=3;
piecesPerRecord=piecesPerBitString*nFlagSets;
printf("\nNeed %ld bytes per flag set describing 2-cycle intersections\n",piecesPerRecord*sizeof(PIECE));

size_t tcFlagsSize = nTC*piecesPerRecord*sizeof(PIECE);
CHECK_MEM( tcFlags = (PIECE *)malloc(tcFlagsSize) )
size_t tcMatSize = nTC*nTC*sizeof(char);
CHECK_MEM( tcMat = (char *)malloc(tcMatSize) )

//	We store all the intersection data in a file, so we can re-use it on subsequent runs

sprintf(flagsFile,"IntersectionFlags%d.dat",n);

FILE *ff = fopen(flagsFile,"rb");
if (ff)
	{
	if (fread(tcFlags,tcFlagsSize,1,ff)!=1 || fread(tcMat,tcMatSize,1,ff)!=1)
		{
		printf("Error reading from file %s\n",flagsFile);
		exit(EXIT_FAILURE);
		};
	fclose(ff);
	printf("Read 2-cycle intersection flags from file %s\n",flagsFile);
	}
else
	{
	printf("Computing 2-cycle intersection flags ...\n");
	for (int i=0;i<nTC;i++)
		{
		ZERO(tcFlags+i*piecesPerRecord,piecesPerRecord)
		for (int j=0;j<nTC;j++)
		if (i==j) tcMat[i*nTC+j]=2;
		else
			{
			int t=intType(twoCycles+n*i,twoCycles+n*j,n);
			if (t<0 || t>2)
				{
				printf("Invalid intersection type %d\n",t);
				exit(EXIT_FAILURE);
				};
				
			tcMat[i*nTC+j]=t;
			SETBIT(tcFlags+i*piecesPerRecord+t*piecesPerBitString,j)
			};
		};
		
	ff = fopen(flagsFile,"wb");
	if (ff==NULL)
		{
		printf("Unable to open file %s to write\n",flagsFile);
		exit(EXIT_FAILURE);
		};
	if (fwrite(tcFlags,tcFlagsSize,1,ff)!=1 || fwrite(tcMat,tcMatSize,1,ff)!=1)
		{
		printf("Error writing to file %s\n",flagsFile);
		exit(EXIT_FAILURE);
		};
	fclose(ff);
	printf("Wrote 2-cycle intersection flags to file %s\n",flagsFile);
	};
	
//	Also set up lists of single and double intersection neighbours for each 2-cycle

setupNeighbours(&tcN, tcMat, nTC, verbose);

//	Standard 2-cycles in the kernel
//	-------------------------------
	
//	Default to the first 3-cycle, which contains (n-2) 2-cycles;
//	if we chose the first 4-cycle, it contains (n-3)(n-2) 2-cycles

//	Number of 3-cycles in kernel:

int n3C;
if (ffc) n3C=n-3;
else n3C=1;

//	Number of 2-cycles in kernel:

if (nsk)
	{
	//	Non-standard kernel
	nSTC=0;
	for (int k=0;k<nskCount;k++) if (nskSpec[k]>0) nSTC++;
	}
else nSTC = n3C*(n-2);

//	Standard 2-cycles as digits
	
CHECK_MEM( STC = malloc((nSTC)*n*sizeof(int)) )

//	Standard 2-cycles as index numbers to our table of 2-cycles

CHECK_MEM( stcIndex = malloc((nSTC)*sizeof(int)) )

//	Map from 2-cycle index numbers back to positions in the kernel, or -1 if not in kernel

CHECK_MEM( stcInverse = malloc(nTC*sizeof(char)) )
for (int k=0;k<nTC;k++) stcInverse[k]=-1;

CHECK_MEM( kernelW34Perms = (int *)malloc(nSTC*sizeof(int)) )
CHECK_MEM( kernelW34Weights = (int *)malloc(nSTC*sizeof(int)) )

if (!nsk)
	{
	//	Construct the standard 2-cycles

	int stcBase[MAX_N][MAX_N];
	for (int b=0;b<n3C;b++)
		{
		int q=0;
		for (int j=1;j<=n-3;j++)
			{
			if (j==b+1) stcBase[b][q++]=n-2;
			stcBase[b][q++]=j;
			};
		rClassMin(&stcBase[b][0],n-2);
		};

	printf("\nFirst %d standard 2-cycles, to be used in the kernel:\n",nSTC);
	int before = n-2;
	for (int k=0;k<nSTC;k++)
		{
		STC[k*n]=n;
		int b=k/(n-2);
		
		int b0=-1;
		for (int b1=0;b1<n-2;b1++) if (before==stcBase[b][b1]) {b0=b1; break;}
		before = stcBase[b][(b0+1)%(n-2)];
		
		int q=1;
		for (int j=1;j<=n-2;j++)
			{
			int j0=stcBase[b][j-1];
			if (j0==before) STC[k*n+(q++)]=n-1;
			STC[k*n+(q++)]=j0;
			};
			
		rClassMin(STC+k*n+1,n-1);
		stcIndex[k]=searchTwoCycles(STC+k*n,n);
		if (stcIndex[k]<0)
			{
			printf("Failed to find index for a standard 2-cycle:\n");
			printInt(stdout,STC+k*n,n,"\n");
			exit(EXIT_FAILURE);
			};
		stcInverse[stcIndex[k]]=k;
		if (verbose)
			{
			printf("Matched standard 2-cycle %d to index %d\n",k,stcIndex[k]);
			};
		};
	printBlock(stdout,STC,n,nSTC);
	
	//	Determine which permutations in the kernel are followed by edges of weight 3 or 4

	int p=0;					//	Start from permutation #0, 1234...n
	int nw=0;
	for (int i=0;i<n3C;i++)		//	Loop for however many 3-cycles are in the kernel
		{
		for (int q=0;q<n-2;q++)
			{
			for (int j=0;j<n-1;j++)
				{
				for (int k=0;k<n-1;k++) p = w1s[p];	//	Weight 1 edges, until we'd close the 1-cycle
				if (j==n-2) break;
				p = w2s[p];							//	Weight 2 edges, until we'd close the 2-cycle
				};
			if (q==n-3) break;						//	Weight 3 edges, until we'd close the 3-cycle
			
			kernelW34Perms[nw] = p;
			kernelW34Weights[nw] = 3;
			p = w3s[p];
			nw++;
			};
		kernelW34Perms[nw] = p;
		kernelW34Weights[nw] = 4;
		p = w4s[p];
		nw++;
		};
	}
else
	{
	//	Non-standard kernel
	
	CHECK_MEM( stcIncomplete = (char *)malloc(nSTC*sizeof(char)) )
	CHECK_MEM( stcOffs = (int *)malloc(nSTC*sizeof(int)) )
	CHECK_MEM( stcNOC = (int *)malloc(nSTC*sizeof(int)) )
	
	nskNOP = nskNOC*n;
	CHECK_MEM( nskPerms = (int *)malloc(nskNOP*sizeof(int)) )
	CHECK_MEM( nskAuto = (int *)malloc((n+1)*sizeof(int)) )

	int p=0;					//	Start from permutation #0, 1234...n
	int nw=0;					//	Count up edges of weights 3 or 4 in kernel
	int ntc=0;					//	Count up 2-cycles, complete or otherwise
	int np=0;					//	Count up permutations
	nskPerms[np++] = 0;
	
	if (verbose)
		{
		printf("Non-standard kernel permutations:\n{\n");
		printInt(stdout,p0,n,",\n");
		};
	
	for (int z=0;z<nskCount;z++)
	if (nskSpec[z]>0)
		{
		int tc=-1, offs=-1;
		
		tc = twoCycleFromPerm(p,&offs);
		
		if ((stcIncomplete[ntc] = (nskSpec[z]!=n-1)))
			{
			//	We have an INCOMPLETE 2-cycle in the kernel here, starting from permutation p
			//	and containing nskSpec[k] 1-cycles.
			
			stcOffs[ntc] = offs;
			stcNOC[ntc] = nskSpec[z];
			};
			
		stcIndex[ntc] = tc;
		stcInverse[tc] = ntc;
		ntc++;
		
		for (int j=0;j<nskSpec[z];j++)
			{
			for (int k=0;k<n-1;k++)
				{
				p = w1s[p];	//	Weight 1 edges, until we'd close the 1-cycle
				nskPerms[np++] = p;
				if (verbose) printInt(stdout,p0+p*n,n,np==nskNOP?"\n":",\n");
				};
			if (j==nskSpec[z]-1) break;
			p = w2s[p];							//	Weight 2 edges
			nskPerms[np++] = p;
			if (verbose) printInt(stdout,p0+p*n,n,np==nskNOP?"\n":",\n");
			};
		
		if (z!=nskCount-1 && nskSpec[z+1]>0)	//	Weight 3 edge, unless we're at the end, or have a weight-4 edge next
			{
			kernelW34Perms[nw] = p;
			kernelW34Weights[nw] = 3;
			p = w3s[p];
			nskPerms[np++] = p;
			if (verbose) printInt(stdout,p0+p*n,n,np==nskNOP?"\n":",\n");
			nw++;
			};
		}
	else
		{
		//	Specified a weight-4 edge
		
		kernelW34Perms[nw] = p;
		kernelW34Weights[nw] = 4;
		p = w4s[p];
		nskPerms[np++] = p;
		if (verbose) printInt(stdout,p0+p*n,n,np==nskNOP?"\n":",\n");
		nw++;
		};
	
	if (verbose) printf("}\n");
	
	if (np!=nskNOP)
		{
		printf("Did not find the expected number of permutations in the kernel\n");
		exit(EXIT_FAILURE);
		};
		
	//	See if kernel is palindromic
	
	//	Set up the automorphism that would reverse it, if it is
	
	nskAuto[0]=-1;
	for (int k=1;k<=n;k++) nskAuto[k] = p0[nskPerms[nskNOP-1]*n + n - k];
	
	nskPalindrome = TRUE;
	for (int i=0;i<nskNOP;i++)
		{
		if (nskPerms[nskNOP-1-i] != actAutoP(nskPerms[i],nskAuto))
			{
			nskPalindrome=FALSE;
			break;
			};
		};
		
	if (nskPalindrome)
		{
		printf("Kernel is palindromic, and can be reversed with the automorphism: ");
		printInt(stdout,nskAuto,n+1,"\n");
		}
	else if (symmPairs)
		{
		printf("Kernel is NOT palindromic, so cannot use the symmPairs option\n");
		exit(EXIT_FAILURE);
		};
		
	//	Give details of kernel
	
	//	First check that kernel is valid, containing no repeated permutations
	
	qsort(nskPerms,nskNOP,sizeof(int),compare1);
	for (int k=1;k<nskNOP;k++)
		{
		if (nskPerms[k]==nskPerms[k-1])
			{
			printf("Non-standard kernel contains repeated permutation: ");
			printInt(stdout,p0+n*nskPerms[k],n,"\n");
			exit(EXIT_FAILURE);
			};
		};
	
	printf("Non-standard kernel covers %d 1-cycles, and has score of %d\n",nskNOC,nskScore);
	
	if (verbose)
		{
		printf("Non-standard kernel 2-cycle details: \n");
		for (int k=0;k<nSTC;k++)
			{
			if (stcIncomplete[k])
				{
				printf("Incomplete 2-cycle #%d: ",stcIndex[k]);
				printInt(stdout,twoCycles+stcIndex[k]*n,n,"\n");
				printf("  with %d 1-cycles (starting from offset=%d):\n",stcNOC[k],stcOffs[k]);
				for (int z=0;z<stcNOC[k];z++)
					{
					int y = (stcOffs[k]+z)%(n-1);
					printf("  ");
					printInt(stdout,oneCycles+oneForTwo[stcIndex[k]*(n-1)+y]*n,n,"");
					printf(" oneForTwoStartPerm[]=%d\n",oneForTwoStartPerms[stcIndex[k]*(n-1)+y]);
					};
				}
			else
				{
				printf("Complete 2-cycle #%d: ",stcIndex[k]);
				printInt(stdout,twoCycles+stcIndex[k]*n,n,"\n");
				};
			};
		};
	};
	
if (verbose)
	{
	for (int i=0;i<nSTC-1;i++)
		{
		printf("Kernel has a weight %d edge from permutation: ",kernelW34Weights[i]);
		printInt(stdout,p0+kernelW34Perms[i]*n,n,"\n");
		};
	};

if (useOrbits)
	{
	//	Set up orbits
	//	-------------


	//	First, find the group of automorphisms that stabilise the set of standard 2-cycles we have selected,
	//	which will either be the first 3-cycle or the first 4-cycle.
	//
	//	Each automorphism is described by (n+1) integers, with the first being +1/-1 with -1 for reversal,
	//	and the remaining n integers being a digit map, giving the substitute digits for 1...n.
	//
	//	If we choose the option "littleGroup", we restrict the stabiliser group.  For odd n, we just remove all
	//	the reversals. For even n, this needs a bespoke approach.

	static int aut[MAX_N+1];
	int *stabiliserGroup = NULL;
	int nStabiliserGroup = 0;

	int littleGroup6[]={1,1,2,3,4,5,6,		-1,3,2,1,4,5,6,		-1,1,4,3,2,5,6,		1,3,4,1,2,5,6};
	int littleGroup8[]={1,1,2,3,4,5,6,7,8,	1,3,4,5,6,1,2,7,8,	1,5,6,1,2,3,4,7,8,
						-1,1,6,5,4,3,2,7,8, -1,5,4,3,2,1,6,7,8,	-1,3,2,1,6,5,4,7,8};
	int *lG=NULL;
	if (littleGroup)
		{
		if (n==6)
			{
			nCompareInt=7;
			qsort(littleGroup6,4,7*sizeof(int),compareInt);
			lG = littleGroup6;
			}
		else if (n==8)
			{
			nCompareInt=9;
			qsort(littleGroup8,6,9*sizeof(int),compareInt);
			lG = littleGroup8;
			}
		else if (n%2==0)
			{
			printf("Little group not supported for n=%d\n",n);
			exit(EXIT_FAILURE);
			};
		};
	int limStab7FFC[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	//					 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 
		
	int *lS=NULL;
	if (limStab)
		{
		if (ffc)
			{
			if (n==7)
				{
				lS = limStab7FFC;
				};
			};
		};

	if (!blocks && !symmPairs)
		{
		for (int pass=0;pass<2;pass++)
			{
			nStabiliserGroup = 0;
			
			for (int rev=1;rev>=-1;rev-=2)
			for (int p=0;p<fn1;p++)
				{
				aut[0]=rev;
				aut[n]=n;
				for (int z=0;z<n-1;z++) aut[z+1]=p1[p*(n-1)+z];

				if (littleGroup)
					{
					if (n%2==1 && !ffc)
						{
						if (rev<0) continue;
						}
					else
						{
						if (searchBlock(lG,n+1,n-2,aut)<0) continue;
						};
					};
			

				int stab=TRUE;
				if (nsk)
					{
					//	Act with automorphism on the individual permutations of a non-standard kernel
					
					for (int k=0;k<nskNOP;k++)
						{
						int ak = actAutoP(nskPerms[k],aut);
						if (!bsearch(&ak,nskPerms,nskNOP,sizeof(int),compare1))
							{
							stab=FALSE;
							break;
							};
						};
					}
				else
					{
					//	Act with automorphism on the full 2-cycles that mke up a standard kernel
					for (int k=0;k<nSTC;k++)
						{
						int ak = actAutoTC(stcIndex[k],aut);
						if (ak<0)
							{
							printf("Cannot find 2-cycle corresponding to action of automorphism\n");
							exit(EXIT_FAILURE);
							};
						if (stcInverse[ak]<0)
							{
							stab=FALSE;
							break;
							};
						};
					};
				if (stab)
					{
					if (limStab)
						{
						if (lS)
							{
							for (int k=0;k<nSTC;k++)
							if (lS[k]!=0)
								{
								int ak = actAutoTC(stcIndex[k],aut);
								int sak = stcInverse[ak];
								if (lS[k]!=lS[sak])
									{
									stab=FALSE;
									break;
									};
								};
							}
						else
							{
							printf("Option limStab not defined for this choice of n and kernel\n");
							exit(EXIT_FAILURE);
							};
						if (!stab) continue;
						};
					if (pass==1)
						{
						for (int z=0;z<n+1;z++) stabiliserGroup[nStabiliserGroup*(n+1)+z] = aut[z];
						};
					nStabiliserGroup++;
					};
				};
				
			if (pass==0)
				{
				CHECK_MEM( stabiliserGroup = (int *)malloc(nStabiliserGroup * (n+1) * sizeof(int)) )
				};
			};
			
		if (verbose)
			{
			printf("%s group for the kernel is:\n",littleGroup?"Reduced stabiliser":"Stabiliser");
			for (int i=0;i<nStabiliserGroup;i++)
				{
				printf("%4d ",i);
				printInt(stdout,stabiliserGroup+i*(n+1),n+1,"\n");
				printf("    Action on the 2-cycles: ");
				for (int k=0;k<nSTC;k++) printf("%d ",stcInverse[actAutoTC(stcIndex[k],stabiliserGroup+i*(n+1))]);
				printf("\n");
				};
			};
		};
		
	//	We now want to compute all orbits of 2-cycles under whatever groups we have

	int nGroups = 1;
	int *Groups[] = {stabiliserGroup};
	int groupSizes[] = {nStabiliserGroup};

	if (blocks)
		{
		groupSizes[0]=n-2;
		}
	else if (symmPairs)
		{
		groupSizes[0]=2;
		};

	nOrbits=0;
	int *orbitSizeTallies=NULL;
	int maxOrbitSize=0;

	//	Track which orbit each 2-cycle belongs to, for each group

	int *orbitInverse = (int *)malloc(nGroups*nTC*sizeof(int));
	CHECK_MEM(orbitInverse)

	for (int pass=0;pass<2;pass++)
		{
		nOrbits=0;
		for (int z=0;z<nGroups*nTC;z++) orbitInverse[z]=-1;
		
		for (int g=0;g<nGroups;g++)
			{
			for (int t=0;t<nTC;t++)
			if (orbitInverse[t*nGroups+g]<0)
				{
				int orbitSize=0;
				for (int h=0;h<groupSizes[g];h++)
					{
					int at = 0;
					if (blocks)
						{
						at=getBlockEntry(t,h%(n-2)+1);
						}
					else if (symmPairs)
						{
						if (h==0) at=t; else at=findSymm(t,n);
						}
					else at = actAutoTC(t,Groups[g]+h*(n+1));
					
					if (at < 0)
						{
						printf("Cannot find 2-cycle corresponding to action of automorphism\n");
						exit(EXIT_FAILURE);
						};
					if (orbitInverse[at*nGroups+g]<0)
						{
						orbitInverse[at*nGroups+g] = nOrbits;
						orbitSize++;
						}
					else
						{
						if (orbitInverse[at*nGroups+g] != nOrbits)
							{
							printf("Group %d, group element %d, two-cycle %d (image %d) in clashing orbits:  %d and %d\n",
								g,h,t,at,nOrbits,orbitInverse[at*nGroups+g]);
							exit(EXIT_FAILURE);
							};
						};
					};
				if (orbitSize > maxOrbitSize) maxOrbitSize = orbitSize;
				if (pass==1)
					{
					orbitSizes[nOrbits] = orbitSize;
					orbitSizeTallies[orbitSize]++;
					};
				nOrbits++;
				};
			};
		
		if (pass==0)
			{
			CHECK_MEM( orbitSizes = (int *)malloc(nOrbits * sizeof(int)) )
			CHECK_MEM( orbitSizeTallies = (int *)malloc((maxOrbitSize+1) * sizeof(int)) )
			for (int z=1;z<=maxOrbitSize;z++) orbitSizeTallies[z]=0;
			};
		};
		
	printf("Found %d orbits for %d groups:\n",nOrbits,nGroups);
	for (int z=1;z<=maxOrbitSize;z++)
		if (orbitSizeTallies[z]>0)
			printf("%d of size %d\n",orbitSizeTallies[z],z);
	printf("\n");

	//	Store the actual orbits

	if (verbose) printf("Orbits of 2-cycles are:\n");

	orbits = (int **)malloc(nOrbits * sizeof(int *));
	CHECK_MEM(orbits)
	int *orbitGroups = (int *)malloc(nOrbits * sizeof(int));
	CHECK_MEM(orbitGroups)

	for (int i=0;i<nOrbits;i++)
		{
		CHECK_MEM( orbits[i] = (int *)malloc(orbitSizes[i]*sizeof(int)) )
		
		int offs=0;
		for (int t=0;t<nTC;t++)
		for (int g=0;g<nGroups;g++)
			if (orbitInverse[t*nGroups+g]==i)
				{
				if (offs==orbitSizes[i])
					{
					printf("Miscalculation in orbit size (size greater than expected)\n");
					exit(EXIT_FAILURE);
					};
				orbits[i][offs++]=t;
				orbitGroups[i]=g;
				};
				
		if (offs!=orbitSizes[i])
			{
			printf("Miscalculation in orbit size (size less than expected)\n");
			exit(EXIT_FAILURE);
			};
		};
	if (verbose) printf("\n");

	//	See which orbits are unique and self-disjoint

	orbitOK = (char *)malloc(nOrbits * sizeof(char));
	CHECK_MEM(orbitOK)
	okOrbits = (int *)malloc(nOrbits * sizeof(int));
	CHECK_MEM(okOrbits)
	nOrbitsOK = 0;

	for (int i=0;i<nOrbits;i++)
		{
		int ok = selfDisj(orbits[i],orbitSizes[i]);
		if ((blocks||(symmPairs && (!fixedPoints))) && orbitSizes[i]==1) ok=FALSE;
		if (fullSymm && (!fixedPoints) && orbitSizes[i]!=groupSizes[0]) ok=FALSE;
		if (ok)
			{
			for (int j=0;j<i;j++)
				{
				if (orbitSizes[j]==orbitSizes[i])
					{
					nCompareInt = orbitSizes[i];
					if (compareInt(orbits[i],orbits[j])==0)
						{
						ok=FALSE;
						break;
						};
					};
				};
			orbitOK[i]=ok;
			if (ok)	okOrbits[nOrbitsOK++]=i;
			};
		};
		
	printf("%d unique, self-disjoint orbits\n",nOrbitsOK);

	if (verbose)
		{
		for (int j=0;j<nOrbitsOK;j++)
			{
			int i=okOrbits[j];
			printf("Group %d Orbit %d (original %d)] ",orbitGroups[i], j, i);
			for (int k=0;k<orbitSizes[i];k++) printf("%d ",orbits[i][k]);
			printf("\n");
			};
		};
		
	//	For each 2-cycle, list the orbits it belongs to

	CHECK_MEM( orbitNumbersTC = (int **)malloc(nTC*sizeof(int *)) )
	CHECK_MEM( numOrbitsTC = (int *)malloc(nTC*sizeof(int)) )
	for (int i=0;i<nTC;i++)
		{
		numOrbitsTC[i]=0;
		CHECK_MEM( orbitNumbersTC[i] = (int *)malloc(nGroups * sizeof(int)) )
		};

	for (int j=0;j<nOrbitsOK;j++)
		{
		int i=okOrbits[j];
		for (int k=0;k<orbitSizes[i];k++)
			{
			int tc=orbits[i][k];
			orbitNumbersTC[tc][numOrbitsTC[tc]++] = i;
			};
		};
		
	//	Sort by size of orbit

	for (int tc=0;tc<nTC;tc++)
		{
		int norbs=numOrbitsTC[tc];
		if (norbs>1)
			{
			qsort(orbitNumbersTC[tc],norbs,sizeof(int),compareOrbitSizes);
			};
		};
		
	if (verbose)
		{
		printf("Orbits for each 2-cycle:\n");
		for (int tc=0;tc<nTC;tc++)
			{
			printf("%d]: ",tc);
			for (int k=0;k<numOrbitsTC[tc];k++) printf("%d ",orbitNumbersTC[tc][k]);
			printf("\n");
			};
		};
		
	PCavailOrbits=nOrbits;
	CHECK_MEM( PCorbitExclusions = (int *)malloc(nOrbits*sizeof(int)) )
	for (int t=0;t<nOrbits;t++) PCorbitExclusions[t]=0;

	CHECK_MEM( PCorbitSlots = (int *)malloc(nOrbits*sizeof(int)) )
	for (int t=0;t<nOrbits;t++) PCorbitSlots[t]=orbitSizes[t];

	CHECK_MEM( PCorbitBlocks = (int *)malloc(nOrbits*sizeof(int)) )
	for (int t=0;t<nOrbits;t++) PCorbitBlocks[t]=0;
	nPCorbitBlocks=0;
	}
else
	{
	PCavailOrbits=0;
	nPCorbitBlocks=0;
	};
	
//	Set up table of all 1-cycles

CHECK_MEM( oneCycleTable = (struct oneCycle *)malloc(n1C*sizeof(struct oneCycle)) )

//	Initialise exclusion counts and pool sizes

PCpoolSize = nTC;
CHECK_MEM( PCexclusions = (int *)malloc(nTC*sizeof(int)) )
for (int t=0;t<nTC;t++) PCexclusions[t]=0;

CHECK_MEM( PCcontacts = (int *)malloc(nTC*sizeof(int)) )
for (int t=0;t<nTC;t++) PCcontacts[t]=0;


CHECK_MEM( loopContacts = (int *)malloc(nTC*sizeof(int)) )
CHECK_MEM( multiContacts = (int *)malloc(nTC*sizeof(int)) )
nLoopContacts=0;
nMultiContacts=0;

CHECK_MEM( exclusionsStack = (int *)malloc(2*nTC*sizeof(int)) )
nExclusionsStack = 0;

for (int i=0;i<n1C;i++)
	{
	static int tcdigits[MAX_N];

	oneCycleTable[i].ocNumber=i;
	CHECK_MEM( oneCycleTable[i].twoCycleNumbers = (int *)malloc(n*sizeof(int)) )
	CHECK_MEM( oneCycleTable[i].edgeStatus = (int *)malloc(n*sizeof(int)) )

	//	Point to digits in the canonical form of this 1-cycle
	
	int *ocdigits = oneCycles + n*i;
	
	//	Get the number of the 2-cycle associated with a 2-edge starting at each permutation
	
	if (verbose) printf("1-cycle #%d has 2-cycles: ",i);
	for (int j=0;j<n;j++)
		{
		for (int k=0;k<n;k++) tcdigits[k]=ocdigits[(j+k)%n];
		rClassMin(tcdigits+1,n-1);
		int t=searchTwoCycles(tcdigits,n);
		oneCycleTable[i].twoCycleNumbers[j]=t;
		if (verbose) printf("%d ",t);
		oneCycleTable[i].edgeStatus[j]=0;
		};
	if (verbose) printf("\n");
	};

//	Validate this against data in oneForTwo[] and oneForTwoStartPerms[]

for (int t=0;t<nTC;t++)
for (int k=0;k<n-1;k++)
	{
	int oc=oneForTwo[t*(n-1)+k];
	int p=oneForTwoStartPerms[t*(n-1)+k];
	if (oneCycleTable[oc].twoCycleNumbers[p]!=t)
		{
		printf("Incompatible data in oneForTwoStartPerms[]\n");
		exit(EXIT_FAILURE);
		};
	};

//	Initialise solution

PCsolSize=0;
PClargestSolSeen=0;
CHECK_MEM( PCsol = (int *)malloc(nTC*sizeof(int)) )
CHECK_MEM( PCorbit = (int *)malloc(nTC*sizeof(int)) )
CHECK_MEM( PCoffs = (int *)malloc(nTC*sizeof(int)) )
CHECK_MEM( PCcount = (int *)malloc(nTC*sizeof(int)) )

if (nsk) PCsolTarget = (fn1 - nskNOC)/(n-2) + nSTC;
else PCsolTarget = ffc ? ((n-1)*(fn3-(n-3)) + nSTC) : ((n-1)*fn3 - 1);

//	Set up a pool of loops we can draw on

loopTableSize = n1C + 2*PCsolTarget;
CHECK_MEM( loopTable = (struct loop *)malloc(loopTableSize*sizeof(struct loop)) )
loopTableUsed = 0;

CHECK_MEM( topLoopList = (struct loop **)malloc(n*sizeof(struct loop *)) )

CHECK_MEM( minL = (struct loop **)malloc(loopTableSize*sizeof(struct loop **)) )

//	Put each 1-cycle into a loop structure, and link the loops together

for (int oc=0;oc<n1C;oc++)
	{
	initLoopOC(loopTable+(loopTableUsed++), oneCycleTable+oc);
	if (oc==0) loopTable[oc].prevTop = &loopHeader;
	else loopTable[oc].prevTop = &loopTable[oc-1];
	if (oc==n1C-1) loopTable[oc].nextTop = &loopHeader;
	else loopTable[oc].nextTop = &loopTable[oc+1];
	};
loopHeader.nextTop = &loopTable[0];
loopHeader.prevTop = &loopTable[n1C-1];
loopHeader.freeCount = -1;
topLevelLoopCount = n1C;

if (verbose) checkLoopTable();

//	Add kernel's 2-cycles to solution

for (int i=0;i<nSTC;i++)
	{
	int si=stcIndex[i];
	int stcOrb=-1;
	
	if (useOrbits)
	for (int k=0;k<numOrbitsTC[si];k++)
		{
		int orb=orbitNumbersTC[si][k];
		stcOrb=orb;
		printf("Starting 2-cycle %d lies in orbit %d\n",si,stcOrb);
		};
		
	if (nsk && stcIncomplete[i])
		{
		addPartialTCtoPC(si,stcOrb,stcOffs[i],stcNOC[i]);
		}
	else if (!addTCtoPC(si,stcOrb,-1))
		{
		printf("Unable to add 2-cycle %d to solution\n",si);
		exit(EXIT_FAILURE);
		};
	};
	
//	Incorporate the loops we just formed into a single, larger loop

stcLoop = &loopTable[loopTableUsed++];
initLoop0(stcLoop);

struct loop *s=loopHeader.nextTop;
for (int i=0;i<nSTC;i++)
	{
	addToLoop(stcLoop,s);
	s = s->nextTop;
	};
	
struct loop *c0 = stcLoop->firstChild, *c=c0;
while (TRUE)
	{
	c->nextTop->prevTop = c->prevTop;
	c->prevTop->nextTop = c->nextTop;
	topLevelLoopCount--;
	c = c->nextSib;
	if (c==c0) break;
	};
LINK_LOOP(stcLoop)
topLevelLoopCount++;
	
if (verbose) checkLoopTable();

if (lastAnchor)
	{
	for (int i=0;i<nSTC-1;i++)
	if ((!nsk) || (!stcIncomplete[i]))
		{
		int t=stcIndex[i];
		if ((symmPairs && t==findSymm(stcIndex[nSTC-1],n)) || 
			(fixedPointAnchor && t==findSymm(t,n))) continue;
		
		for (int j=0;j<tcN.nvsi[t];j++) excludeTCfromPC(tcN.vsi[t][j]);
		};
	};

//	Exclude anything with double contacts with roots

for (int k=0;k<nSTC;k++)
if ((!nsk) || (!stcIncomplete[k]))
	{
	int t=stcIndex[k];
	for (int i=0;i<tcN.nvsi[t];i++)
		{
		int tc = tcN.vsi[t][i];
		if (++PCcontacts[tc]==2)
			{
			printf("Excluding 2-cycle %d because it has 2 contacts with standard 2-cycles\n",tc);
			if (!excludeTCfromPC(tc))
				{
				printf("Exclusion of 2-cycle %d is unviable\n",tc);
				exit(EXIT_FAILURE);
				};
			};
		};
	};
for (int t=0;t<nTC;t++) PCcontacts[t]=0;

excludeMultiContacts(stcLoop);

int spLen = fn + fn1 + fn2 + fn3 + n - 4;

if (nsk) spLen -= (nskScore/(n-2) - 1);

//	Open files for results

FILE *f;
sprintf(optionsDescription,"%s%s%s%s%s%s%s%s%s%s%s",
	nsk?nskString:(ffc?"FFC":"FTC"),
	
	stabiliser?"_STAB":"",
	limStab?"_LIMSTAB":"",
	symmPairs?"_2SYMM":"",
	littleGroup?"_LG":"",
	blocks?"_BLKS":"",
	
	lastAnchor?"_LA":"",
	fixedPointAnchor?"_FPA":"",
	
	fullSymm?"_FS":"",
	fixedPoints?"_FP":"",
	
	treesOnly?"_TreesOnly":"");
sprintf(twoCyclesOutputFile,"%d_%d_twoCycles_%s.txt",
	n, spLen, optionsDescription);
sprintf(superPermsOutputFile,"%d_%d_%s.txt",
	n, spLen, optionsDescription);
	
twoCyclesOutputFileOpened=FALSE;
superPermsOutputFileOpened=FALSE;
	
//	Search for solutions

printf("\nSearching for solutions of size %d ...\n",PCsolTarget);
PCsolCount=0;
PCprovCount=0;
searchPC(0);

if (twoCyclesOutputFileOpened)
	{
	f = fopen(twoCyclesOutputFile,"aa");
	if (f==NULL)
		{
		printf("Error opening results file %s to append\n",twoCyclesOutputFile);
		exit(EXIT_FAILURE);
		};
	fprintf(f,"}\n");
	fclose(f);
	};
	
return 0;
}
