/*

DistributedChaffinMethod.c
==========================

Author: Greg Egan
Version: 2
Last Updated: 25 April 2019

This program implements Benjamin Chaffin's algorithm for finding minimal superpermutations with a branch-and-bound
search.  It is based in part on Nathaniel Johnston's 2014 version of Chaffin's algorithm; see:

http://www.njohnston.ca/2014/08/all-minimal-superpermutations-on-five-symbols-have-been-found/

This version is designed to run a distributed search, spread across any number of contributing instances of the program,
which share their results and coordinate their activities via a database on a single server.  A non-distributed
version can be found at:

https://github.com/superpermutators/superperm/tree/master/ChaffinMethod

Usage:

No user input is required.

The program waits in a loop for tasks to become available from the coordinating server.  When a task is
assigned, the program searches all branches starting from one node of the search tree, regularly checking
back with the server during the search for any updates from other instances that will allow it to prune the
search more aggressively. If the program finds it has spent more than a set time in one sub-branch of the search,
it will instruct the server to split the original task into multiple tasks, one for each sub-branch, so that
other instances can deal with other sub-branches; this splitting can take place multiple times.

If the program fails to check back with the server after a set time (determined by the server's administrator)
then it is assumed to have crashed (or been terminated by the user) and the task it was performing is reassigned to
another instance of the program.

*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32

//	Compiling for Windows

#include <windows.h>
#include <process.h>

#else

//	Compiling for MacOS or Linux

#include <unistd.h>

#endif

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

//	Server URL

#define SERVER_URL "http://www.gregegan.net/SCIENCE/Superpermutations/ChaffinMethod.php?version=2&"

//	Command-line utility that gets response from a supplied URL
//	Current choice is curl (with options to suppress progress meter but display any errors)

#define URL_UTILITY "curl --silent --show-error "

//	Name of temporary file in which response from server is placed

#define SERVER_RESPONSE_FILE_NAME_TEMPLATE "DCMServerResponse_%lu.txt"

//	Name of log file

#define LOG_FILE_NAME_TEMPLATE "DCMLog_%lu.txt"

//	Max size of file names

#define FILE_NAME_SIZE 128

//	Times

#define MINUTE 60

//	Time between checking in with server (while performing a task)

#define TIME_BETWEEN_SERVER_CHECKINS (2*MINUTE)

//	Time to spend on a branch before splitting the search

#define TIME_BEFORE_SPLIT (3*MINUTE)

//	Initial number of nodes to check before we bother to check elapsed time;
//	we rescale the actual value (in nodesBeforeTimeCheck) if it is too large or too small

#define NODES_BEFORE_TIME_CHECK 1000000

//	Set a floor and ceiling so we can't waste an absurd amount of time doing time checks,
//	or take too long between them.

#define MIN_NODES_BEFORE_TIME_CHECK 10000
#define MAX_NODES_BEFORE_TIME_CHECK 100000000

//	Size of general-purpose buffer for messages from server, log, etc.

#define BUFFER_SIZE (32*1024)

//	Macros
//	------

#define MFREE(p) if ((p)!=NULL) free(p); 
#define CHECK_MEM(p) if ((p)==NULL) {printf("Insufficient memory\n"); exit(EXIT_FAILURE);};

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

struct task
{
unsigned int task_id;
unsigned int access_code;
unsigned int n_value;
unsigned int w_value;
char *prefix;
unsigned int prefixLen;
unsigned int perm_to_exceed;
unsigned int prev_perm_ruled_out;
};

//	Global variables
//	----------------

int n=-1;					//	The number of symbols in the permutations we are considering
int fn;						//	n!
int nm;						//	n-1
int nmbits;					//	(n-1)*DBITS
int maxInt;					//	Highest integer representation of an n-digit sequence we can encounter, plus 1
int maxIntM;				//	Highest integer representation of an (n-1)-digit sequence we can encounter, plus 1
int maxW;					//	Largest number of wasted characters we allow for
char *curstr=NULL;			//	Current string as integer digits
char *curi=NULL;			//	Current string digit indices
char *curd=NULL;			//	Current string digits visited			
char *bestSeen=NULL;		//	Longest string seen in search, as integer digits
int bestSeenLen, bestSeenP;
char *asciiString=NULL;		//	String as ASCII digits
int max_perm;				//	Maximum number of permutations visited by any string seen so far
int *mperm_res=NULL;		//	For each number of wasted characters, the maximum number of permutations that can be visited
int tot_bl;					//	The total number of wasted characters we are allowing in strings, in current search
char *unvisited=NULL;		//	Flags set FALSE when we visit a permutation, indexed by integer rep of permutation
char *valid=NULL;			//	Flags saying whether integer rep of digit sequence corresponds to a valid permutation
int *ldd=NULL;				//	For each digit sequence, n - (the longest run of distinct digits, starting from the last)
struct digitScore *nextDigits=NULL;	//	For each (n-1)-length digit sequence, possible next digits in preferred order

int noc;					//	Number of 1-cycles
int nocThresh;				//	Threshold for unvisited 1-cycles before we try new bounds		
int *oneCycleCounts=NULL;	//	Number of unvisited permutations in each 1-cycle
int *oneCycleIndices=NULL;	//	The 1-cycle to which each permutation belongs
int oneCycleBins[MAX_N+1];	//	The numbers of 1-cycles that have 0 ... n unvisited permutations

int done=FALSE;				//	Global flag we can set for speedy fall-through of recursion once we know there is nothing else we want to do

//	Monitoring 1-cycle tracking

int ocpTrackingOn, ocpTrackingOff;

//	For n=0,1,2,3,4,5,6,7
int ocpThreshold[]={1000,1000,1000,1000, 6, 24, 120, 720};

char buffer[BUFFER_SIZE];
char lbuffer[BUFFER_SIZE];

struct task currentTask;

#define N_TASK_STRINGS 7
char *taskStrings[] = {"Task id: ","Access code: ","n: ","w: ","str: ","pte: ","pro: "};

unsigned long int nodesChecked;		//	Count of nodes checked since last time check
unsigned long int nodesBeforeTimeCheck = NODES_BEFORE_TIME_CHECK;
time_t startedCurrentTask;			//	Time we started current task
time_t timeOfLastCheckin;			//	Time we last contacted the server


static char SERVER_RESPONSE_FILE_NAME[FILE_NAME_SIZE];
static char LOG_FILE_NAME[FILE_NAME_SIZE];

//	Function definitions
//	--------------------

void logString(const char *s);
void sleepForSecs(int secs);
void setupForN(int nval);
int sendServerCommand(const char *command);
void sendServerCommandAndLog(const char *s);
int logServerResponse(const char *reqd);
int getTask(struct task *tsk);
int getMax(int nval, int wval, int oldMax);
void doTask(void);
void splitTask(const char *retainedDigits);
void fillStr(int pos, int pfound, int partNum);
int fac(int k);
void makePerms(int n, int **permTab);
void witnessCurrentString(int size);
int compareDS(const void *ii0, const void *jj0);
void rClassMin(int *p, int n);
int pruneOnPerms(int w, int d0);

//	Main program
//	------------

int main(int argc, const char * argv[])
{
//	Choose random file names

time_t t0;
time(&t0);

srand((int)( (t0 + clock()) % (1<<31) ));
unsigned long int rnum=rand();

sprintf(SERVER_RESPONSE_FILE_NAME,SERVER_RESPONSE_FILE_NAME_TEMPLATE,rnum);
sprintf(LOG_FILE_NAME,LOG_FILE_NAME_TEMPLATE,rnum);

int justTest = argc==2 && strcmp(argv[1],"test")==0;

//	First, just check we can establish contact with the server

int s1 = sendServerCommand("action=hello");
int s2 = logServerResponse("Hello world.");

if (s1!=0 || (!s2))
	{
	logString("Error: Failed to make contact with the server\n");
	exit(EXIT_FAILURE);
	};
	
if (justTest) exit(0);

while (TRUE)
	{
	int t = getTask(&currentTask);
	
	if (t<0)
		{
		logString("Quit instruction from server");
		break;
		};
	
	if (t==0)
		{
		logString("No tasks available");
		sleepForSecs(TIME_BETWEEN_SERVER_CHECKINS);
		}
	else
		{
		sprintf(buffer,"Assigned new task (id=%u, access=%u, n=%u, w=%u, prefix=%s, perm_to_exceed=%u, prev_perm_ruled_out=%u)",
			currentTask.task_id,
			currentTask.access_code,
			currentTask.n_value,
			currentTask.w_value,
			currentTask.prefix,
			currentTask.perm_to_exceed,
			currentTask.prev_perm_ruled_out);
		logString(buffer);
		
		doTask();
		};
	};
return 0;
}

//	Function to set up storage and various tables for a given value of n
//
//	Storage is deallocated from previous use if necessary

void setupForN(int nval)
{
if (n==nval) return;

n = nval;
fn = fac(n);

//	Storage for current string

MFREE(curstr)
CHECK_MEM( curstr = (char *)malloc(2*fn*sizeof(char)) )
MFREE(curi)
CHECK_MEM( curi = (char *)malloc(2*fn*sizeof(char)) )
MFREE(curd)
CHECK_MEM( curd = (char *)malloc(2*fn*n*sizeof(char)) )
MFREE(asciiString)
CHECK_MEM( asciiString = (char *)malloc(2*fn*sizeof(char)) )
MFREE(bestSeen)
CHECK_MEM( bestSeen = (char *)malloc(2*fn*sizeof(char)) )

//	Storage for things associated with different numbers of wasted characters

maxW = fn;

MFREE(mperm_res)
CHECK_MEM( mperm_res = (int *)malloc(maxW*sizeof(int)) )

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

MFREE(valid)
CHECK_MEM( valid = (char *)malloc(maxInt*sizeof(char)) )
MFREE(unvisited)
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
	
for (int i=0;i<n;i++) free(permTab[i]);
free(permTab);
	
//	For each number d_1 d_2 d_3 ... d_n as a digit sequence, what is
//	the length of the longest run d_j ... d_n in which all the digits are distinct.

//	Also, record which 1-cycle each permutation belongs to

MFREE(ldd)
CHECK_MEM( ldd = (int *)malloc(maxInt*sizeof(int)) )

noc = fac(n-1);
nocThresh = noc/2;
MFREE(oneCycleCounts)
CHECK_MEM( oneCycleCounts = (int *)malloc(maxInt*sizeof(int)) )
MFREE(oneCycleIndices)
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

MFREE(nextDigits)
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
}

void doTask()
{
tot_bl = currentTask.w_value;

//	Initialise all permutations as unvisited

for (int i=0; i<maxInt; i++) unvisited[i] = TRUE;

//	Initialise 1-cycle information

for (int i=0;i<maxInt;i++) oneCycleCounts[i]=n;
for (int b=0;b<n;b++) oneCycleBins[b]=0;
oneCycleBins[n]=noc;

//	Start the current string with the specified prefix;
//	this will involve visiting various permutations and changes to 1-cycle counts

int tperm0=0;
int pf=0;
for (int j0=0;j0<currentTask.prefixLen;j0++)
	{
	int d = currentTask.prefix[j0]-'0';
	curstr[j0] = d;
	tperm0 = (tperm0>>DBITS) | (d << nmbits);
	if (valid[tperm0])
		{
		if (unvisited[tperm0]) pf++;
		unvisited[tperm0] = FALSE;
		
		int prevC, oc;
		oc=oneCycleIndices[tperm0];
		prevC = oneCycleCounts[oc]--;
		oneCycleBins[prevC]--;
		oneCycleBins[prevC-1]++;
		};
	};
int partNum0 = tperm0>>DBITS;

//	Maybe track 1-cycle counts

ocpTrackingOn = tot_bl >= ocpThreshold[n];
ocpTrackingOff = !ocpTrackingOn;

//	Set baseline times

nodesChecked = 0;
time(&startedCurrentTask);
time(&timeOfLastCheckin);

//	Recursively fill in the string

done=FALSE;
max_perm = currentTask.perm_to_exceed;
bestSeenP=0;
fillStr(currentTask.prefixLen,pf,partNum0);

//	Finish with current task with the server

sprintf(buffer,"Finished current search, bestSeenP=%d",bestSeenP);
logString(buffer);

for (int k=0;k<bestSeenLen;k++) asciiString[k] = '0'+bestSeen[k];
asciiString[bestSeenLen] = '\0';

sprintf(buffer,"action=finishTask&id=%u&access=%u&str=%s&pro=%u",
	currentTask.task_id, currentTask.access_code, asciiString, max_perm+1);
sendServerCommandAndLog(buffer);

free(currentTask.prefix);
}

// this function recursively fills the string

void fillStr(int pos, int pfound, int partNum)
{
if (done) return;

if (pfound > bestSeenP)
	{
	bestSeenP = pfound;
	bestSeenLen = pos;
	for (int i=0;i<bestSeenLen;i++) bestSeen[i] = curstr[i];
	};
	
if (++nodesChecked >= nodesBeforeTimeCheck && pos > currentTask.prefixLen)
	{
	//	We have hit a threshold for nodes checked, so time to check the time
	
	time_t t;
	time(&t);
	double elapsedTime = difftime(t, timeOfLastCheckin);
	nodesBeforeTimeCheck = (unsigned long int) ((TIME_BETWEEN_SERVER_CHECKINS / elapsedTime) * nodesBeforeTimeCheck);
	if (nodesBeforeTimeCheck <= MIN_NODES_BEFORE_TIME_CHECK) nodesBeforeTimeCheck = MIN_NODES_BEFORE_TIME_CHECK;
	else if (nodesBeforeTimeCheck >= MAX_NODES_BEFORE_TIME_CHECK) nodesBeforeTimeCheck = MAX_NODES_BEFORE_TIME_CHECK;
	
	nodesChecked = 0;
	
	if (elapsedTime > 0.9 * TIME_BETWEEN_SERVER_CHECKINS)
		{
		//	We have hit a threshold for elapsed time since last check in with the server
		
		timeOfLastCheckin = t;
		
		sprintf(buffer,"action=checkIn&id=%u&access=%u",
			currentTask.task_id, currentTask.access_code);
		sendServerCommandAndLog(buffer);
		
		//	Also check for current maximum for the (n,w) pair we are working on
		
		max_perm = getMax(currentTask.n_value, currentTask.w_value, max_perm);
		if (max_perm+1 >= currentTask.prev_perm_ruled_out)
			{
			done=TRUE;
			return;
			};
		
		elapsedTime = difftime(t, startedCurrentTask);
		if (elapsedTime > TIME_BEFORE_SPLIT)
			{
			//	We have hit a threshold for elapsed time since we started this task, so split the task
			
			startedCurrentTask = t;
			
			logString("Splitting current task ...");
			
			//	Number of digit choices that led to the sub-branch we are in now
			
			int ourBranchC = curi[currentTask.prefixLen];
			
			//	We split the task with the original prefix into (n-1) new tasks
			//	where that prefix is followed by every possible non-repeated digit.
			
			//	We finalise those new tasks whose sub-branches we have already fully traversed,
			//	and then continue in the current sub-branch under the banner of a new task
			//	with a longer prefix.
			
			//	The server will allocate the remaining digits to other tasks.
			
			static char retainedDigits[MAX_N+1];
			for (int z=0;z<ourBranchC;z++) retainedDigits[z]='0'+curd[n*currentTask.prefixLen + z];
			retainedDigits[ourBranchC]='\0';
			
			splitTask(retainedDigits);
			};
		};
	};

int tperm, ld;
int alreadyWasted = pos - pfound - n + 1;	//	Number of character wasted so far
int spareW = tot_bl - alreadyWasted;		//	Maximum number of further characters we can waste while not exceeding tot_bl

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

curi[pos] = 0;
char *cd = curd+n*pos;

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
	
	cd[curi[pos]++] = curstr[pos] = ndz->digit;
	tperm = ndz->fullNum;
	
	int vperm = (ld==0);
	if (vperm && unvisited[tperm])
		{
		if (pfound+1>max_perm)
			{
			max_perm = pfound+1;
			witnessCurrentString(pos+1);

			if (pfound+1 > bestSeenP)
				{
				bestSeenP = pfound+1;
				bestSeenLen = pos+1;
				for (int i=0;i<bestSeenLen;i++) bestSeen[i] = curstr[i];
				};

			if (max_perm+1 >= currentTask.prev_perm_ruled_out)
				{
				done=TRUE;
				return;
				};
			};

		unvisited[tperm]=FALSE;
		if (ocpTrackingOn)
			{
			int prevC=0, oc=0;
			oc=oneCycleIndices[tperm];
			prevC = oneCycleCounts[oc]--;
			oneCycleBins[prevC]--;
			oneCycleBins[prevC-1]++;
		
			fillStr(pos+1, pfound+1, ndz->nextPart);
		
			oneCycleBins[prevC-1]--;
			oneCycleBins[prevC]++;
			oneCycleCounts[oc]=prevC;
			}
		else
			{
			fillStr(pos+1, pfound+1, ndz->nextPart);
			};
		unvisited[tperm]=TRUE;
		}
	else if	(spareW > 0)
		{
		if (vperm)
			{
			deferredRepeat=TRUE;
			swap12 = !unvisited[nd[1].nextPerm];
			}
		else
			{
			int d = pruneOnPerms(spareW0, pfound - max_perm);
			if	(d > 0)
				{
				fillStr(pos+1, pfound, ndz->nextPart);
				}
			else
				{
				break;
				};
			};
		};
		
	if (pos < currentTask.prefixLen)
		{
		done=TRUE;
		return;
		};
	};
	
//	If we encountered a choice that led to a repeat visit to a permutation, we follow (or prune) that branch now.
//	It will always come from the FIRST choice in the original list, as that is where any valid permutation must be.
	
if (deferredRepeat)
	{
	int d = pruneOnPerms(spareW-1, pfound - max_perm);
	if	(d>0)
		{
		cd[curi[pos]++] = curstr[pos] = nd->digit;
		fillStr(pos+1, pfound, nd->nextPart);
		if (pos < currentTask.prefixLen)
			{
			done=TRUE;
			return;
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

void witnessCurrentString(int size)
{
//	Convert current digit string to null-terminated ASCII string

for (int k=0;k<size;k++) asciiString[k] = '0'+curstr[k];
asciiString[size] = '\0';

//	Log the new best string locally

sprintf(buffer, "Found %d permutations in string %s", max_perm, asciiString);
logString(buffer);

//	Log it with the server

sprintf(buffer,"action=witnessString&n=%u&w=%u&str=%s",n,tot_bl,asciiString);
sendServerCommandAndLog(buffer);
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
		return res0;
		};
	};
return res0;
}

//	Sleep for a specified number of seconds

void sleepForSecs(int secs)
{
#ifdef _WIN32

Sleep(secs*1000);

#else

sleep(secs);

#endif
}

//	Log a string, accompanied by a time stamp, to both the console and the log file

void logString(const char *s)
{
//	Prepare time stamp

struct tm *ct;
time_t t1;
time(&t1);
ct = localtime(&t1);
static char tsb[30];
char *ts = asctime(ct);
strcpy(tsb,ts);
unsigned long int tlen = strlen(tsb);
if (tsb[tlen-1]=='\n') tsb[tlen-1]='\0';

//	Output string with time stamps

printf("%s %s\n",tsb, s);

FILE *fp = fopen(LOG_FILE_NAME,"at");
if (fp==NULL)
	{
	printf("Error: Unable to open log file %s to append\n",LOG_FILE_NAME);
	exit(EXIT_FAILURE);
	};
fprintf(fp,"%s %s\n",tsb, s);
fclose(fp);
}

//	Send a command string via URL_UTILITY to the server at SERVER_URL, putting the response in the file SERVER_RESPONSE_FILE_NAME
//
//	Returns non-zero if an error was encountered 

int sendServerCommand(const char *command)
{
//	Pre-empty the response file so it does not end up with any misleading content from a previous command if the
//	current command fails.

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"wt");
if (fp==NULL)
	{
	printf("Error: Unable to write to server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};
fclose(fp);

unsigned long int ulen = strlen(URL_UTILITY);
unsigned long int slen = strlen(SERVER_URL);
unsigned long int clen = strlen(command);
unsigned long int flen = strlen(SERVER_RESPONSE_FILE_NAME);
unsigned long int len = ulen+slen+clen+flen+10;
char *cmd;
CHECK_MEM( cmd = malloc(len*sizeof(char)) )
sprintf(cmd,"%s \"%s%s\" > %s",URL_UTILITY,SERVER_URL,command,SERVER_RESPONSE_FILE_NAME);
int res = system(cmd);
free(cmd);
return res;
}

//	Read the server's response into the log;
//	If first argument is not null, it is a string that the first line must match.
//
//	Return TRUE if all OK, FALSE if any line starts with "Error"

int logServerResponse(const char *reqd)
{
int result = TRUE;
FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Unable to read from server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};

int lineNumber = 0;
while (!feof(fp))
	{
	//	Get a line from the server response, ensure it is null-terminated without a newline
	
	char *f=fgets(buffer,BUFFER_SIZE,fp);
	if (f==NULL) break;
	unsigned long int blen = strlen(buffer);
	if (buffer[blen-1]=='\n')
		{
		buffer[blen-1]='\0';
		blen--;
		};
	lineNumber++;
		
	if (strncmp(buffer,"Error",5)==0) result=FALSE;
	
	if (lineNumber==1 && reqd!=NULL && strncmp(buffer,reqd,strlen(reqd))!=0) result=FALSE;
	
	sprintf(lbuffer,"Server: %s",buffer);
	logString(lbuffer);
	};
	
fclose(fp);
return result;
}

int parseTaskParameters(const char *s, unsigned long int slen, struct task *tsk, int taskItems, int *tif)
{
//	Look for a task parameter

for (int i=0;i<N_TASK_STRINGS;i++)
	{
	unsigned long int len = strlen(taskStrings[i]);
	if (strncmp(s,taskStrings[i],len)==0)
		{
		switch(i)
			{
			case 0:
				sscanf(s+len,"%u",&tsk->task_id);
				break;
			
			case 1:
				sscanf(s+len,"%u",&tsk->access_code);
				break;
			
			case 2:
				sscanf(s+len,"%u",&tsk->n_value);
				setupForN(tsk->n_value);
				break;
			
			case 3:
				sscanf(s+len,"%u",&tsk->w_value);
				break;
			
			case 4:
				CHECK_MEM( tsk->prefix = malloc((slen-len+1)*sizeof(char)) )
				strcpy(tsk->prefix, s+len);
				tsk->prefixLen = (unsigned int)strlen(tsk->prefix);
				break;
			
			case 5:
				sscanf(s+len,"%u",&tsk->perm_to_exceed);
				break;
			
			case 6:
				sscanf(s+len,"%u",&tsk->prev_perm_ruled_out);
				break;
			
			default:
				break;
			};
		
		if (tif)
			{
			if (!tif[i]) taskItems++;
			tif[i]=TRUE;
			}
		else taskItems++;
		
		break;
		};
	};
	
return taskItems;
}

//	Check with the server to try to get a task
//	Returns:
//
//	0	no task available
//	-1	quit instruction from server
//	n	n value for task

int getTask(struct task *tsk)
{
sendServerCommandAndLog("action=getTask");

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Unable to read from server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};

int quit=FALSE, taskItems=0;
static int tif[N_TASK_STRINGS];
for (int i=0;i<N_TASK_STRINGS;i++) tif[i]=FALSE;

while (!feof(fp))
	{
	//	Get a line from the server response, ensure it is null-terminated without a newline
	
	char *f=fgets(buffer,BUFFER_SIZE,fp);
	if (f==NULL) break;
	unsigned long int blen = strlen(buffer);
	if (buffer[blen-1]=='\n')
		{
		buffer[blen-1]='\0';
		blen--;
		};
	
	if (strncmp(buffer,"Quit",4)==0)
		{
		quit=TRUE;
		break;
		};
	
	if (strncmp(buffer,"No tasks",8)==0)
		{
		break;
		};
	
	if (taskItems < N_TASK_STRINGS)
		{
		taskItems = parseTaskParameters(buffer, blen, tsk, taskItems, tif);
		}
	else
		{
		//	Look for a (w,p) pair
		
		if (buffer[0]=='(')
			{
			int w, p;
			sscanf(buffer+1,"%d,%d",&w,&p);
			mperm_res[w]=p;
			};
		};
		
	};
fclose(fp);

if (quit) return -1;
if (taskItems == N_TASK_STRINGS) return tsk->n_value;
return 0;
}

void sendServerCommandAndLog(const char *s)
{
sprintf(lbuffer,"To server: %s",s);
logString(lbuffer);

while (TRUE)
	{
	int srep = sendServerCommand(s);
	if (srep==0) break;
	logString("Unable to send command to server, will retry shortly");
	sleepForSecs(TIME_BETWEEN_SERVER_CHECKINS);
	};

if (!logServerResponse(NULL))
	{
	exit(EXIT_FAILURE);
	};
}

int getMax(int nval, int wval, int oldMax)
{
sprintf(buffer,"action=checkMax&n=%d&w=%d",	nval, wval);
sendServerCommandAndLog(buffer);

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Unable to read from server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};

int max = oldMax;
while (!feof(fp))
	{
	//	Get a line from the server response, ensure it is null-terminated without a newline
	
	char *f=fgets(buffer,BUFFER_SIZE,fp);
	if (f==NULL) break;
	unsigned long int blen = strlen(buffer);
	if (buffer[blen-1]=='\n')
		{
		buffer[blen-1]='\0';
		blen--;
		};
	
	if (buffer[0]=='(')
		{
		int n0, w0, p0;
		sscanf(buffer+1,"%d,%d,%d",&n0,&w0,&p0);
		if (n0==nval && w0==wval && p0 > oldMax)
			{
			max = p0;
			sprintf(buffer,"Updated max_perm to %d",max);
			logString(buffer);
			};
		break;
		};
	};
fclose(fp);

return max;
}

//	Split the current task

//	We split the task with the original prefix into (n-1) new tasks
//	where that prefix is followed by every possible non-repeated digit.

//	We finalise those new tasks whose sub-branches we have already fully traversed,
//	and then continue in the current sub-branch under the banner of a new task
//	with a longer prefix.

//	The server will allocate the remaining digits to other tasks.

void splitTask(const char *retainedDigits)
{
unsigned long int nrd = strlen(retainedDigits);

sprintf(buffer,"action=splitTask&id=%u&access=%u&retain=%s",
	currentTask.task_id, currentTask.access_code,retainedDigits);
sendServerCommandAndLog(buffer);

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Unable to read from server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};
	
int taskCount = 0;
static struct task ftask[MAX_N];
static int finalise[MAX_N];
for (int i=0;i<MAX_N;i++) finalise[i]=FALSE;

int taskItems=0;
static int tif[N_TASK_STRINGS];
for (int i=0;i<N_TASK_STRINGS;i++) tif[i]=FALSE;

while (!feof(fp))
	{
	//	Get a line from the server response, ensure it is null-terminated without a newline
	
	char *f=fgets(buffer,BUFFER_SIZE,fp);
	if (f==NULL) break;
	unsigned long int blen = strlen(buffer);
	if (buffer[blen-1]=='\n')
		{
		buffer[blen-1]='\0';
		blen--;
		};
		
	//	See if line contains any task parameters
	
	taskItems = parseTaskParameters(buffer, blen, &ftask[taskCount], taskItems, tif);
	
	//	If we have three, that should be Task id / Access code / str, indices 0, 1, 4 in list.
	
	if (taskItems==3)
		{
		if (tif[0] && tif[1] && tif[4])
			{
			if (ftask[taskCount].prefix[ftask[taskCount].prefixLen-1] == retainedDigits[nrd-1])
				{
				//	This is the current sub-branch, which we will continue to execute
				//	as a new task.
				
				currentTask.task_id = ftask[taskCount].task_id;
				currentTask.access_code = ftask[taskCount].access_code;
				free(currentTask.prefix);
				currentTask.prefix = ftask[taskCount].prefix;
				currentTask.prefixLen = ftask[taskCount].prefixLen;
				}
			else
				{
				finalise[taskCount] = TRUE;
				};
			
			taskItems = 0;
			for (int i=0;i<N_TASK_STRINGS;i++) tif[i]=FALSE;
			taskCount++;
			}
		else
			{
			printf("Error: Server response for splitTask could not be parsed\n");
			exit(EXIT_FAILURE);
			};
		};
		
	};

fclose(fp);

//	Now finalise the tasks for sub-branches we traversed.

for (int t=0;t<taskCount;t++)
if (finalise[t])
	{
	for (int k=0;k<bestSeenLen;k++) asciiString[k] = '0'+bestSeen[k];
	asciiString[bestSeenLen] = '\0';

	sprintf(buffer,"action=finishTask&id=%u&access=%u&str=%s&pro=%u&split=Y",
	ftask[t].task_id, ftask[t].access_code, asciiString, max_perm+1);
	sendServerCommandAndLog(buffer);

	free(ftask[t].prefix);
	};

}
