/*

DistributedChaffinMethod.c
==========================

Author: Greg Egan
Version: 1 to 8, 10 - 13

Secondary Author: Jay Pantone
Version: 9

Current version: 13.0
Last Updated: 21 May 2019

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

For more details, see the accompanying README.

*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32

//	Compiling for Windows

#include <windows.h>
#include <process.h>
#include <io.h>
#define UNIX_LIKE FALSE

#else

//	Compiling for MacOS or Linux

#include <unistd.h>
#include <signal.h>

#define TRUE (1==1)
#define FALSE (1==0)
#define UNIX_LIKE TRUE

#endif

//	Constants
//	---------

//	Server URL

#define SERVER_URL "http://supermutations.net/ChaffinMethod.php?version=13&"

//	Choose whether to use an "InstanceCount" file on the server to avoid running more than one PHP process at once

#define USE_SERVER_INSTANCE_COUNTS FALSE

//	Choose whether to use a "server lock" file on the client computer to coordinate server access between multiple client instances

#define USE_SERVER_LOCK_FILE FALSE

//	Set NO_SERVER to TRUE to run a debugging version that makes no contact with the server, and just
//	performs a search with parameters that need to be inserted into the source code (in getTask()) manually.

#define NO_SERVER FALSE 

#if USE_SERVER_INSTANCE_COUNTS

//	URL for InstanceCount file

	#define IC_URL "http://www.supermutations.net/InstanceCount.txt"
	
#endif

#if USE_SERVER_LOCK_FILE

//	Name of server lock file shared by sibling clients running with a shared working directory

	#define SERVER_LOCK_FILE_NAME "ServerLock.txt"
	
//	Maximum number of times we sleep consecutively waiting for siblings to free the lock on the server;
//	if this is exceeded, we assume something has gone awry with the lock file.

	#define MAX_SLEEP_WAITING_ON_SIBS 20
	
#endif

//	Smallest and largest values of n accepted

//	(Note that if we went higher than n=7, DBITS would need to increase, and we would also need to change some variables
//	from 32-bit to 64-bit ints. However, at this point it seems extremely unlikely that it would ever be practical to
//	run this code with n=8.)

#define MIN_N 3
#define MAX_N 7

//	Number of bits allowed for each digit in integer representation of permutations

#define DBITS 3

//	Command-line utility that gets response from a supplied URL
//	Current choice is curl (with options to suppress progress meter but display any errors)

#define URL_UTILITY "curl --silent --show-error "

//	Name of temporary file in which response from server is placed

#define SERVER_RESPONSE_FILE_NAME_TEMPLATE "DCMServerResponse_%u.txt"

//	Name of log file

#define LOG_FILE_NAME_TEMPLATE "DCMLog_%u.txt"

//	Name of stop files

#define STOP_FILE_NAME_TEMPLATE "STOP_%u.txt"
#define STOP_FILE_ALL "STOP_ALL.txt"

//	Max size of file names

#define FILE_NAME_SIZE 128

//	Times

#define MINUTE 60

//	Time we AIM to spend between system calls to check on the time;
//	we count nodes between these checks

#define TIME_BETWEEN_TIME_CHECKS (5)

//	Time for (short) delays when the server wants us to wait

#define MIN_SERVER_WAIT 2
#define VAR_SERVER_WAIT 4

//	Default time we AIM to check in with the server
//	NB:  This is independent of time until splitting and time spent on each subtree

#define DEFAULT_TIME_BETWEEN_SERVER_CHECKINS (3*MINUTE)

//	Default time to spend on a prefix before splitting the search

#define DEFAULT_TIME_BEFORE_SPLIT (20*MINUTE)

//	Default maximum time to spend exploring a subtree when splitting

#define DEFAULT_MAX_TIME_IN_SUBTREE (2*MINUTE)

//	When the time spent on a task exceeds this threshold, we start exponentially reducing the number of nodes
//	we explore in each subtree, with an (1/e)-life given

#define TAPER_THRESHOLD (60*MINUTE)

#define TAPER_DECAY (5*MINUTE)

//	Initial number of nodes to check before we bother to check elapsed time;
//	we rescale the actual value (in nodesBeforeTimeCheck) if it is too large or too small

#define NODES_BEFORE_TIME_CHECK 20000000L

//	Set a floor and ceiling so we can't waste an absurd amount of time doing time checks,
//	or take too long between them.

#define MIN_NODES_BEFORE_TIME_CHECK 10000000L
#define MAX_NODES_BEFORE_TIME_CHECK 1000000000L

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
char *prefix, *branchOrder;
unsigned int prefixLen, branchOrderLen;
unsigned int perm_to_exceed;
unsigned int prev_perm_ruled_out;
unsigned int timeBeforeSplit;
unsigned int maxTimeInSubtree;
unsigned int timeBetweenServerCheckins;
};

//	Global variables
//	----------------

unsigned int programInstance;			//	Random number that individualises this instance of the program running on same computer
unsigned int clientID;			//	Client ID with server
char *ipAddress;			//	IP address
int n=-1;					//	The number of symbols in the permutations we are considering
int fn;						//	n!
int nm;						//	n-1
int nmbits;					//	(n-1)*DBITS
int maxInt;					//	Highest integer representation of an n-digit sequence we can encounter, plus 1
int maxIntM;				//	Highest integer representation of an (n-1)-digit sequence we can encounter, plus 1
int maxW;					//	Largest number of wasted characters we allow for
char *curstr=NULL;			//	Current string as integer digits
char *curi=NULL;
char *bestSeen=NULL;		//	Longest string seen in search, as integer digits
int bestSeenLen, bestSeenP;
int *successor1;			//	For each permutation, its weight-1 successor
int *successor2;			//	For each permutation, its weight-2 successor
int *klbLen;				//	For each number of wasted characters, the lengths of the strings that visit known-lower-bound permutations
char **klbStrings;			//	For each number of wasted characters, a list of all strings that visit known-lower-bound permutations
char *asciiString=NULL;		//	String as ASCII digits
char *asciiString2=NULL;		//	String as ASCII digits
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
int splitMode=FALSE;		//	Set TRUE when we are splitting the task
int isSuper=FALSE;			//	Set TRUE when we have found a superpermutation
int cancelledTask=FALSE;	//	Set TRUE when the server cancelled our connection to the task

//	Monitoring 1-cycle tracking

int ocpTrackingOn, ocpTrackingOff;

//	For n=0,1,2,3,4,5,6,7
int ocpThreshold[]={1000,1000,1000,1000, 6, 24, 120, 720};

struct task currentTask;

#define N_TASK_STRINGS 11
#define N_TASK_STRINGS_OBLIGATORY 8
char *taskStrings[] = {"Task id: ","Access code: ","n: ","w: ","str: ","pte: ","pro: ","branchOrder: ", "timeBeforeSplit: ", "maxTimeInSubtree: ","timeBetweenServerCheckins: "};

#define N_CLIENT_STRINGS 3
char *clientStrings[] = {"Client id: ", "IP: ","programInstance: "};

int64_t totalNodeCount, subTreesSplit, subTreesCompleted;
int64_t nodesChecked;		//	Count of nodes checked since last time check
int64_t nodesBeforeTimeCheck = NODES_BEFORE_TIME_CHECK;
int64_t nodesToProbe0, nodesToProbe, nodesLeft;
time_t startedRunning;				//	Time program started running
time_t startedCurrentTask=0;		//	Time we started current task
time_t timeOfLastTimeCheck;			//	Time we last checked the time
time_t timeOfLastTimeReport;		//	Time we last reported elapsed time to the user
time_t timeOfLastServerCheckin;		//	Time we last contacted the server

int timeBetweenServerCheckins = DEFAULT_TIME_BETWEEN_SERVER_CHECKINS;
int timeBeforeSplit = DEFAULT_TIME_BEFORE_SPLIT;
int maxTimeInSubtree = DEFAULT_MAX_TIME_IN_SUBTREE;

#if UNIX_LIKE

//	Signal action structure

struct sigaction sigIntAction;
int hadSigInt;

//	Flag to say we should use a lock file to stop siblings trying to talk to server simultaneously

int useServerLock;

//	File descriptor for server lock

int serverLockFD;


#endif

static char SERVER_RESPONSE_FILE_NAME[FILE_NAME_SIZE];
static char LOG_FILE_NAME[FILE_NAME_SIZE];
static char STOP_FILE_NAME[FILE_NAME_SIZE];

//	Time quota, in minutes
//	The default is for the "timeLimit" / "timeLimitHard "option with no argument;
//	default behaviour is to keep running indefinitely

#define DEFAULT_TIME_LIMIT 120
int timeQuotaMins=0, timeQuotaHardMins=0, timeQuotaEitherMins=0;

//  Default team name
#define DEFAULT_TEAM_NAME "anonymous"
#define MAX_TEAM_NAME_LENGTH 32
char *teamName;



//	Known values from previous calculations

int known3[][2]={{0,3},{1,6}};
int known4[][2]={{0,4},{1,8},{2,12},{3,14},{4,18},{5,20},{6,24}};
int known5[][2]={{0,5},{1,10},{2,15},{3,20},{4,23},{5,28},{6,33},{7,36},{8,41},{9,46},{10,49},{11,53},{12,58},{13,62},{14,66},{15,70},{16,74},{17,79},{18,83},{19,87},{20,92},{21,96},{22,99},{23,103},{24,107},{25,111},{26,114},{27,116},{28,118},{29,120}};
int known6[][2]={{0,6},{1,12},{2,18},{3,24},{4,30},{5,34},{6,40},{7,46},{8,52},{9,56},{10,62},{11,68},{12,74},{13,78},{14,84},{15,90},{16,94},{17,100},{18,106},{19,112},{20,116},{21,122},{22,128},{23,134},{24,138},{25,144},{26,150},{27,154},{28,160},{29,166},{30,172},{31,176},{32,182},{33,188},{34,192},{35,198},{36,203},{37,209},{38,214},{39,220},{40,225},{41,230},{42,236},{43,241},{44,246},{45,252},{46,257},{47,262},{48,268},{49,274},{50,279},{51,284},{52,289},{53,295},{54,300},{55,306},{56,311},{57,316},{58,322},{59,327},{60,332},{61,338},{62,344},{63,349},{64,354},{65,360},{66,364},{67,370},{68,375},{69,380},{70,386},{71,391},{72,396},{73,402},{74,407},{75,412},{76,418},{77,423},{78,429},{79,434},{80,439},{81,445},{82,450},{83,455},{84,461},{85,465},{86,470},{87,476},{88,481},{89,486},{90,492},{91,497},{92,502},{93,507},{94,512},{95,518},{96,523},{97,528},{98,534},{99,539},{100,543},{101,548},{102,552},{103,558},{104,564},{105,568},{106,572},{107,578},{108,583},{109,589},{110,594},{111,599},{112,604},{113,608},{114,613},{115,618}};

int *knownN=NULL;
int numKnownW=0;


//	Function definitions
//	--------------------

void logString(const char *s);
void sleepForSecs(int secs);
void setupForN(int nval);
int sendServerCommand(const char *command);
int sendServerCommandAndLog(const char *s, const char **responseList, int nrl);
int logServerResponse(const char **responseList, int nrl);
void registerClient(void);
void unregisterClient(void);
void relinquishTask(void);
int getTask(struct task *tsk);
int getMax(int nval, int wval, int oldMax, unsigned int tid, unsigned int acc, unsigned int cid, char *ip, unsigned int pi);
void doTask(void);
int checkIn(void);
int splitTask(int pos);
void fillStr(int pos, int pfound, int partNum);
int fillStrNL(int pos, int pfound, int partNum);
int fac(int k);
void makePerms(int n, int **permTab);
void witnessCurrentString(int size);
void witnessLowerBound(char *s, int size, int w, int p);
void maybeUpdateLowerBound(int tperm, int size, int w, int p);
void maybeUpdateLowerBoundSplice(int size, int w, int p);
void maybeUpdateLowerBoundAppend(int tperm, int size, int w, int p);
int compareDS(const void *ii0, const void *jj0);
void rClassMin(int *p, int n);
int pruneOnPerms(int w, int d0);
int getServerInstanceCount(void);
void sigIntHandler(int a);
void sleepUntilSiblingsFreeServer(void);
void releaseServerLock(void);
void nodesAndTime(void);

//	Main program
//	------------

int main(int argc, const char * argv[])
{
static char buffer[BUFFER_SIZE];
FILE *fp;
int justTest=FALSE;
timeQuotaMins=0;
timeQuotaHardMins=0;
teamName = DEFAULT_TEAM_NAME;
currentTask.task_id = 0;


//	Choose a random number to identify this instance of the program;
//	this also individualises the log file and the server response file.


time(&startedRunning);

#ifdef _WIN32
	int rseed=(int)( (startedRunning + clock() + _getpid()) % (1<<31) );
#else
	int rseed=(int)( (startedRunning + clock()  + (int) getpid()) % (1<<31) );
#endif


printf("Random seed is: %d\n", rseed);
srand(rseed);

while(TRUE)
	{
	programInstance=rand();
	sprintf(SERVER_RESPONSE_FILE_NAME,SERVER_RESPONSE_FILE_NAME_TEMPLATE,programInstance);
	sprintf(LOG_FILE_NAME,LOG_FILE_NAME_TEMPLATE,programInstance);
	sprintf(STOP_FILE_NAME,STOP_FILE_NAME_TEMPLATE,programInstance);
	
	//	Check for pre-existing files to avoid any collisions
	
	fp = fopen(LOG_FILE_NAME,"r");
	if (fp!=NULL)
		{
		fclose(fp);
		continue;
		};
	fp = fopen(SERVER_RESPONSE_FILE_NAME,"r");
	if (fp!=NULL)
		{
		fclose(fp);
		continue;
		};
		
	break;
	};
	
sprintf(buffer,"Program instance number: %u",programInstance);
logString(buffer);

//	Process command line arguments

for (int i=1;i<argc;i++)
	{
	if (strcmp(argv[i],"test")==0) justTest=TRUE;
	else if (strcmp(argv[i],"timeLimit")==0)
		{
		if (i+1<argc)
			{
			if (sscanf(argv[i+1],"%d",&timeQuotaMins) != 1 || timeQuotaMins <= 0) timeQuotaMins = DEFAULT_TIME_LIMIT;
			else i++;
			}
		else timeQuotaMins = DEFAULT_TIME_LIMIT;
		
		sprintf(buffer,"After the program has run for a time limit of %d minutes, it will wait to finish the current task, then quit.\n",timeQuotaMins);
		logString(buffer);
		if (timeQuotaEitherMins==0 || (timeQuotaMins < timeQuotaEitherMins)) timeQuotaEitherMins = timeQuotaMins;
		}
	else if (strcmp(argv[i],"timeLimitHard")==0)
		{
		if (i+1<argc)
			{
			if (sscanf(argv[i+1],"%d",&timeQuotaHardMins) != 1 || timeQuotaHardMins <= 0) timeQuotaHardMins = DEFAULT_TIME_LIMIT;
			else i++;
			}
		else timeQuotaHardMins = DEFAULT_TIME_LIMIT;
		
		sprintf(buffer,"After the program has run for a time limit of %d minutes, it will quit within no more than 5 minutes, even if it is in the middle of a task.\n",timeQuotaHardMins);
		logString(buffer);
		if (timeQuotaEitherMins==0 || (timeQuotaHardMins < timeQuotaEitherMins)) timeQuotaEitherMins = timeQuotaHardMins;
		}
	else if (strcmp(argv[i],"team")==0)
		{
		if (i+1<argc) {
			if (strlen(argv[i+1]) <= MAX_TEAM_NAME_LENGTH) {
				teamName = (char *) argv[i+1];
				for (int pos=0; teamName[pos]!= '\0'; pos++) {
					if (!isalpha(teamName[pos]) && !isdigit(teamName[pos]) && (teamName[pos] != ' ')) {
						printf("Team names must contain only alphanumeric ascii characters and spaces.\n");
						exit(EXIT_FAILURE);
					}
				}
				// We have to encode spaces in the team name as "%20" to pass them via curl
				// From https://stackoverflow.com/questions/3424474/replacing-spaces-with-20-in-c

				int new_string_length = 0;
				for (char *c = teamName; *c != '\0'; c++) {
					if (*c == ' ') new_string_length += 2;
					new_string_length++;
				}

				char *qstr = malloc((new_string_length + 1) * sizeof qstr[0]);
				char *c1, *c2;
				for (c1 = teamName, c2 = qstr; *c1 != '\0'; c1++) {
					if (*c1 == ' ') {
						c2[0] = '%';
						c2[1] = '2';
						c2[2] = '0';
						c2 += 3;
					}else{
						*c2 = *c1;
						c2++;
					}
				}
				*c2 = '\0';
				teamName = qstr;

			} else {
				printf("Team names are limited to %d characters.\n", MAX_TEAM_NAME_LENGTH);
				exit(EXIT_FAILURE);
			}
			i++;
		}
		else teamName = DEFAULT_TEAM_NAME;
		
		sprintf(buffer,"Team name set to %s.\n",teamName);
		logString(buffer);
		}
	else
		{
		printf("Unknown option %s\n",argv[i]);
		exit(EXIT_FAILURE);
		};
	};

//	First, just check we can establish contact with the server

sprintf(buffer,"Team name: %s",teamName);
logString(buffer);
const char *hwRL[]={"Hello world."};
if (sendServerCommandAndLog("action=hello",hwRL,sizeof(hwRL)/sizeof(hwRL[0])) != 1)
	{
	printf("Did not obtained expected response from server\n");
	exit(EXIT_FAILURE);
	};

if (justTest) exit(0);

//	Register with the server, offering to do actual work

registerClient();

sprintf(buffer,
	"To stop the program automatically between tasks, create a file %s or %s in the working directory\n",
	STOP_FILE_NAME,STOP_FILE_ALL);
logString(buffer);

//	Set up handler for SIGINT (e.g. from CTRL-C)

#if UNIX_LIKE

sigIntAction.sa_handler = sigIntHandler;
sigemptyset(&sigIntAction.sa_mask);
sigaddset(&sigIntAction.sa_mask, SIGINT);
sigIntAction.sa_flags = 0;
if (sigaction(SIGINT, &sigIntAction,NULL)==0)
	logString("CTRL-C / SIGINT will be trapped:\n"
	"1 x CTRL-C will quit when current task is finished\n"
	"3 x CTRL-C will relinquish the current task with the server, then quit\n"
	"7 x CTRL-C will quit immediately\n"
	);
else logString("Unable to set a handler for CTRL-C / SIGINT, so these actions will kill the program immediately.\n");
hadSigInt=0;

//	Flag to say we should use a lock file to stop siblings trying to talk to server simultaneously

useServerLock = TRUE;

#endif

startedCurrentTask=0;

while (TRUE)
	{
	//	Check for CTRL-C / SIGINT
	
	#if UNIX_LIKE
	
	if (hadSigInt>0)
		{
		logString("Received CTRL-C / SIGINT, so stopping.\n");
		unregisterClient();
		exit(0);
		};
	
	#endif
	
	//	Check for STOP files
	
	fp = fopen(STOP_FILE_NAME,"r");
	if (fp!=NULL)
		{
		sprintf(buffer,"Detected the presence of the file %s, so stopping.\n",STOP_FILE_NAME);
		logString(buffer);
		unregisterClient();
		exit(0);
		};
	
	fp = fopen(STOP_FILE_ALL,"r");
	if (fp!=NULL)
		{
		sprintf(buffer,"Detected the presence of the file %s, so stopping.\n",STOP_FILE_ALL);
		logString(buffer);
		unregisterClient();
		exit(0);
		};
		
	//	Check to see if we exceed time quota
	
	if (timeQuotaEitherMins > 0)
		{
		time_t currentTime;
		time(&currentTime);
		double elapsedTime = difftime(currentTime, startedRunning);
		if (elapsedTime / 60 > timeQuotaEitherMins)
			{
			sprintf(buffer,"Program has exceeded the time quota of %d minutes, so stopping.\n",timeQuotaEitherMins);
			logString(buffer);
			unregisterClient();
			exit(0);
			};
		};
		
	//	If we did a very quick task, maybe sleep
	
	if (startedCurrentTask>0)
		{
		time_t timeNow;
		time(&timeNow);
		int timeSinceLastTask = (int)difftime(timeNow, startedCurrentTask);
		int stime = timeBetweenServerCheckins - timeSinceLastTask;
		if (stime >= 1)
			{
			printf("Sleeping for %d seconds, because the last task completed just %d seconds ago ...\n",
				stime,timeSinceLastTask);
			sleepForSecs(stime);
			startedCurrentTask=0;
			continue;
			};
		};
	
	int t = getTask(&currentTask);
	
	if (t<0)
		{
		logString("Quit instruction from server");
		break;
		};
	
	if (t==0)
		{
		logString("No tasks available");
		sleepForSecs(timeBetweenServerCheckins);
		}
	else
		{
		sprintf(buffer,
			"Assigned new task (id=%u, access=%u, n=%u, w=%u, prefix=%s, perm_to_exceed=%u, prev_perm_ruled_out=%u, timeBeforeSplit=%u seconds, maxTimeInSubtree=%u seconds, timeBetweenServerCheckins=%u seconds)",
			currentTask.task_id,
			currentTask.access_code,
			currentTask.n_value,
			currentTask.w_value,
			currentTask.prefix,
			currentTask.perm_to_exceed,
			currentTask.prev_perm_ruled_out,
			currentTask.timeBeforeSplit,
			currentTask.maxTimeInSubtree,
			currentTask.timeBetweenServerCheckins);
		logString(buffer);
		
		doTask();
		
		#if NO_SERVER
		break;
		#endif
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
MFREE(asciiString)
CHECK_MEM( asciiString = (char *)malloc(2*fn*sizeof(char)) )
MFREE(asciiString2)
CHECK_MEM( asciiString2 = (char *)malloc(2*fn*sizeof(char)) )
MFREE(bestSeen)
CHECK_MEM( bestSeen = (char *)malloc(2*fn*sizeof(char)) )

//	Storage for things associated with different numbers of wasted characters

maxW = fn;

MFREE(mperm_res)
CHECK_MEM( mperm_res = (int *)malloc(maxW*sizeof(int)) )
MFREE(klbLen)
CHECK_MEM( klbLen = (int *)malloc(maxW*sizeof(int)) )
MFREE(klbStrings)
CHECK_MEM( klbStrings = (char **)malloc(maxW*sizeof(char *)) )

//	Storage for known-lower-bound strings

for (int i=0;i<maxW;i++)
	{
	CHECK_MEM( klbStrings[i] = (char *)malloc(2*fn*sizeof(char)))
	klbLen[i] = 0;
	};

if (n==3) {knownN = &known3[0][0]; numKnownW=sizeof(known3)/(sizeof(int))/2;}
else if (n==4) {knownN = &known4[0][0]; numKnownW=sizeof(known4)/(sizeof(int))/2;}
else if (n==5) {knownN = &known5[0][0]; numKnownW=sizeof(known5)/(sizeof(int))/2;}
else if (n==6) {knownN = &known6[0][0]; numKnownW=sizeof(known6)/(sizeof(int))/2;}
else knownN = NULL;

for (int i=0;i<maxW;i++) mperm_res[i]=n;
if (knownN)
	{
	for (int i=0; i<numKnownW; i++) mperm_res[i] = knownN[2*i+1];
	};

#if NO_SERVER
if (knownN==NULL)
	{
	printf("Error: No data available for n=%d\n",n);
	exit(EXIT_FAILURE);
	};
	
if (currentTask.w_value > numKnownW)
	{
	printf("Error: No data available for w=%d\n",currentTask.w_value);
	exit(EXIT_FAILURE);
	};
#endif

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
MFREE(successor1)
CHECK_MEM( successor1 = (int *)malloc(maxInt*sizeof(int)) )
MFREE(successor2)
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
static char buffer[BUFFER_SIZE];

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
	bestSeen[j0] = curstr[j0] = d;
	curi[j0] = currentTask.branchOrder[j0]-'0';
	tperm0 = (tperm0>>DBITS) | (d << nmbits);
	if (valid[tperm0])
		{
		if (unvisited[tperm0])
			{
			pf++;
			unvisited[tperm0] = FALSE;
			
			int prevC, oc;
			oc=oneCycleIndices[tperm0];
			prevC = oneCycleCounts[oc]--;
			if (prevC-1<0 || prevC >n)
				{
				printf("oneCycleBins index is out of range (prevC=%d)\n",prevC);
				exit(EXIT_FAILURE);
				};
			oneCycleBins[prevC]--;
			oneCycleBins[prevC-1]++;
			};
		};
	};
int partNum0 = tperm0>>DBITS;
bestSeenP=pf;
bestSeenLen=currentTask.prefixLen;

//	Maybe track 1-cycle counts

ocpTrackingOn = tot_bl >= ocpThreshold[n];
ocpTrackingOff = !ocpTrackingOn;

//	Set baseline times

nodesChecked = 0;
totalNodeCount = 0;
subTreesSplit = 0;
subTreesCompleted = 0;
time(&startedCurrentTask);
timeOfLastTimeReport = timeOfLastTimeCheck = startedCurrentTask;

timeBeforeSplit = currentTask.timeBeforeSplit;
maxTimeInSubtree = currentTask.maxTimeInSubtree;
timeBetweenServerCheckins = currentTask.timeBetweenServerCheckins;

//	Recursively fill in the string

done=FALSE;
splitMode=FALSE;
cancelledTask=FALSE;
max_perm = currentTask.perm_to_exceed;
isSuper = (max_perm==fn);

if (isSuper || max_perm+1 < currentTask.prev_perm_ruled_out)
	{
	fillStr(currentTask.prefixLen,pf,partNum0);
	};
	
for (int k=0;k<bestSeenLen;k++) asciiString[k] = '0'+bestSeen[k];
asciiString[bestSeenLen] = '\0';

#if !NO_SERVER

//	Finish with current task with the server

if (!cancelledTask)
while (TRUE)
	{
	sprintf(buffer,"action=finishTask&id=%u&access=%u&str=%s&pro=%u&team=%s&nodeCount=%"PRId64,
		currentTask.task_id, currentTask.access_code, asciiString, max_perm+1, teamName, totalNodeCount);
	const char *ftRL[]={"OK","Cancelled"};
	if (sendServerCommandAndLog(buffer,ftRL,sizeof(ftRL)/sizeof(ftRL[0]))>0) break;
	
	sprintf(buffer,"Did not obtained expected response from server, will retry after %d seconds",timeBetweenServerCheckins);
	logString(buffer);
	sleepForSecs(timeBetweenServerCheckins);
	};

free(currentTask.prefix);
free(currentTask.branchOrder);
currentTask.task_id = 0;
#endif

//	Give stats on the task

time_t timeNow;
time(&timeNow);
int tskTime = (int)difftime(timeNow, startedCurrentTask);
int tskMin = tskTime / 60;
int tskSec = tskTime % 60;

sprintf(buffer,"Finished current search, bestSeenP=%d, nodes visited=%"PRId64", time taken=%d min %d sec",
	bestSeenP,totalNodeCount,tskMin,tskSec);
logString(buffer);
if (splitMode)
	{
	sprintf(buffer,"Delegated %"PRId64" sub-trees, completed %"PRId64" locally",subTreesSplit,subTreesCompleted);
	logString(buffer);
	};
sprintf(buffer,"--------------------------------------------------------\n");
logString(buffer);
}

void nodesAndTime()
{
static char buffer[BUFFER_SIZE];

totalNodeCount++;
if (++nodesChecked >= nodesBeforeTimeCheck)
	{
	//	We have hit a threshold for nodes checked, so time to check the time
	
	time_t timeNow;
	time(&timeNow);
	double timeSpentOnTask = difftime(timeNow, startedCurrentTask);
	double timeSinceLastTimeCheck = difftime(timeNow, timeOfLastTimeCheck);
	double timeSinceLastTimeReport= difftime(timeNow, timeOfLastTimeReport);
	double timeSinceLastServerCheckin = difftime(timeNow, timeOfLastServerCheckin);
	
	if (timeQuotaHardMins > 0)
		{
		double elapsedTime = difftime(timeNow, startedRunning);
		if (elapsedTime / 60 > timeQuotaHardMins)
			{
			logString("A 'timeLimitHard' quota has been reached, so the program will relinquish the current task with the server then quit.\n");
			if (currentTask.task_id != 0) unregisterClient();
			exit(0);
			};
		};

	if (timeSinceLastTimeReport > MINUTE)
		{
		int tskTime = (int)timeSpentOnTask;
		int tskMin = tskTime / 60;
		int tskSec = tskTime % 60;

		printf("Time spent on task so far = ");
		if (tskMin==0) printf("       ");
		else printf(" %2d min",tskMin);
		printf(" %2d sec.",tskSec);
		printf("  Nodes searched per second = %"PRId64"\n",(int64_t)((double)nodesBeforeTimeCheck/(timeSinceLastTimeCheck)));
		timeOfLastTimeReport = timeNow;
		};

	//	Adjust the number of nodes we check before doing a time check, to bring the elapsed
	//	time closer to the target
	
	nodesBeforeTimeCheck = timeSinceLastTimeCheck<=0 ? 2*nodesBeforeTimeCheck :
		(int64_t) ((TIME_BETWEEN_TIME_CHECKS / timeSinceLastTimeCheck) * nodesBeforeTimeCheck);

	if (nodesBeforeTimeCheck <= MIN_NODES_BEFORE_TIME_CHECK) nodesBeforeTimeCheck = MIN_NODES_BEFORE_TIME_CHECK;
	else if (nodesBeforeTimeCheck >= MAX_NODES_BEFORE_TIME_CHECK) nodesBeforeTimeCheck = MAX_NODES_BEFORE_TIME_CHECK;
	
	timeOfLastTimeCheck = timeNow;
	nodesChecked = 0;
	
	if (timeSinceLastServerCheckin > timeBetweenServerCheckins)
		{
		//	When we check in for this task, we might be told it's redundant
		
		int sres=checkIn();
		if (sres>=2) done=TRUE;
		if (sres==3) cancelledTask=TRUE;
		};

	if (!splitMode)
		{
		if (timeSpentOnTask > timeBeforeSplit)
			{
			//	We have hit a threshold for elapsed time since we started this task, so split the task
			
			nodesToProbe0 = nodesToProbe = (int64_t) (nodesBeforeTimeCheck * maxTimeInSubtree) / (TIME_BETWEEN_TIME_CHECKS); 
			sprintf(buffer,"Splitting current task, will examine up to %"PRId64" nodes in each subtree ...",nodesToProbe);
			logString(buffer);
			splitMode=TRUE;
			};
		};
	
	//	Taper off nodesToProbe if we have been running too long

	if (timeSpentOnTask > TAPER_THRESHOLD)
		{
		nodesToProbe = (int64_t)(nodesToProbe0 * exp(-(timeSpentOnTask-TAPER_THRESHOLD)/TAPER_DECAY));
		sprintf(buffer,"Task taking too long, will only examine up to %"PRId64" nodes in each subtree ...",nodesToProbe);
		logString(buffer);
		};
	};
}

// this function recursively fills the string

void fillStr(int pos, int pfound, int partNum)
{
if (done) return;
nodesAndTime();

if (splitMode)
	{
	nodesLeft = nodesToProbe;
	if (fillStrNL(pos,pfound,partNum))
		{
		if ((subTreesCompleted++)%10==9)
			{
			printf("Completed %"PRId64" sub-trees locally so far ...\n",subTreesCompleted);
			};
		}
	else
		{
		//	When we ask to split this task, we might be told it's redundant
		
		int sres=splitTask(pos);
		if (sres>=2) done=TRUE;
		if (sres==3) cancelledTask=TRUE;
		
		if ((subTreesSplit++)%10==9)
			{
			printf("Delegated %"PRId64" sub-trees so far ...\n",subTreesSplit);
			};
		};
	return;
	};

if (pfound > bestSeenP)
	{
	bestSeenP = pfound;
	bestSeenLen = pos;
	for (int i=0;i<bestSeenLen;i++) bestSeen[i] = curstr[i];
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

int childIndex=0;
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
			max_perm = pfound+1;
			isSuper = (max_perm==fn);
			witnessCurrentString(pos+1);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);

			if (pfound+1 > bestSeenP)
				{
				bestSeenP = pfound+1;
				bestSeenLen = pos+1;
				for (int i=0;i<bestSeenLen;i++) bestSeen[i] = curstr[i];
				};

			if (max_perm+1 >= currentTask.prev_perm_ruled_out && !isSuper)
				{
				done=TRUE;
				return;
				};
			}
		else if (isSuper && pfound+1 == max_perm)
			{
			witnessCurrentString(pos+1);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);
			};

		unvisited[tperm]=FALSE;
		if (ocpTrackingOn)
			{
			int prevC=0, oc=0;
			oc=oneCycleIndices[tperm];
			prevC = oneCycleCounts[oc]--;
			oneCycleBins[prevC]--;
			oneCycleBins[prevC-1]++;
		
			curi[pos] = childIndex++;
			fillStr(pos+1, pfound+1, ndz->nextPart);
		
			oneCycleBins[prevC-1]--;
			oneCycleBins[prevC]++;
			oneCycleCounts[oc]=prevC;
			}
		else
			{
			curi[pos] = childIndex++;
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
			if	(d > 0 || (isSuper && d>=0))
				{
				curi[pos] = childIndex++;
				fillStr(pos+1, pfound, ndz->nextPart);
				}
			else
				{
				break;
				};
			};
		};
	};
	
//	If we encountered a choice that led to a repeat visit to a permutation, we follow (or prune) that branch now.
//	It will always come from the FIRST choice in the original list, as that is where any valid permutation must be.
	
if (deferredRepeat)
	{
	int d = pruneOnPerms(spareW-1, pfound - max_perm);
	if	(d>0 || (isSuper && d>=0))
		{
		curstr[pos] = nd->digit;
		curi[pos] = childIndex++;
		fillStr(pos+1, pfound, nd->nextPart);
		};
	};
}

//	Node-limited version of fillStr()

int fillStrNL(int pos, int pfound, int partNum)
{
if (done) return TRUE;
if (--nodesLeft < 0) return FALSE;
nodesAndTime();

int res = TRUE;

if (pfound > bestSeenP)
	{
	bestSeenP = pfound;
	bestSeenLen = pos;
	for (int i=0;i<bestSeenLen;i++) bestSeen[i] = curstr[i];
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

int childIndex=0;
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
			max_perm = pfound+1;
			isSuper = (max_perm==fn);
			witnessCurrentString(pos+1);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);

			if (pfound+1 > bestSeenP)
				{
				bestSeenP = pfound+1;
				bestSeenLen = pos+1;
				for (int i=0;i<bestSeenLen;i++) bestSeen[i] = curstr[i];
				};

			if (max_perm+1 >= currentTask.prev_perm_ruled_out && !isSuper)
				{
				done=TRUE;
				return TRUE;
				};
			}
		else if (isSuper && pfound+1 == max_perm)
			{
			witnessCurrentString(pos+1);
			maybeUpdateLowerBound(tperm,pos+1,tot_bl,max_perm);
			};

		unvisited[tperm]=FALSE;
		if (ocpTrackingOn)
			{
			int prevC=0, oc=0;
			oc=oneCycleIndices[tperm];
			prevC = oneCycleCounts[oc]--;
			oneCycleBins[prevC]--;
			oneCycleBins[prevC-1]++;
		
			curi[pos] = childIndex++;
			res = fillStrNL(pos+1, pfound+1, ndz->nextPart);
		
			oneCycleBins[prevC-1]--;
			oneCycleBins[prevC]++;
			oneCycleCounts[oc]=prevC;
			}
		else
			{
			curi[pos] = childIndex++;
			res = fillStrNL(pos+1, pfound+1, ndz->nextPart);
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
			if	(d > 0 || (isSuper && d>=0))
				{
				curi[pos] = childIndex++;
				res = fillStrNL(pos+1, pfound, ndz->nextPart);
				}
			else
				{
				break;
				};
			};
		};
	if (!res) return FALSE;
	};
	
//	If we encountered a choice that led to a repeat visit to a permutation, we follow (or prune) that branch now.
//	It will always come from the FIRST choice in the original list, as that is where any valid permutation must be.
	
if (deferredRepeat)
	{
	int d = pruneOnPerms(spareW-1, pfound - max_perm);
	if	(d>0 || (isSuper && d>=0))
		{
		curstr[pos] = nd->digit;
		curi[pos] = childIndex++;
		res = fillStrNL(pos+1, pfound, nd->nextPart);
		};
	};
	
return res;
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
static char buffer[BUFFER_SIZE];

//	Convert current digit string to null-terminated ASCII string

for (int k=0;k<size;k++) asciiString[k] = '0'+curstr[k];
asciiString[size] = '\0';

//	Log the new best string locally

sprintf(buffer, "Found %d permutations in string %s", max_perm, asciiString);
logString(buffer);

#if !NO_SERVER

//	Log it with the server

while (TRUE)
	{
	sprintf(buffer,"action=witnessString&n=%u&w=%u&str=%s&team=%s",n,tot_bl,asciiString,teamName);
	const char *wsRL[]={"Valid string"};
	if (sendServerCommandAndLog(buffer,wsRL,sizeof(wsRL)/sizeof(wsRL[0]))==1) break;
	
	sprintf(buffer,"Did not obtained expected response from server, will retry after %d seconds",timeBetweenServerCheckins);
	logString(buffer);
	sleepForSecs(timeBetweenServerCheckins);
	};

#endif
}

void witnessLowerBound(char *s, int size, int w, int p)
{
static char buffer[BUFFER_SIZE];

//	Convert digit string to null-terminated ASCII string

for (int k=0;k<size;k++) asciiString[k] = '0'+s[k];
asciiString[size] = '\0';

//	Log the string locally

sprintf(buffer, "Found new lower bound string for w=%d with %d permutations in string %s", w, p, asciiString);
logString(buffer);

#if !NO_SERVER

//	Log it with the server

while (TRUE)
	{
	sprintf(buffer,"action=witnessString&n=%u&w=%u&str=%s&team=%s",n,w,asciiString,teamName);
	const char *wsRL[]={"Valid string"};
	if (sendServerCommandAndLog(buffer,wsRL,sizeof(wsRL)/sizeof(wsRL[0]))==1) break;
	
	sprintf(buffer,"Did not obtained expected response from server, will retry after %d seconds",timeBetweenServerCheckins);
	logString(buffer);
	sleepForSecs(timeBetweenServerCheckins);
	};

#endif
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
size_t tlen = strlen(tsb);
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

//	Get the Instance Count of the server process

#if NO_SERVER || (!USE_SERVER_INSTANCE_COUNTS)

int getServerInstanceCount()
{
return 0;
}

#else

int getServerInstanceCount()
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

size_t ulen = strlen(URL_UTILITY);
size_t slen = strlen(IC_URL);
size_t flen = strlen(SERVER_RESPONSE_FILE_NAME);
size_t len = ulen+slen+flen+10;
char *cmd;
CHECK_MEM( cmd = malloc(len*sizeof(char)) )
sprintf(cmd,"%s \"%s\" > %s",URL_UTILITY,IC_URL,SERVER_RESPONSE_FILE_NAME);
int res = system(cmd);
free(cmd);
if (res!=0) return 1;

fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Error: Unable to read server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};
if (fscanf(fp,"%d",&res)!=1) res=1;
fclose(fp);
return res;
}

#endif

//	Send a command string via URL_UTILITY to the server at SERVER_URL, putting the response in the file SERVER_RESPONSE_FILE_NAME
//
//	Returns non-zero if an error was encountered 

#if NO_SERVER

int sendServerCommand(const char *command)
{
return 0;
}

#else

int sendServerCommand(const char *command)
{
//	Maybe get a local lock on server access

sleepUntilSiblingsFreeServer();

//	Maybe wait for server instance count to drop to zero

while (TRUE)
	{
	int ic = getServerInstanceCount();
	if (ic==0) break;
	logString("Waiting for server to be free");
	sleepForSecs(ic + rand() % VAR_SERVER_WAIT);
	};
	
//	Pre-empty the response file so it does not end up with any misleading content from a previous command if the
//	current command fails.

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"wt");
if (fp==NULL)
	{
	printf("Error: Unable to write to server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};
fclose(fp);

size_t ulen = strlen(URL_UTILITY);
size_t slen = strlen(SERVER_URL);
size_t clen = strlen(command);
size_t flen = strlen(SERVER_RESPONSE_FILE_NAME);
size_t len = ulen+slen+clen+flen+10;
char *cmd;
CHECK_MEM( cmd = malloc(len*sizeof(char)) )
sprintf(cmd,"%s \"%s%s\" > %s",URL_UTILITY,SERVER_URL,command,SERVER_RESPONSE_FILE_NAME);
int res = system(cmd);
free(cmd);

//	Check if file is still empty.  If it is, that counts as an error making contact and we need to retry.

fp = fopen(SERVER_RESPONSE_FILE_NAME,"r");
if (fp==NULL)
	{
	printf("Error: Unable to open server response file %s to read\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};
fseek(fp,0,SEEK_END);
if (ftell(fp)==0) res=-1;
fclose(fp);

//	Release local lock on server access, if any

releaseServerLock();

return res;
}

#endif

//	Read the server's last response into the log.
//	If first argument is not null, it is a string that the first line must match.
//
//	Returns:
//
//	-2 if there was an Error
//	-1 if there was a Wait request from the server
//	1 + [the index to the list of possible first line responses], if there is such a list
//	0 otherwise

#if NO_SERVER

int logServerResponse(const char **responseList, int nrl)
{
return 0;
}

#else

int logServerResponse(const char **responseList, int nrl)
{
static char buffer[BUFFER_SIZE], lbuffer[BUFFER_SIZE];
int error=FALSE, wait=FALSE, response=0;

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
	size_t blen = strlen(buffer);
	if (buffer[blen-1]=='\n')
		{
		buffer[blen-1]='\0';
		blen--;
		};
	lineNumber++;
		
	if (strncmp(buffer,"Error",5)==0) error=TRUE;
	if (strncmp(buffer,"Wait",4)==0) wait=TRUE;
	if (lineNumber==1 && responseList!=NULL)
		{
		for (int q=0;q<nrl;q++)
			{
			if (strncmp(buffer,responseList[q],strlen(responseList[q]))==0)
				{
				response=1+q;
				break;
				};
			};
		};
	
	sprintf(lbuffer,"Server: %s",buffer);
	logString(lbuffer);
	};
fclose(fp);
	
if (error) return -2;
if (wait) return -1;
return response;
}

#endif

int parseTaskParameters(const char *s, size_t slen, struct task *tsk, int taskItems, int *tif)
{
//	Look for a task parameter

for (int i=0;i<N_TASK_STRINGS;i++)
	{
	size_t len = strlen(taskStrings[i]);
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
			
			case 7:
				CHECK_MEM( tsk->branchOrder = malloc((slen-len+1)*sizeof(char)) )
				strcpy(tsk->branchOrder, s+len);
				tsk->branchOrderLen = (unsigned int)strlen(tsk->branchOrder);
				break;
			
			case 8:
				sscanf(s+len,"%u",&tsk->timeBeforeSplit);
				break;
			
			case 9:
				sscanf(s+len,"%u",&tsk->maxTimeInSubtree);
				break;
			
			case 10:
				sscanf(s+len,"%u",&tsk->timeBetweenServerCheckins);
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

#if NO_SERVER

//	Insert details of the search to run here

int getTask(struct task *tsk)
{
tsk->task_id = 0;
tsk->access_code = 0;

tsk->n_value = 6;
tsk->w_value = 105;
tsk->prefix =
"1234561234516234512634512364512346512436512463512465312465132465134265134625134652134562135";
tsk->branchOrder =
"0000000000000000000000000000000000000100000000000000000000000000020000000000000000000100001";
tsk->perm_to_exceed = 567;
tsk->prev_perm_ruled_out = 569;

tsk->prefixLen = (unsigned int)strlen(tsk->prefix);
tsk->branchOrderLen = (unsigned int)strlen(tsk->branchOrder);
setupForN(tsk->n_value);
return tsk->n_value;
}

#else

int getTask(struct task *tsk)
{
static char buffer[BUFFER_SIZE];
sprintf(buffer,"action=getTask&clientID=%u&IP=%s&programInstance=%u&team=%s",clientID,ipAddress,programInstance,teamName);
sendServerCommandAndLog(buffer,NULL,0);

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Unable to read from server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};

int quit=FALSE, taskItems=0;
static int tif[N_TASK_STRINGS];
for (int i=0;i<N_TASK_STRINGS;i++) tif[i]=FALSE;

tsk->prefixLen = 0;
tsk->branchOrderLen = 0;
tsk->timeBeforeSplit = DEFAULT_TIME_BEFORE_SPLIT;
tsk->maxTimeInSubtree = DEFAULT_MAX_TIME_IN_SUBTREE;
tsk->timeBetweenServerCheckins = DEFAULT_TIME_BETWEEN_SERVER_CHECKINS;

const char *tbsc = "timeBetweenServerCheckins: ";
size_t tbscL = strlen(tbsc);

while (!feof(fp))
	{
	//	Get a line from the server response, ensure it is null-terminated without a newline
	
	char *f=fgets(buffer,BUFFER_SIZE,fp);
	if (f==NULL) break;
	size_t blen = strlen(buffer);
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
		
	//	Always obey timeBetweenServerCheckins, even when no task provided
	
	if (strncmp(buffer,tbsc,tbscL)==0)
		{
		sscanf(buffer+tbscL,"%u",&timeBetweenServerCheckins);
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
if (tsk->branchOrderLen != tsk->prefixLen)
	{
	sprintf(buffer,
		"Error: branchOrder and prefix have different lengths (%d, %d respectively)\n",tsk->branchOrderLen,tsk->prefixLen);
	logString(buffer);
	exit(EXIT_FAILURE);
	};

if (taskItems <= N_TASK_STRINGS && taskItems >= N_TASK_STRINGS_OBLIGATORY) return tsk->n_value;
return 0;
}

#endif

int sendServerCommandAndLog(const char *s, const char **responseList, int nrl)
{
#if !NO_SERVER
static char buffer[BUFFER_SIZE];
sprintf(buffer,"To server: %s",s);
logString(buffer);

time(&timeOfLastServerCheckin);

while (TRUE)
	{
	int sleepTime = 0;
	int srep=sendServerCommand(s);
	if (srep==0)
		{
		int sr = logServerResponse(responseList, nrl);
		if (sr>=0) return sr;
		
		if (sr==-2) exit(EXIT_FAILURE);
		
		//	explicit wait request, server overwhelmed
		
		sleepTime = MIN_SERVER_WAIT + rand() % VAR_SERVER_WAIT;
		}
		
		//	No actual contact with server, so wait longer
		
	else sleepTime = timeBetweenServerCheckins;
	
	sprintf(buffer,"Unable to send command to server, will retry after %d seconds",sleepTime);
	logString(buffer);
	sleepForSecs(sleepTime);
	};
#endif
}

int getMax(int nval, int wval, int oldMax, unsigned int tid, unsigned int acc, unsigned int cid, char *ip, unsigned int pi)
{
#if NO_SERVER

return oldMax;

#else

static char buffer[BUFFER_SIZE];
sprintf(buffer,
	"action=checkMax&n=%d&w=%d&id=%u&access=%u&clientID=%u&IP=%s&programInstance=%u",
		nval, wval, tid, acc, cid, ip, pi);
sendServerCommandAndLog(buffer,NULL,0);

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
	size_t blen = strlen(buffer);
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
#endif
}

//	Create a new task to delegate a branch exploration that the current task would have performed
//	Returns 1,2,3 for OK/Done/Cancelled

int splitTask(int pos)
{
int res=0;
static char buffer[BUFFER_SIZE];

for (int i=0;i<pos;i++) asciiString[i]='0'+curstr[i];
asciiString[pos]='\0';

for (int i=0;i<pos;i++) asciiString2[i]='0'+curi[i];
asciiString2[pos]='\0';

while (TRUE)
	{
	sprintf(buffer,"action=splitTask&id=%u&access=%u&newPrefix=%s&branchOrder=%s",
		currentTask.task_id, currentTask.access_code,asciiString,asciiString2);
	const char *stRL[]={"OK","Done","Cancelled"};
	res = sendServerCommandAndLog(buffer,stRL,sizeof(stRL)/sizeof(stRL[0]));
	if (res>0) break;
	
	sprintf(buffer,"Did not obtained expected response from server, will retry after %d seconds",timeBetweenServerCheckins);
	logString(buffer);
	sleepForSecs(timeBetweenServerCheckins);
	};

return res;
}

//	Check in with the server
//	Returns 1,2,3 for OK/Done/Cancelled

int checkIn()
{
int res=0;
static char buffer[128];

while (TRUE)
	{
	sprintf(buffer,"action=checkIn&id=%u&access=%u",
		currentTask.task_id, currentTask.access_code);
	const char *chkRL[]={"OK","Done","Cancelled"};
	res = sendServerCommandAndLog(buffer,chkRL,sizeof(chkRL)/sizeof(chkRL[0]));
	if (res>0) break;
	
	sprintf(buffer,"Did not obtained expected response from server, will retry after %d seconds",timeBetweenServerCheckins);
	logString(buffer);
	sleepForSecs(timeBetweenServerCheckins);
	};

return res;
}

#if NO_SERVER

void registerClient()
{
return;
}

#else

void registerClient()
{
static char buffer[BUFFER_SIZE];

while (TRUE)
	{
	sprintf(buffer,"action=register&programInstance=%u&team=%s",programInstance,teamName);
	const char *regRL[]={"Registered"};
	int sr = sendServerCommandAndLog(buffer,regRL,sizeof(regRL)/sizeof(regRL[0]));
	if (sr==1) break;
	
	sprintf(buffer,"Will retry after %d seconds",timeBetweenServerCheckins);
	logString(buffer);
	sleepForSecs(timeBetweenServerCheckins);
	};

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"rt");
if (fp==NULL)
	{
	printf("Unable to read from server response file %s\n",SERVER_RESPONSE_FILE_NAME);
	exit(EXIT_FAILURE);
	};

int clientItems = 0;
unsigned int dummyPIN;
while (!feof(fp))
	{
	//	Get a line from the server response, ensure it is null-terminated without a newline
	
	char *f=fgets(buffer,BUFFER_SIZE,fp);
	if (f==NULL) break;
	size_t blen = strlen(buffer);
	if (buffer[blen-1]=='\n')
		{
		buffer[blen-1]='\0';
		blen--;
		};

	for (int i=0;i<N_CLIENT_STRINGS;i++)
		{
		size_t len = strlen(clientStrings[i]);
		if (strncmp(buffer,clientStrings[i],len)==0)
			{
			switch(i)
				{
				case 0:
					sscanf(buffer+len,"%u",&clientID);
					clientItems++;
					break;
				
				case 1:
					CHECK_MEM( ipAddress = malloc((blen-len+1)*sizeof(char)) )
					strcpy(ipAddress, buffer+len);
					clientItems++;
					break;
				
				case 2:
					sscanf(buffer+len,"%u",&dummyPIN);
					if (dummyPIN!=programInstance)
						{
						printf("Error: Program instance number mismatch\n");
						exit(EXIT_FAILURE);
						};
					clientItems++;
					break;
				
				default:
					break;
				};
			
			break;
			};
		};
	};
fclose(fp);

if (clientItems!=N_CLIENT_STRINGS)
	{
	printf("Error: Incomplete response from server, only saw %d of %d items\n",clientItems,N_CLIENT_STRINGS);
	exit(EXIT_FAILURE);
	};
}

#endif

#if NO_SERVER

void unregisterClient()
{
return;
}

#else

void unregisterClient()
{
char buffer[256];

sprintf(buffer,
	"action=unregister&clientID=%u&IP=%s&programInstance=%u",
		clientID, ipAddress, programInstance);
sendServerCommandAndLog(buffer,NULL,0);
}

#endif

#if NO_SERVER

void relinquishTask()
{
return;
}

#else

void relinquishTask()
{
char buffer[128];

sprintf(buffer,
	"action=relinquishTask&id=%u&access=%u&clientID=%u",
		currentTask.task_id,currentTask.access_code,clientID);
sendServerCommandAndLog(buffer,NULL,0);
}

#endif

//	Handler for SIGINT

#if UNIX_LIKE

void sigIntHandler(int a)
{
hadSigInt++;
printf("\n");
if (hadSigInt<=2)
	{
	logString("CTRL-C / SIGINT received, so program will quit after the current task.\n");
	}
else if (hadSigInt<=6)
	{
	//	Try to relinquish current task, if any
	
	logString("More than 2 CTRL-C / SIGINTs received, so program will try to relinquish the current task with the server then quit.\n");
	if (currentTask.task_id != 0) unregisterClient();
	exit(0);
	}
else
	{
	logString("More than 6 CTRL-C / SIGINTs received, so program is quitting immediately.\n");
	exit(EXIT_FAILURE);
	};
}
#endif

//	(Maybe) sleep until siblings free server

#if (UNIX_LIKE && USE_SERVER_LOCK_FILE)

void sleepUntilSiblingsFreeServer()
{
static char buffer[128];
if (useServerLock)
	{
	int serverLockIsOurs = FALSE;
	for (int i=0;i<MAX_SLEEP_WAITING_ON_SIBS;i++)
		{
		serverLockFD = open(SERVER_LOCK_FILE_NAME, O_RDWR|O_CREAT|O_EXCL, 0666);
		if (serverLockFD<0)
			{
			int sleepTime = MIN_SERVER_WAIT + rand() % VAR_SERVER_WAIT;
			sprintf(buffer,"Sibling program has lock on server, sleeping for %d seconds",sleepTime);
			logString(buffer);
			sleepForSecs(sleepTime);
			}
		else
			{
			serverLockIsOurs=TRUE;
			logString("Obtained local lock on server access");
			break;
			};
		};
		
	if (!serverLockIsOurs)
		{
		sprintf(buffer,"Unable to lock file %s after %d attempts, so giving up on locking",
			SERVER_LOCK_FILE_NAME,MAX_SLEEP_WAITING_ON_SIBS);
		logString(buffer);
		useServerLock=FALSE;
		};
	};
}

void releaseServerLock()
{
if (useServerLock && serverLockFD >=0)
	{
	close(serverLockFD);
	unlink(SERVER_LOCK_FILE_NAME);
	logString("Released local lock on server access");
	};
}

#else

void sleepUntilSiblingsFreeServer()
{
}

void releaseServerLock()
{
}

#endif

//	Try to get a new lower bound for weight w+1 by appending digits.
//
//	See how many permutations we get by following a single weight-2 edge, and then as many weight-1 edges
//	as possible before we hit a permutation already visited.

void maybeUpdateLowerBoundAppend(int tperm, int size, int w, int p)
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
	mperm_res[w+1] = m;
	
	curstr[size] = curstr[size-(n-1)];
	curstr[size+1] = curstr[size-n];
	for (int j=0;j<nu-1;j++) curstr[size+2+j] = curstr[size-(n-2)+j];

	for (int k=0;k<size+nu+1;k++) klbStrings[w+1][k]=curstr[k];
	klbLen[w+1] = size+nu+1;
	witnessLowerBound(klbStrings[w+1], klbLen[w+1], w+1, m);
	
	maybeUpdateLowerBoundAppend(okT, klbLen[w+1], w+1, m);
	};

for (int i=0;i<nu;i++) unvisited[unv[i]]=TRUE;
unvisited[tperm]=TRUE;
}

//	Try to get a new lower bound for weight w+1 by splicing in digits.
//
//	This particular startegy will only work for n=6 and strings that contain a weight-3 edge.
//
//	We follow a weight-2 edge instead of the weight-3 edge, then follow four weight-1 edges, then
//	a weight-3 edge will again take us back to the permutation we would have reached without the detour.
//	If all five permutations in question were unvisited, we will have added them at the cost of an increase in
//	weight of 1. 

void maybeUpdateLowerBoundSplice(int size, int w, int p)
{
if (n!=6 || mperm_res[w+1] >= p+5) return;	//	Nothing to gain

char *wasted;
int *perms;
CHECK_MEM( wasted = (char *)malloc(size*sizeof(char)) )
CHECK_MEM( perms = (int *)malloc(size*sizeof(int)) )

//	Identify wasted characters in the current string

int tperm0=0;
for (int j0=0;j0<size;j0++)
	{
	int d = curstr[j0];
	tperm0 = (tperm0>>DBITS) | (d << nmbits);
	perms[j0]=tperm0;
	wasted[j0] = !valid[tperm0];
	};
	
//	Look for weight-3 edges.  Skip the initial n-1 characters, which will always be marked as wasted.

for (int j0=n;j0+3<size;j0++)
	{
	if (!wasted[j0] && wasted[j0+1] && wasted[j0+2] && !wasted[j0+3])
		{
		//	We are at a weight-3 edge
		
		int t0 = perms[j0];
		int t = successor2[t0];
		int nu=0, okT=0;
		while (unvisited[t] && nu<5)
			{
			nu++;
			okT = t;			//	Record the last unvisited permutation integer
			t=successor1[t];
			};
		if (nu==5)
			{
			//	We found 5 unvisited permutations we can visit this way
			
			mperm_res[w+1]=p+5;
			int sz=j0+1;
			char *ks=klbStrings[w+1];
			for (int k=0;k<sz;k++) ks[k]=curstr[k];
			ks[sz] = ks[sz-(n-1)];
			ks[sz+1] = ks[sz-n];
			for (int j=0;j<nu-1;j++) ks[sz+2+j] = ks[sz-(n-2)+j];
			int q=sz+nu+1;
			for (int k=j0+1;k<size;k++) ks[q++] = curstr[k];
			klbLen[w+1] = q;
			witnessLowerBound(klbStrings[w+1], klbLen[w+1], w+1, p+5);
			break;
			};
		};
	};
	
free(wasted);
free(perms);
}

void maybeUpdateLowerBound(int tperm, int size, int w, int p)
{
maybeUpdateLowerBoundSplice(size,w,p);
maybeUpdateLowerBoundAppend(tperm,size,w,p);
}
