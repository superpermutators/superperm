//
//  KernelFinder
//
//  Created by Greg Egan on 01/03/2019.
//
/*

Searches for "non-standard kernels", in the sense Robin Houston described here:

https://groups.google.com/d/msg/superpermutators/VRwU2OIuRhM/fofaUWqrBgAJ

A Houston kernel consists of a sequence of complete 1-cycles, separated by edges of weight 2, 3 or 4.
Each run of d consecutive 1-cycles separated by weight-2 edges is represented by the digit d, with
consecutive digits implicitly separated by weight-3 edges.  A weight-4 edge is explicitly given by
a break in the digit blocks; Robin Houston wrote these with a space, but we will use a dash, '-',
as that makes it easier to pass kernels as command line arguments.  Internally, we will represent
a kernel as a sequence of integers, with -1 indicating weight-4 edges.

The total number of 1-cycles covered is just the sum of the digits (i.e. the non-negative integers in
out internal representation).  The score for the kernel, as Robin Houston defined it, is given by
the number of 1-cycles minus (n-2) times the total length of the sequence (where the length includes
any weight-4 edges).

Since these kernels deal in complete 1-cycles, we actually consider the identical situation of moving between
permutations of one fewer digits, using edges of weight one less than those described above.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*

Contents:

Macros and parameters
Global variables
Basic utility functions
Kernel operations
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

//	Memory allocation check/fail

#define CHECK_MEM(p) if ((p)==NULL) {printf("Insufficient memory\n"); exit(EXIT_FAILURE);};

//	================
//	Global variables
//	================

//	Options set on the command line
//	-------------------------------

static int n;						//	The number of digits we are permuting (one less than the kernel target n)
static int kernelN;					//	The kernel target n
static int palindromic1=FALSE;		//	Whether to restrict search to palindromic kernels of type 1.
static int palindromic2=FALSE;		//	Whether to restrict search to palindromic kernels of type 2.
static int minimumScore=0;			//	Minimum score to output

//	File names
//	----------

static char outputFile[128];

//	Permutations
//	------------

//	Factorials of n, n-1, n-2, n-3, n-4

static int fn, fn1, fn2, fn3, fn4;

//	A list of permutation lists

static int **permTab;

//	Lists of permutations on n, n-1, n-2, n-3 symbols

static int *p0=NULL, *p1=NULL, *p2=NULL, *p3=NULL;

//	The weight-1, weight-2, weight-3 and weight-4 successors of each permutation

static int *w1s=NULL, *w2s=NULL, *w3s=NULL, *w4s=NULL;
static int **w1234s=NULL;

//	The weight-1, weight-2, weight-3 and weight-4 predecessors of each permutation

static int *w1p=NULL, *w2p=NULL, *w3p=NULL, *w4p=NULL;
static int **w1234p=NULL;

//	Flags set as permutations are visited

static char *pVisited=NULL;

//	The current kernel

static int maxKernelLength=0;
static int *currentKernel=NULL, currentKernelLength=0, currentKernelScore=0, longestKernelChecked=0;

//	The current permutation

static int currentPerm=0, currentPerm2=0;

//	Best score seen so far

static int bestScoreSeen=0;

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

//	Count the number of 1-cycles covered by a kernel

int oneCyclesCovered(int *kernel, int kLen)
{
int sum=0;
for (int i=0;i<kLen;i++) if (kernel[i]>0) sum+=kernel[i];
return sum;
}

//	The score of a kernel

int scoreK(int *kernel, int kLen, int nk)
{
return oneCyclesCovered(kernel,kLen) - (nk-2)*kLen;
}

//	Print a kernel

void printKernel(FILE *fp, int *kernel, int kLen)
{
if (palindromic1)
	{
	for (int i=kLen-1;i>=0;i--)
		{
		if (kernel[i]>0) fprintf(fp,"%d",kernel[i]);
		else fprintf(fp,"-");
		};
	for (int i=0;i<kLen;i++)
		{
		if (kernel[i]>0) fprintf(fp,"%d",kernel[i]);
		else fprintf(fp,"-");
		};
	fprintf(fp,"\n");
	}
else if (palindromic2)
	{
	for (int i=kLen-1;i>=1;i--)
		{
		if (kernel[i]>0) fprintf(fp,"%d",kernel[i]);
		else fprintf(fp,"-");
		};
	for (int i=0;i<kLen;i++)
		{
		if (kernel[i]>0) fprintf(fp,"%d",kernel[i]);
		else fprintf(fp,"-");
		};
	fprintf(fp,"\n");
	}
else
	{
	for (int i=0;i<kLen;i++)
		{
		if (kernel[i]>0) fprintf(fp,"%d",kernel[i]);
		else fprintf(fp,"-");
		};
	fprintf(fp,"\n");
	};
}

//	=================
//	Kernel operations
//	=================

//	Add one step to the kernel, either a positive digit from 1 to n, or -1 for a weight-3 edge
//
//	If this step causes a permutation to be revisited, the addition is unwound and returns FALSE

int addStep(int d, int pal)
{
if (pal)
	{
	if (d>0)
		{
		int p = currentPerm, p2=currentPerm2;
		for (int i=0;i<d;i++)
			{
			if (pVisited[p]||pVisited[p2])
				{
				//	Unwind addition
				
				for (int j=i-1;j>=0;j--)
					{
					p = w1p[p];
					pVisited[p] = FALSE;
					
					p2 = w1s[p2];
					pVisited[p2] = FALSE;
					};
				return FALSE;
				};
				
			pVisited[p]=TRUE;
			pVisited[p2]=TRUE;
			if (i<d-1)
				{
				p = w1s[p];
				p2 = w1p[p2];
				};
			};
		currentPerm = w2s[p];
		currentPerm2 = w2p[p2];
		}
	else
		{
		currentPerm = w3s[w2p[currentPerm]];
		currentPerm2 = w3p[w2s[currentPerm2]];
		};

	currentKernel[currentKernelLength++] = d;
	currentKernelScore += 2*((d>0 ? d : 0) - (kernelN - 2));
	return TRUE;
	}
else
	{
	if (d>0)
		{
		int p = currentPerm;
		for (int i=0;i<d;i++)
			{
			if (pVisited[p])
				{
				//	Unwind addition
				
				for (int j=i-1;j>=0;j--)
					{
					p = w1p[p];
					pVisited[p] = FALSE;
					};
				return FALSE;
				};
				
			pVisited[p]=TRUE;
			if (i<d-1) p = w1s[p];
			};
		currentPerm = w2s[p];
		}
	else
		{
		currentPerm = w3s[w2p[currentPerm]];
		};

	currentKernel[currentKernelLength++] = d;
	currentKernelScore += ((d>0 ? d : 0) - (kernelN - 2));
	return TRUE;
	};
}

//	Remove the latest step added to the kernel

void removeStep(int pal)
{
if (pal)
	{
	if (currentKernelLength>0)
		{
		int d = currentKernel[--currentKernelLength];
		if (d<0)
			{
			currentKernelScore += 2*(kernelN-2);
			currentPerm = w2s[w3p[currentPerm]];
			currentPerm2 = w2p[w3s[currentPerm2]];
			}
		else
			{
			currentKernelScore -= 2*(d - (kernelN - 2));
			currentPerm = w2p[currentPerm];
			currentPerm2 = w2s[currentPerm2];
			for (int i=0;i<d;i++)
				{
				pVisited[currentPerm] = FALSE;
				pVisited[currentPerm2] = FALSE;
				if (i<d-1)
					{
					currentPerm = w1p[currentPerm];
					currentPerm2 = w1s[currentPerm2];
					};
				};
			};
		};
	}
else
	{
	if (currentKernelLength>0)
		{
		int d = currentKernel[--currentKernelLength];
		if (d<0)
			{
			currentKernelScore += kernelN-2;
			currentPerm = w2s[w3p[currentPerm]];
			}
		else
			{
			currentKernelScore -= (d - (kernelN - 2));
			currentPerm = w2p[currentPerm];
			for (int i=0;i<d;i++)
				{
				pVisited[currentPerm] = FALSE;
				if (i<d-1) currentPerm = w1p[currentPerm];
				};
			};
		};
	};
}

void search()
{
if (currentKernelScore > bestScoreSeen)
	{
	bestScoreSeen = currentKernelScore;
	printf("Best score seen so far = %d\n",bestScoreSeen);
	};
	
//	Effective kernel length is larger than actual nu,mber of symbols stored for palindromes

int ekl;
if (palindromic1) ekl = 2*currentKernelLength;
else if (palindromic2) ekl = 2*currentKernelLength-1;
else ekl = currentKernelLength;
	
if (ekl > longestKernelChecked)
	{
	longestKernelChecked = ekl;
	printf("Longest kernel checked so far = %d\n",longestKernelChecked);
	};
	
if (currentKernelScore >= minimumScore && currentKernelScore % (kernelN-2) == 0 && currentKernel[currentKernelLength-1]>0)
	{
	FILE *fout = fopen(outputFile,"aa");
	if (fout==NULL)
		{
		printf("Unable to open output file %s to append\n",outputFile);
		exit(EXIT_FAILURE);
		};
	printKernel(fout,currentKernel,currentKernelLength);
	fclose(fout);
	};
	
if (ekl==maxKernelLength) return;

int maxMore = maxKernelLength - ekl;

if (currentKernelScore + maxMore < minimumScore) return;

int pal = palindromic1 || (palindromic2 && currentKernelLength!=0);

for (int d=2;d<=n;d++)
	{
	if (addStep(d, pal))
		{
		search();
		removeStep(pal);
		}
	else break;
	};
	
if (currentKernelLength!=0 && currentKernel[currentKernelLength-1]>0)
	{
	if (addStep(-1,pal))
		{
		search();
		removeStep(pal);
		};
	};
}


//	============
//	Main program
//	============

int main(int argc, const char * argv[])
{
//	Parse command line
//	------------------

//	Currently only accept one numerical argument, n, but this can be expanded

int numArgs[]={-1,-1,-1,-1}, countNA=0, minNA=1, maxNA=3;

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
	 		kernelN=numArgs[0];
	 		n=kernelN-1;
	 		
			if (kernelN>MAX_N||kernelN<MIN_N)
				{
				if (kernelN>=0) printf("kernelN = %d is out of range, must be from %d to %d\n",kernelN,MIN_N,MAX_N);
				ok=FALSE;
				break;
				}
			else ok=TRUE;
			};
		}
		
//	How much to print to console

	else if (strcmp(argv[i],"palindromic1")==0) palindromic1=TRUE;
	else if (strcmp(argv[i],"palindromic2")==0) palindromic2=TRUE;
	else
		{
		ok=FALSE;
		break;
		};
	};
	
if (countNA>=2) minimumScore = numArgs[1];
else minimumScore = kernelN-2;
	
if (countNA>=3) maxKernelLength = numArgs[2];
else maxKernelLength = 30;
	
if (countNA<minNA || countNA>maxNA)
	{
	printf("Expected between %d and %d numerical arguments\n",minNA,maxNA);
	ok=FALSE;
	};
	
if (palindromic1 && palindromic2) ok = FALSE;
	
if (!ok)
	{
	printf("Bad command line syntax\n");
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

//	Generate a list of successors/predecessors of each permutation

CHECK_MEM( w1s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w2s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w3s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w4s = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w1234s = (int **)malloc(5*sizeof(int *)) )

w1234s[1]=w1s;
w1234s[2]=w2s;
w1234s[3]=w3s;
w1234s[4]=w4s;

CHECK_MEM( w1p = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w2p = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w3p = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w4p = (int *)malloc(fn*sizeof(int)) )
CHECK_MEM( w1234p = (int **)malloc(5*sizeof(int *)) )

w1234p[1]=w1p;
w1234p[2]=w2p;
w1234p[3]=w3p;
w1234p[4]=w4p;

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
	w1p[w1s[i]] = i;
		
	successor2(p, ntmp, n);
	w2s[i] = searchPermutations(ntmp,n);
	if (w2s[i]<0)
		{
		printf("Cannot locate weight-2 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
	w2p[w2s[i]] = i;
		
	successor3(p, ntmp, n);
	w3s[i] = searchPermutations(ntmp,n);
	if (w3s[i]<0)
		{
		printf("Cannot locate weight-3 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
	w3p[w3s[i]] = i;
		
	successor4(p, ntmp, n);
	w4s[i] = searchPermutations(ntmp,n);
	if (w4s[i]<0)
		{
		printf("Cannot locate weight-4 successor of permutation #%d\n",i);
		exit(EXIT_FAILURE);
		};
	w4p[w4s[i]] = i;
	};

sprintf(outputFile,"Kernels%d_%d_%d%s%s.txt",
	kernelN,minimumScore,maxKernelLength,palindromic1?("P1"):"",palindromic2?("P2"):"");
FILE *fout = fopen(outputFile,"wa");
if (fout==NULL)
	{
	printf("Unable to open output file %s to write\n",outputFile);
	exit(EXIT_FAILURE);
	};
	
fclose(fout);

CHECK_MEM( pVisited = (char *)malloc(fn*sizeof(char)) )
for (int i=0;i<fn;i++) pVisited[i]=FALSE;

CHECK_MEM( currentKernel = (int *)malloc(2*fn*sizeof(int)) )

currentKernelLength=0;
currentKernelScore=0;
currentPerm=0;

if (palindromic1 || palindromic2) currentPerm2 = w2p[currentPerm];

bestScoreSeen=-fn;
longestKernelChecked=0;
search();

//	Special case for odd-numbered palindromes, where we have a weight-3 edge in the middle

if (palindromic2)
	{
	currentKernel[0]=-1;
	currentKernelLength=1;
	currentKernelScore=-(kernelN-2);
	currentPerm=0;
	currentPerm2=w3p[currentPerm];
	search();
	};

return 0;
}
