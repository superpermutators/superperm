//
//  FastDCMTest.c
//
//  Author:		Greg Egan
//	Version:	6.1
//	Date:		17 July 2019
//
//	This is a TEST version of the DistributedChaffinMethod client that uses
//	OpenCL to run a parallel version of the search on a GPU.

//	=============================
//	Test version or full version?
//	=============================

#define TEST_VERSION TRUE

//	Headers

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
#include <errno.h>

#include "Headers/NInfo.h"
#include "Headers/Structures.h"
#include "Headers/GPU.h"

#ifdef _WIN32

//	Compiling for Windows

#include <windows.h>
#include <process.h>
#include <io.h>
#include <CL/opencl.h>

#define UNIX_LIKE FALSE

#else

//	Compiling for MacOS or Linux

#include <unistd.h>
#include <signal.h>

#ifdef __APPLE__

//	For MacOS

#define CL_SILENCE_DEPRECATION 1
#include <OpenCL/opencl.h>

#else

//	For Linux etc.

#include <CL/opencl.h>

#endif

#define UNIX_LIKE TRUE

#endif

#ifndef TRUE
#define TRUE (1==1)
#define FALSE (1==0)
#endif

#define Kb 1024
#define Mb (1024*Kb)

#define VERBOSE FALSE

//	Macros
//	------

#define MFREE(p) if ((p)!=NULL) free(p); 
#define CHECK_MEM(p) if ((p)==NULL) {printf("Insufficient CPU memory\n"); exit(EXIT_FAILURE);};

//	Constants
//	---------

//	Server URL

#define SERVER_URL "http://supermutations.net/ChaffinMethod.php?version=13&"

//	Set NO_SERVER to TRUE to run a debugging version that makes no contact with the server, and just
//	performs a search with parameters that need to be inserted into the source code (in getTask()) manually.

#define NO_SERVER FALSE 

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
#define QUIT_FILE_NAME_TEMPLATE "QUIT_%u.txt"
#define QUIT_FILE_ALL "QUIT_ALL.txt"

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

#define DEFAULT_TIME_BEFORE_SPLIT (120*MINUTE)

//	Default maximum time to spend exploring a subtree when splitting, multiplied by number of threads

#define DEFAULT_MAX_TIME_IN_SUBTREE (2*MINUTE)

//	Actual time limit on simultaneous subtree exploration

#define SUBTREE_TIME_CEILING (10*MINUTE)

//	When the time spent on a task exceeds this threshold, we start exponentially reducing the number of nodes
//	we explore in each subtree, with an (1/e)-life given

#define TAPER_THRESHOLD (180*MINUTE)

#define TAPER_DECAY (5*MINUTE)

//	Size of general-purpose buffer for messages from server, log, etc.

#define BUFFER_SIZE (32*1024)

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
unsigned char *curstr=NULL;			//	Current string as integer digits
unsigned char *curi=NULL;
unsigned char *bestSeen=NULL;		//	Longest string seen in search, as integer digits
int bestSeenLen, bestSeenP;
int *successor1;			//	For each permutation, its weight-1 successor
int *successor2;			//	For each permutation, its weight-2 successor
int *klbLen;				//	For each number of wasted characters, the lengths of the strings that visit known-lower-bound permutations
char **klbStrings;			//	For each number of wasted characters, a list of all strings that visit known-lower-bound permutations
char *asciiString=NULL;		//	String as ASCII digits
char *asciiString2=NULL;		//	String as ASCII digits
int max_perm;				//	Maximum number of permutations visited by any string seen so far
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

int isSuper=FALSE;			//	Set TRUE when we have found a superpermutation
int done=FALSE;				//	Set TRUE when task becomes redundant
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

time_t startedRunning;				//	Time program started running
time_t startedCurrentTask=0;		//	Time we started current task
time_t timeOfLastTimeCheck;			//	Time we last checked the time
time_t timeOfLastTimeReport;		//	Time we last reported elapsed time to the user
time_t timeOfLastServerCheckin;		//	Time we last contacted the server

int timeBetweenServerCheckins = DEFAULT_TIME_BETWEEN_SERVER_CHECKINS;
int timeBeforeSplit = DEFAULT_TIME_BEFORE_SPLIT;
int maxTimeInSubtree = DEFAULT_MAX_TIME_IN_SUBTREE;
int timeInSubtree = DEFAULT_MAX_TIME_IN_SUBTREE;

int serverPressure = 0;				//	Set greater than 0 if server is facing heavy traffic

cl_ulong totalNodesSearched=0;
int subTreesDelegated=0;
int subTreesLocal=0;

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
static char QUIT_FILE_NAME[FILE_NAME_SIZE];

static char *sqFiles[]={STOP_FILE_NAME,STOP_FILE_ALL,QUIT_FILE_NAME,QUIT_FILE_ALL};

//	Time quota, in minutes
//	The default is for the "timeLimit" / "timeLimitHard "option with no argument;
//	default behaviour is to keep running indefinitely

#define DEFAULT_TIME_LIMIT 120
int timeQuotaMins=0, timeQuotaHardMins=0, timeQuotaEitherMins=0;

//	When "longRunner" option is chosen, task never decides it is taking too long and shrinks sub-tree depth

int longRunner=FALSE;

//  Default team name
#define DEFAULT_TEAM_NAME "anonymous"
#define MAX_TEAM_NAME_LENGTH 32
char *teamName;

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
void doTask(void);
int checkIn(void);
int splitTask(int pos);
int fac(int k);
void makePerms(int n, int **permTab);
void witnessCurrentString(int size);
void witnessLowerBound(char *s, int size, int w, int p);
void maybeUpdateLowerBound(int tperm, int size, int w, int p);
void maybeUpdateLowerBoundSplice(int size, int w, int p);
void maybeUpdateLowerBoundAppend(int tperm, int size, int w, int p);
int compareDS(const void *ii0, const void *jj0);
void rClassMin(int *p, int n);
void sigIntHandler(int a);
void timeCheck(time_t timeNow);
void cleanupOpenCL(int host);
int setFlagsFromDigits(const unsigned char *digits, int digitsLen, int offs);

//	Table of strings corresponding to each permutation number

char pstrings[FN+NVAL][NVAL+1];

//	Details of GPU

#define MAX_CL_INFO 2048

const char *gpuName = NULL, *gpuPlatform = NULL;
int gpuDeviceNumber = -1, chosenDeviceNumber = -1;
static char chosenGPU[MAX_CL_INFO];
cl_platform_id gpu_platform;
cl_device_id gpu_device;
cl_ulong gpu_gms=0;				//	Global memory size
cl_ulong gpu_mma=0;				//	Maximum memory allocation
cl_ulong gpu_lms=0;				//	Local memory size
cl_ulong gpu_cbs=0;				//	Constant buffer size
cl_uint gpu_cu=0;				//	Number of compute units
size_t gpu_mws=0;				//	Maximum workgroup size

//	Header files that we need to prepend to the kernels we build

const char *headerFiles[] = {"Headers/NInfo.h","Headers/Structures.h","Headers/GPU.h"};
int nHeaders;
size_t *headerLengths=NULL;
char **headerSourceCode=NULL;

//	The kernels themselves

#define INDEX_KERNEL 0
#define DELEGATE_KERNEL 1
#define COLLECT_KERNEL 2

#define SEARCH_KERNEL 3
#define SPLIT_KERNEL 4

#define OCP_KERNEL_OFFSET 2
#define SUPER_KERNEL_OFFSET 4
#define NODES_KERNEL_OFFSET 8
#define TEST_KERNEL_OFFSET 24

#define OCP0 "#define OCP 0\n"
#define OCP1 "#define OCP 1\n"
#define SUP0 "#define SUPER 0\n"
#define SUP1 "#define SUPER 1\n"
#define NDS0 "#define NODES_EXIT 0\n"
#define NDS1 "#define NODES_EXIT 1\n"
#define NDS2 "#define NODES_EXIT 2\n"

const char *kernelFiles[][3] = {
	{"Kernels/orderIndexKernel.txt", "orderIndex", ""},
	{"Kernels/delegateKernel.txt", "delegate", ""},
	{"Kernels/collectSuperKernel.txt", "collectSuper", ""},

	{"Kernels/searchKernel.txt", "search",	OCP0 SUP0 NDS0},
	{"Kernels/splitKernel.txt", "split",	OCP0 SUP0 NDS0},

	{"Kernels/searchKernel.txt", "search",	OCP1 SUP0 NDS0},
	{"Kernels/splitKernel.txt", "split",	OCP1 SUP0 NDS0},

	{"Kernels/searchKernel.txt", "search",	OCP0 SUP1 NDS0},
	{"Kernels/splitKernel.txt", "split",	OCP0 SUP1 NDS0},

	{"Kernels/searchKernel.txt", "search",	OCP1 SUP1 NDS0},
	{"Kernels/splitKernel.txt", "split",	OCP1 SUP1 NDS0},

	{"Kernels/searchKernel.txt", "search",	OCP0 SUP0 NDS1},
	{"Kernels/splitKernel.txt", "split",	OCP0 SUP0 NDS1},

	{"Kernels/searchKernel.txt", "search",	OCP1 SUP0 NDS1},
	{"Kernels/splitKernel.txt", "split",	OCP1 SUP0 NDS1},

	{"Kernels/searchKernel.txt", "search",	OCP0 SUP1 NDS1},
	{"Kernels/splitKernel.txt", "split",	OCP0 SUP1 NDS1},

	{"Kernels/searchKernel.txt", "search",	OCP1 SUP1 NDS1},
	{"Kernels/splitKernel.txt", "split",	OCP1 SUP1 NDS1},

	{"Kernels/searchKernel.txt", "search",	OCP0 SUP0 NDS2},
	{"Kernels/splitKernel.txt", "split",	OCP0 SUP0 NDS2},

	{"Kernels/searchKernel.txt", "search",	OCP1 SUP0 NDS2},
	{"Kernels/splitKernel.txt", "split",	OCP1 SUP0 NDS2},

	{"Kernels/searchKernel.txt", "search",	OCP0 SUP1 NDS2},
	{"Kernels/splitKernel.txt", "split",	OCP0 SUP1 NDS2},

	{"Kernels/searchKernel.txt", "search",	OCP1 SUP1 NDS2},
	{"Kernels/splitKernel.txt", "split",	OCP1 SUP1 NDS2},
	
	{"Kernels/searchKernel.txt", "search",	OCP0 SUP0 NDS0 "#define KERNEL_TEST 1\n"}
};

int nKernels=0;
size_t *kernelLengths=NULL;
char **kernelSourceCode=NULL, **kernelFullSource=NULL;
cl_program *kernelPrograms=NULL;
cl_kernel *kernels=NULL;
size_t *kernelWGS=NULL, *kernelPWM=NULL, min_kernel_ws;
cl_ulong *kernelPMS=NULL, *kernelLMS=NULL;
int log_min_kernel_ws;

//	Information about N-digit strings

cl_uchar *perms=NULL;

//	Table of maximum permutations for each number of wasted digits

size_t mpermSizeBytes;
cl_ushort *mperm_res0=NULL, *mperm_res;

//	OpenCL structures

cl_context context=NULL;
cl_command_queue commands=NULL;

//	Thread numbers and heap sizes

int max_global_ws = 0, max_groups = 0, max_local_ws = 0;
int nStringsPerHeap = 0;

//	Host memory

#define NUM_HOST_HEAPS 2

struct string *host_heaps[NUM_HOST_HEAPS];
cl_uint *host_blockSum = NULL;
cl_uint *host_inputIndices = NULL;
struct maxPermsLoc *host_maxPerms = NULL;
cl_ulong *host_nodesSearched = NULL;

//	GPU memory

#define NUM_GPU_HEAPS 7
#define NUM_GPU_PS 2
#define NUM_GPU_INDICES 1

cl_mem gpu_perms=NULL, gpu_mperm_res0=NULL;
cl_mem gpu_heaps[NUM_GPU_HEAPS];
cl_mem gpu_inputIndices[NUM_GPU_INDICES];
cl_mem gpu_nodesSearched=NULL;
cl_mem gpu_prefixSum[NUM_GPU_PS];
cl_mem gpu_blockSum[NUM_GPU_PS];
cl_mem gpu_maxPerms=NULL;

//	Quota for steps taken by each thread

#define DEFAULT_STEP_QUOTA 1000

cl_ulong stepQuota = DEFAULT_STEP_QUOTA;

//	Known values from previous calculations

int known3[][2]={{0,3},{1,6}};
int known4[][2]={{0,4},{1,8},{2,12},{3,14},{4,18},{5,20},{6,24}};
int known5[][2]={{0,5},{1,10},{2,15},{3,20},{4,23},{5,28},{6,33},{7,36},{8,41},{9,46},{10,49},{11,53},{12,58},{13,62},{14,66},{15,70},{16,74},{17,79},{18,83},{19,87},{20,92},{21,96},{22,99},{23,103},{24,107},{25,111},{26,114},{27,116},{28,118},{29,120}};
int known6[][2]={{0,6},{1,12},{2,18},{3,24},{4,30},{5,34},{6,40},{7,46},{8,52},{9,56},{10,62},{11,68},{12,74},{13,78},{14,84},{15,90},{16,94},{17,100},{18,106},{19,112},{20,116},{21,122},{22,128},{23,134},{24,138},{25,144},{26,150},{27,154},{28,160},{29,166},{30,172},{31,176},{32,182},{33,188},{34,192},{35,198},{36,203},{37,209},{38,214},{39,220},{40,225},{41,230},{42,236},{43,241},{44,246},{45,252},{46,257},{47,262},{48,268},{49,274},{50,279},{51,284},{52,289},{53,295},{54,300},{55,306},{56,311},{57,316},{58,322},{59,327},{60,332},{61,338},{62,344},{63,349},{64,354},{65,360},{66,364},{67,370},{68,375},{69,380},{70,386},{71,391},{72,396},{73,402},{74,407},{75,412},{76,418},{77,423},{78,429},{79,434},{80,439},{81,445},{82,450},{83,455},{84,461},{85,465},{86,470},{87,476},{88,481},{89,486},{90,492},{91,497},{92,502},{93,507},{94,512},{95,518},{96,523},{97,528},{98,534},{99,539},{100,543},{101,548},{102,552},{103,558},{104,564},{105,568},{106,572},{107,578},{108,583},{109,589},{110,594},{111,599},{112,604},{113,608},{114,613},{115,618},{116,621},{117,625}};

#define USE_KNOWN0(n) int *knownN = &known ## n[0][0]; int numKnownW=sizeof(known## n)/(sizeof(int))/2;

#define USE_KNOWN(n) USE_KNOWN0(n)

#if NVAL>=3 && NVAL<=6
	USE_KNOWN(NVAL)
#else
	int *knownN=NULL;
	int numKnownW=0;
#endif

//	Best string encountered in current search

struct string bestString;

//	Read a text file into memory
//	Convert any carriage returns into linefeeds

char *readTextFile(const char *fileName, size_t *textSize)
{
char *res = NULL;

FILE *fp = fopen(fileName,"r");
if (fp==NULL)
	{
	printf("Unable to open the file %s to read (%s)\n",fileName,strerror(errno));
	exit(EXIT_FAILURE);
	};

fseek(fp,0,SEEK_END);
size_t fileSize = ftell(fp);
fseek(fp,0,SEEK_SET);

CHECK_MEM( res = (char *)malloc(fileSize+1) )

if (fread(res, fileSize, 1, fp) != 1)
	{
	printf("Unable to read from the file %s (%s)\n",fileName,strerror(errno));
	exit(EXIT_FAILURE);
	};
fclose(fp);

res[fileSize] = '\0';
for (size_t i=0;i<fileSize;i++) if (res[i]=='\r') res[i]='\n';

*textSize = fileSize;
return res;
}

//	Deal with an OpenCL return code;
//	return on success, or cleanup and exit program on error.

void openCL(cl_int rc)
{
if (rc == CL_SUCCESS) return;

const char *msg = NULL;

switch (rc)
{
case CL_INVALID_VALUE:
	msg = "CL_INVALID_VALUE";
	break;
case CL_OUT_OF_HOST_MEMORY:
	msg = "CL_OUT_OF_HOST_MEMORY";
	break;
case CL_OUT_OF_RESOURCES:
	msg = "CL_OUT_OF_RESOURCES";
	break;
case CL_INVALID_PLATFORM:
	msg = "CL_INVALID_PLATFORM";
	break;
case CL_INVALID_PROPERTY:
	msg = "CL_INVALID_PROPERTY";
	break;
case CL_INVALID_DEVICE_TYPE:
	msg = "CL_INVALID_DEVICE_TYPE";
	break;
case CL_INVALID_DEVICE:
	msg = "CL_INVALID_DEVICE";
	break;
case CL_DEVICE_NOT_AVAILABLE:
	msg = "CL_DEVICE_NOT_AVAILABLE";
	break;
case CL_DEVICE_NOT_FOUND:
	msg = "CL_DEVICE_NOT_FOUND";
	break;
case CL_INVALID_CONTEXT:
	msg = "CL_INVALID_CONTEXT";
	break;
case CL_INVALID_PROGRAM:
	msg = "CL_INVALID_PROGRAM";
	break;
case CL_INVALID_PROGRAM_EXECUTABLE:
	msg = "CL_INVALID_PROGRAM_EXECUTABLE";
	break;
case CL_INVALID_KERNEL_NAME:
	msg = "CL_INVALID_KERNEL_NAME";
	break;
case CL_INVALID_KERNEL_DEFINITION:
	msg = "CL_INVALID_KERNEL_DEFINITION";
	break;
case CL_INVALID_BINARY:
	msg = "CL_INVALID_BINARY";
	break;
case CL_INVALID_BUILD_OPTIONS:
	msg = "CL_INVALID_BUILD_OPTIONS";
	break;
case CL_INVALID_OPERATION:
	msg = "CL_INVALID_OPERATION";
	break;
case CL_COMPILER_NOT_AVAILABLE:
	msg = "CL_COMPILER_NOT_AVAILABLE";
	break;
case CL_BUILD_PROGRAM_FAILURE:
	msg = "CL_BUILD_PROGRAM_FAILURE";
	break;
case CL_INVALID_COMMAND_QUEUE:
	msg = "CL_INVALID_COMMAND_QUEUE";
	break;
case CL_INVALID_KERNEL:
	msg = "CL_INVALID_KERNEL";
	break;
case CL_INVALID_KERNEL_ARGS:
	msg = "CL_INVALID_KERNEL_ARGS";
	break;
case CL_INVALID_WORK_DIMENSION:
	msg = "CL_INVALID_WORK_DIMENSION";
	break;
case CL_INVALID_GLOBAL_WORK_SIZE:
	msg = "CL_INVALID_GLOBAL_WORK_SIZE";
	break;
case CL_INVALID_GLOBAL_OFFSET:
	msg = "CL_INVALID_GLOBAL_OFFSET";
	break;
case CL_INVALID_WORK_GROUP_SIZE:
	msg = "CL_INVALID_WORK_GROUP_SIZE";
	break;
case CL_INVALID_WORK_ITEM_SIZE:
	msg = "CL_INVALID_WORK_ITEM_SIZE";
	break;
case CL_MISALIGNED_SUB_BUFFER_OFFSET:
	msg = "CL_MISALIGNED_SUB_BUFFER_OFFSET";
	break;
case CL_INVALID_IMAGE_SIZE:
	msg = "CL_INVALID_IMAGE_SIZE";
	break;
case CL_IMAGE_FORMAT_NOT_SUPPORTED:
	msg = "CL_IMAGE_FORMAT_NOT_SUPPORTED";
	break;
case CL_MEM_OBJECT_ALLOCATION_FAILURE:
	msg = "CL_MEM_OBJECT_ALLOCATION_FAILURE";
	break;
case CL_INVALID_EVENT_WAIT_LIST:
	msg = "CL_INVALID_EVENT_WAIT_LIST";
	break;
default:
	msg = "Unknown error";
};

printf("OpenCL returned the error: %s\n",msg);
cleanupOpenCL(TRUE);
exit(EXIT_FAILURE);
}

//	Count the number of distinct permutations in a digit string (integer digits 0 ...N-1, not ASCII)

int countPerms(unsigned char *s, int ns)
{
unsigned char pf[FN+1];
for (int k=0;k<=FN;k++) pf[k]=0;

unsigned short lastN = 0;
int pc=0;
for (int k=0;k<ns;k++)
	{
	unsigned char d=s[k];
	lastN = (lastN/NVAL) + NN1*d;
	unsigned char P = perms[lastN];
	unsigned short pNum = (P>=FNM) ? FN : (d*FNM+P);
	
	//	Count any new permutation we visit
	
	if (P<FNM && (!pf[pNum])) pc++;
	
	//	Set the flag, either for permutation or "no permutation"
	
	pf[pNum]=TRUE;
	};
return pc;
}

//	Expand a "string" structure into a complete digit string (integer digits 0...N-1 if offset is 0; integer digits 1...N if offset is 1)
//
//	Each string structure is related to a prefix; the string itself only contains the last N digits of the prefix
//
//	Return the total length of the expanded string

int expandString(struct string *str, unsigned char *s, char *prefix, size_t prefixLen, int offset)
{
int fsc=0;
for (int k=0;k<prefixLen;k++) s[fsc++] = prefix[k]-'1'+offset;
for (int k=NVAL;k<str->pos;k++) s[fsc++]=(str->digits[k] & DIGIT_BITS)+offset;
return fsc;
}

void computeBranchOrder(unsigned char *digit, int size, unsigned char *br)
{
br[0]=0;
for (int k=1;k<size;k++) br[k]=((digit[k]-1)+2*NVAL-1-(digit[k-1]-1))%NVAL;
}

//	Print string data

void print_string_data(FILE *fp, struct string *nd, char *prefix, size_t prefixLen, int verbose, int minimal, int check)
{
static char lastN1D[NVAL];
static unsigned char fullString[2*FN];
int ok=TRUE;

if (verbose) fprintf(fp,"String root depth: %d\n",nd->rootDepth);

lastN1D[NVAL-1]=0;
if (!minimal) fprintf(fp,"waste = %u\n",nd->waste);
if (!minimal) fprintf(fp,"perms = %u\n",nd->perms);
unsigned short int s0=nd->lastN1;
for (int k=0;k<NVAL-1;k++)
	{
	unsigned short d = s0 % NVAL;
	lastN1D[k] = '1'+d;
	s0 = s0 / NVAL;
	};
if (verbose) fprintf(fp,"lastN1 = %s\n",lastN1D);
if (verbose) fprintf(fp,"pFlags: ");
int fcount=0;
for (int pNum=0;pNum<=FN;pNum++)
	{
	if ((nd->digits[pNum] & PERMUTATION_BITS)!=0)
		{
		if (verbose) fprintf(fp,"%s ",pstrings[pNum]);
		fcount++;
		};
	};
if (verbose) fprintf(fp,"\n");

if (!minimal) fprintf(fp,"Full string: ");
int fsc=0;
for (int k=0;k<prefixLen;k++)
	{
	fprintf(fp,"%c",prefix[k]);
	fullString[fsc++] = prefix[k]-'1';
	};

for (int k=NVAL;k<nd->pos;k++)
	{
	char d = nd->digits[k] & DIGIT_BITS;
	fprintf(fp,"%c",'1'+d);
	fullString[fsc++]=d;
	};
fprintf(fp,"\n");

if (!minimal) fprintf(fp,"Next digit = %c\n",'1'+(nd->digits[nd->pos] & DIGIT_BITS));

if (check)
	{
	int cp = countPerms(fullString,fsc);
	if (cp != nd->perms)
		{
		printf("String/perms mismatch in data: counted %d distinct permutations in string, but perms = %u\n",cp,nd->perms);
		ok=FALSE;
		};
		
	int waste = fsc - (NVAL-1) - cp;
	if (waste != nd->waste)
		{
		printf("String/perms mismatch in data: counted %d wasted digits in string, but waste = %u\n",waste,nd->waste);
		ok=FALSE;
		};
		
	if (fcount-1 != nd->perms)
		{
		printf("Flags/perms mismatch in data: counted %d permutation flags, but perms = %u\n",fcount-1,nd->perms);
		ok=FALSE;
		};

	if (!ok) exit(EXIT_FAILURE);
	};
}

int initOpenCL(const char *gpuName, const char *gpuPlatform, int gpuDeviceNumber, int verbose, int host)
{
for (int i=0;i<NUM_GPU_HEAPS;i++) gpu_heaps[i]=NULL;

//	Tally all the memory we end up allocating on the GPU

uint64_t gpuAlloc=0;

//	Tally the size in bytes of all the data we want to store in the GPU's constant buffer memory,
//	so we can determine if the GPU we find is suitable.

uint64_t cbNeeded = 0;

//	Construct a table which identifies whether strings of N digits from 0 to (N-1) are
//	permutations.
//
//	NB: Unlike "DistributedChaffinMethod.c", we work internally with digits 0 ... (N-1)
//
//	When we encode the last N digits of the string as an integer,
//	the first is the least significant, the most recent is the
//	most significant.
//
//	So we encode the last N digits as:
//
//	d_0 + N*d_1 + (N^2)*d_2 + ... + (N^(N-1))*d_{N-1}
//
//	Each entry P in the array of unsigned bytes, perms[], indexed by the integer
//	encoding of the last N digits of the string, has a value between 0 and (N-1)!+N-1
//	inclusive (e.g. 0-125 for N=6, 0-28 for N=5).
//
//	* If P is less than (N-1)!, then the permutation number
//	for the digits is d*(N-1)! + P, where d is the last digit of the string.
//
//  * If P >= (N-1)!, then the digits are not a permutation, and P-(N-1)! is the least
//	number of digits that must be added to the string before it can end in a permutation.

//	Compute N^N, the number of digit strings

int nn = 1;
for (int k=0;k<NVAL;k++) nn*=NVAL;

//	Storage for the permutation info for each string

size_t permsSizeBytes = nn*sizeof(cl_uchar);
if (host)
	{
	CHECK_MEM( perms = (cl_uchar *) malloc(permsSizeBytes) )
	};
cbNeeded += permsSizeBytes;

//	Compute the permutation info

static unsigned char digits[NVAL], dcount[NVAL], pcount[NVAL];
for (int k=0;k<NVAL;k++) pcount[k]=0;

for (int s=0;s<nn;s++)
	{
	int s0=s;
	for (int k=0;k<NVAL;k++) dcount[k]=0;
	int isPerm=TRUE;
	for (int k=0;k<NVAL;k++)
		{
		int d = digits[k] = s0 % NVAL;
		s0 = s0 / NVAL;
		if (dcount[d]++ != 0) isPerm=FALSE;
		};
	
	if (isPerm)
		{
		//	We have a permutation.
		
		//	The number in perms[] needs to range from 0 to (N-1)-1!, and identify the 1-cyle
		//	the permutation belongs to, so we reuse the same number for a full 1-cycle, and
		//	only store it, for all the members of the 1-cycle, when when we have a permutation
		//	ending in 0.
		
		if (digits[NVAL-1]==0)
			{
			int pc = pcount[digits[NVAL-1]];
			for (int y=0;y<NVAL;y++)
				{
				int s1 = 0;
				for (int z=NVAL-1;z>=0;z--) s1=NVAL*s1+digits[(z+y)%NVAL];
				perms[s1] = pc;

				//	Create string version of permutation
				
				unsigned short pNum = digits[(NVAL-1+y)%NVAL]*FNM+perms[s1];
				for (int k=0;k<NVAL;k++) pstrings[pNum][k]='1'+digits[(k+y)%NVAL];
				pstrings[pNum][NVAL]='\0';
				
				pcount[y]++;
				};
			};
		}
	else
		{
		//	Not a permutation, so find longest run of repetition-free digits at the end
		
		int repFree=TRUE;
		for (int l=2;l<=NVAL;l++)
			{
			for (int i=0;i<l && repFree;i++)
			for (int j=i+1;j<l;j++)
				{
				if (digits[NVAL-1-i]==digits[NVAL-1-j])
					{
					repFree=FALSE;
					break;
					};
				};

			//	We've hit the first length, l, with a repetition, so l-1 is longest without repetition
			
			if (!repFree)
				{
				//	N-(longest no-rep length) is number of digits we need to add before we could get a permutation
				
				perms[s] = FNM+NVAL-(l-1);
				break;
				};
			};
			
		if (repFree)
			{
			printf("Failed to identify longest repetition-free run in digits of s=%d\n",s);
			exit(EXIT_FAILURE);
			};
		};
	};
	
strcpy(&pstrings[FN][0],"NPERM");
	
//	Check that we ended up with sensible results

for (int k=0;k<NVAL;k++)
	{
	if (pcount[k]!=FNM)
		{
		printf("pcount[%d]=%d, expected %d\n",k,pcount[k],FNM);
		exit(EXIT_FAILURE);
		};
	};
	
//	Table of maximum permutations for each number of wasted digits
	
mpermSizeBytes = (MAX_WASTE_VALS+MPERM_OFFSET)*sizeof(cl_ushort);
if (host)
	{
	CHECK_MEM( mperm_res0 = (cl_ushort *)malloc(mpermSizeBytes) )
	};
cbNeeded += mpermSizeBytes;

for (int k=0;k<MPERM_OFFSET;k++) mperm_res0[k]=0;
mperm_res = mperm_res0+MPERM_OFFSET;
for (int k=0;k<MAX_WASTE_VALS;k++) mperm_res[k]=NVAL;
if (knownN)
	for (int k=0;k<numKnownW;k++)
		mperm_res[k] = knownN[2*k+1];

//	See what OpenCL platforms and GPUs are available

#define MAX_CL_PLATFORMS 10
#define MAX_CL_DEVICES 10

int foundPlatformAndGPU=FALSE;
cl_platform_id platforms[MAX_CL_PLATFORMS];
cl_device_id devices[MAX_CL_PLATFORMS][MAX_CL_DEVICES];
cl_uint num_platforms, num_devices[MAX_CL_PLATFORMS];

openCL( clGetPlatformIDs(MAX_CL_PLATFORMS,
                        platforms,
                        &num_platforms) );
                        
static char openCL_info[MAX_CL_INFO];

if (verbose) printf("Found %u OpenCL platform%s on this system:\n", num_platforms, num_platforms==1?"":"s");
if (num_platforms == 0) exit(EXIT_FAILURE);

for (int i=0;i<num_platforms;i++)
	{
	int gpuPlatformOK = TRUE;
	
	if (verbose) printf("Platform %d:\n",i+1);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, MAX_CL_INFO, openCL_info, NULL) );
	if (verbose) printf("Profile: %s\n",openCL_info);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, MAX_CL_INFO, openCL_info, NULL) );
	if (verbose) printf("Version: %s\n",openCL_info);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, MAX_CL_INFO, openCL_info, NULL) );
	if (verbose) printf("Name: %s\n",openCL_info);
	
	if (gpuPlatform)
		{
		gpuPlatformOK = (strncmp(gpuPlatform,openCL_info,strlen(gpuPlatform))==0);
		};
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, MAX_CL_INFO, openCL_info, NULL) );
	
	if (gpuPlatform)
		{
		if (strncmp(gpuPlatform,openCL_info,strlen(gpuPlatform))==0) gpuPlatformOK = TRUE;
		};

	if (verbose) printf("Vendor: %s\n",openCL_info);
	
	if (!gpuPlatformOK)
		{
		printf("[This platform's name or vendor do not start with the user-specified \"%s\", so it has been ruled out]\n",gpuPlatform);
		continue;
		};
	
	//	Get the devices available on this platform
	
	cl_int cRet = clGetDeviceIDs(platforms[i],
                      CL_DEVICE_TYPE_GPU,
                      MAX_CL_DEVICES,
                      devices[i],
                      &num_devices[i]);
                      
	if (cRet != CL_SUCCESS)	num_devices[i]=0;
                      
	if (verbose) printf("Found %u GPU device%s for this platform:\n",num_devices[i],num_devices[i]==1?"":"s");
	
	for (int j=0; j<num_devices[i]; j++)
		{
		if (verbose) printf("\tDevice %d:\n",j+1);
		
		int gpuOK = TRUE, gpuDeviceNameOK = TRUE, gpuDeviceNumberOK = TRUE;
		
		//	Details of GPU

		cl_ulong gms;				//	Global memory size
		cl_ulong mma;				//	Maximum memory allocation
		cl_ulong lms;				//	Local memory size
		cl_ulong cbs;				//	Constant buffer size
		cl_uint cu;					//	Number of compute units
		size_t mws;					//	Maximum workgroup size
		cl_bool ca, la;

		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_NAME, MAX_CL_INFO, openCL_info, NULL) );
		if (verbose) printf("\tDevice name: %s\n",openCL_info);

		if (gpuName)
			{
			gpuDeviceNameOK = (strncmp(gpuName,openCL_info,strlen(gpuName))==0);
			};
			
		if (gpuDeviceNumber > 0)
			{
			gpuDeviceNumberOK = (j+1)==gpuDeviceNumber;
			};

		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_VENDOR, MAX_CL_INFO, openCL_info, NULL) );
		if (verbose) printf("\tDevice vendor: %s\n",openCL_info);
		
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_COMPILER_AVAILABLE, sizeof(ca), &ca, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_LINKER_AVAILABLE, sizeof(la), &la, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(gms), &gms, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(mma), &mma, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(lms), &lms, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cbs), &cbs, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cu), &cu, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(mws), &mws, NULL) );
		if (verbose) printf("\tCompiler available: %s\n", ca == CL_TRUE ? "Yes" : "No");
		if (verbose) printf("\tLinker available: %s\n", ca == CL_TRUE ? "Yes" : "No");
		if (verbose) printf("\tGlobal memory size = %u Mb\n", (unsigned int)(gms/Mb));
		if (verbose) printf("\tMaximum memory allocation = %u Mb\n", (unsigned int)(mma/Mb));
		if (verbose) printf("\tLocal memory size = %u Kb\n", (unsigned int)(lms/Kb));
		if (verbose) printf("\tConstant buffer size = %u Kb\n", (unsigned int)(cbs/Kb));
		if (verbose) printf("\tCompute units = %u\n", (unsigned int)cu);
		if (verbose) printf("\tMaximum workgroup size = %u\n", (unsigned int)mws);
		
		if (!gpuDeviceNameOK)
			{
			if (verbose) printf("\t[This device name does not start with the user-specified \"%s\", so it has been ruled out]\n",gpuName);
			gpuOK = FALSE;
			};
		
		if (!gpuDeviceNumberOK)
			{
			if (verbose) printf("\t[This device number in the list is not the user-specified %d, so it has been ruled out]\n",gpuDeviceNumber);
			gpuOK = FALSE;
			};
		
		if (ca != CL_TRUE && la != CL_TRUE)
			{
			if (verbose) printf("\t[This device does not have a compiler/linker available]\n");
			gpuOK = FALSE;
			};
			
		if (cbs < cbNeeded)
			{
			if (verbose) printf("\t[The program needs %"PRIu64" bytes of constant memory buffer (%u Kb), but the GPU only permits %u Kb]\n",
				cbNeeded, (unsigned int)cbNeeded/Kb, (unsigned int)cbs/Kb);
			gpuOK = FALSE;
			};
			
		if (gpuOK)
			{
			if (verbose) printf("\t[This GPU meets all the requirements to run the program]\n");
			foundPlatformAndGPU = TRUE;
			
			//	If we had multiple GPUs we could use, pick the "best" one
			
			if (mws*cu > gpu_mws*gpu_cu)
				{
				gpu_platform = platforms[i];
				gpu_device = devices[i][j];
				gpu_gms = gms;
				gpu_mma = mma;
				gpu_lms = lms;
				gpu_cbs = cbs;
				gpu_cu = cu;
				gpu_mws = mws;
				openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_NAME, MAX_CL_INFO, chosenGPU, NULL) );
				chosenDeviceNumber = j+1;
				}
			else
				{
				if (verbose) printf("\t[But a previously listed GPU would run at least as many threads, so it will be used instead.]\n");
				};
			};
		};
	};
if (verbose) printf("\n");

if (!foundPlatformAndGPU)
	{
	printf("No suitable OpenCL platform / GPU device was found\n");
	exit(EXIT_FAILURE);
	};
	
if (verbose) printf("Using GPU: %s [device number %d in list]\n",chosenGPU,chosenDeviceNumber);

//	Get an OpenCL context

cl_int clErr;
cl_context_properties cprop[] = {CL_CONTEXT_PLATFORM, (cl_context_properties) gpu_platform, 0};
context = clCreateContext(cprop, 1, &gpu_device, NULL, NULL, &clErr);
openCL(clErr);
if (verbose) printf("Created openCL context\n");

commands = clCreateCommandQueue(context, gpu_device, 0, &clErr);
openCL(clErr);
if (verbose) printf("Created openCL command queue\n\n");

//	Read in / create headers and prepend them to kernel source code

size_t headerLengthTotal = 0;
nHeaders = sizeof(headerFiles) / sizeof(headerFiles[0]);
CHECK_MEM( headerLengths = (size_t *)malloc(nHeaders * sizeof(size_t)) )
CHECK_MEM( headerSourceCode = (char **)calloc(nHeaders, sizeof(char *)) )
for (int i=0;i<nHeaders;i++)
	{
	headerSourceCode[i] = readTextFile(headerFiles[i],headerLengths+i);
	headerLengthTotal += headerLengths[i];
	};
	
nKernels = sizeof(kernelFiles) / (sizeof(kernelFiles[0]));
CHECK_MEM( kernelLengths = (size_t *)malloc(nKernels * sizeof(size_t)) )
CHECK_MEM( kernelSourceCode = (char **)calloc(nKernels, sizeof(char *)) )
CHECK_MEM( kernelFullSource = (char **)calloc(nKernels, sizeof(char *)) )
for (int i=0;i<nKernels;i++)
	{
	kernelSourceCode[i] = readTextFile(kernelFiles[i][0],kernelLengths+i);
	
	size_t klen = strlen(kernelFiles[i][2]+1) + headerLengthTotal + kernelLengths[i];
	
	CHECK_MEM( kernelFullSource[i] = (char *)malloc((klen+1) * sizeof(char)) )
	
	size_t ptr = 0;
	
	strcpy(kernelFullSource[i]+ptr, kernelFiles[i][2]);
	ptr += strlen(kernelFiles[i][2]);
	*(kernelFullSource[i]+(ptr++)) = '\n';
	
	for (int j=0;j<nHeaders;j++)
		{
		strcpy(kernelFullSource[i]+ptr, headerSourceCode[j]);
		ptr += headerLengths[j];
		};
	strcpy(kernelFullSource[i]+ptr, kernelSourceCode[i]);
	};
	
if (verbose) printf("\n");
	
//	Create kernel programs from source code

CHECK_MEM ( kernelPrograms = (cl_program *)calloc(nKernels, sizeof(cl_program)) )
CHECK_MEM ( kernels = (cl_kernel *)calloc(nKernels, sizeof(cl_kernel)) )
CHECK_MEM ( kernelWGS = (size_t *)calloc(nKernels, sizeof(size_t)) )
CHECK_MEM ( kernelLMS = (cl_ulong *)calloc(nKernels, sizeof(size_t)) )
CHECK_MEM ( kernelPMS = (cl_ulong *)calloc(nKernels, sizeof(size_t)) )
CHECK_MEM ( kernelPWM = (size_t *)calloc(nKernels, sizeof(size_t)) )

min_kernel_ws = gpu_mws;
for (int i=0;i<nKernels;i++)
	{
	if (verbose) printf("Building kernel \"%s()\" (kernel %d of %d) ...\n",kernelFiles[i][1],i+1,nKernels);
	kernelPrograms[i] = clCreateProgramWithSource(context, 1, (const char **) kernelFullSource+i, NULL, &clErr);
	openCL(clErr);
	if (verbose) printf("  Compiling ... ");
	
	clErr = clBuildProgram(kernelPrograms[i], 1, &gpu_device, "-Werror", NULL, NULL);
	
	if (clErr != CL_SUCCESS)
		{
		//	If kernel failed to build, get the log
		
		openCL( clGetProgramBuildInfo(kernelPrograms[i], gpu_device, CL_PROGRAM_BUILD_LOG, MAX_CL_INFO, openCL_info, NULL) );
		printf("%s\n",openCL_info);
		openCL(clErr);
		};
	
	kernels[i] = clCreateKernel(kernelPrograms[i], kernelFiles[i][1], &clErr);
	openCL(clErr);
	
	openCL( clGetKernelWorkGroupInfo(kernels[i], gpu_device,CL_KERNEL_WORK_GROUP_SIZE, sizeof(kernelWGS[i]), &kernelWGS[i],NULL) );
	openCL( clGetKernelWorkGroupInfo(kernels[i], gpu_device,CL_KERNEL_LOCAL_MEM_SIZE, sizeof(kernelLMS[i]), &kernelLMS[i],NULL) );
	openCL( clGetKernelWorkGroupInfo(kernels[i], gpu_device,CL_KERNEL_PRIVATE_MEM_SIZE, sizeof(kernelPMS[i]), &kernelPMS[i],NULL) );
	openCL( clGetKernelWorkGroupInfo(kernels[i], gpu_device,CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(kernelPWM[i]), &kernelPWM[i],NULL) );
	
	if (verbose) printf("Successfully compiled kernel \"%s()\", work group size=%lu, local memory size=%llu, private memory size=%llu, preferred work group size multiple=%lu\n",
		kernelFiles[i][1],kernelWGS[i],kernelLMS[i],kernelPMS[i],kernelPWM[i]);
	
	if (kernelWGS[i] < min_kernel_ws) min_kernel_ws = kernelWGS[i];
	};
if (verbose) printf("\n");

log_min_kernel_ws = 0;
while ((1<<log_min_kernel_ws) < min_kernel_ws) log_min_kernel_ws++;
if ((1<<log_min_kernel_ws) > min_kernel_ws) log_min_kernel_ws--;
min_kernel_ws = (1<<log_min_kernel_ws);

//	Allocate memory and load data

if (verbose) printf("Allocating memory for constant tables in GPU ...\n");
gpu_perms = clCreateBuffer(context, CL_MEM_READ_ONLY, permsSizeBytes, NULL, &clErr);
openCL(clErr);
gpu_mperm_res0 = clCreateBuffer(context, CL_MEM_READ_ONLY, mpermSizeBytes, NULL, &clErr);
openCL(clErr);

openCL( clEnqueueWriteBuffer(commands, gpu_perms, CL_TRUE, 0, permsSizeBytes, perms, 0, NULL, NULL) );

openCL( clEnqueueWriteBuffer(commands, gpu_mperm_res0, CL_TRUE, 0, mpermSizeBytes, mperm_res0, 0, NULL, NULL) );


if (verbose) printf("  Constant tables successfully loaded into GPU\n\n");

//	Determine how many threads to run.
//
//	We want to use the highest power of 2 less than the full number of compute units;
//	we also need to be able to allocate sufficient global memory to hold NUM_GPU_HEAPS strings for each
//	thread.

max_local_ws = (int)min_kernel_ws;

max_groups = 1;
while (TRUE)
	{
	size_t mem = max_groups * NUM_GPU_HEAPS * sizeof(struct string) * max_local_ws;
	if (2*mem > gpu_mma || 2*max_groups >= gpu_cu) break;
	max_groups*=2;
	};
	
max_global_ws = max_groups * max_local_ws;

if (verbose) printf("Aim to run %d x %d = %d threads\n\n",max_groups,max_local_ws,max_global_ws);

nStringsPerHeap = max_global_ws;

//	Host memory

if (host)
	{
	for (int i=0;i<NUM_HOST_HEAPS;i++)
		{
		CHECK_MEM( host_heaps[i] = (struct string *)malloc(nStringsPerHeap*sizeof(struct string)) )
		};

	CHECK_MEM( host_blockSum = (cl_uint *)malloc(max_groups*sizeof(cl_uint)) )

	CHECK_MEM( host_inputIndices = (cl_uint *)malloc(nStringsPerHeap*sizeof(cl_uint)) )
	for (int k=0;k<nStringsPerHeap;k++) host_inputIndices[k] = k;

	CHECK_MEM( host_maxPerms = (struct maxPermsLoc *)malloc(max_groups*sizeof(struct maxPermsLoc)) )

	CHECK_MEM( host_nodesSearched = (cl_ulong *)malloc(max_groups*sizeof(cl_ulong)) )
	};

if (verbose) printf("Trying to allocate memory in the GPU ...\n");

//	GPU memory

for (int i=0;i<NUM_GPU_HEAPS;i++)
	{
	gpu_heaps[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, nStringsPerHeap*sizeof(struct string), NULL, &clErr);
	openCL(clErr);
	gpuAlloc += nStringsPerHeap*sizeof(struct string);
	};

for (int i=0;i<NUM_GPU_INDICES;i++)
	{
	gpu_inputIndices[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, nStringsPerHeap*sizeof(cl_uint), NULL, &clErr);
	openCL(clErr);
	gpuAlloc += nStringsPerHeap*sizeof(cl_uint);
	};

gpu_nodesSearched = clCreateBuffer(context, CL_MEM_READ_WRITE, max_groups*sizeof(cl_ulong), NULL, &clErr);
openCL(clErr);
gpuAlloc += max_groups*sizeof(cl_ulong);

for (int i=0;i<NUM_GPU_PS;i++)
	{
	gpu_prefixSum[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, nStringsPerHeap*sizeof(struct stringStatus), NULL, &clErr);
	openCL(clErr);
	gpuAlloc += nStringsPerHeap*sizeof(struct stringStatus);

	gpu_blockSum[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, max_groups*sizeof(cl_uint), NULL, &clErr);
	openCL(clErr);
	gpuAlloc += max_groups*sizeof(cl_uint);
	};

gpu_maxPerms = clCreateBuffer(context, CL_MEM_READ_WRITE, max_groups*sizeof(struct maxPermsLoc), NULL, &clErr);
openCL(clErr);
gpuAlloc += max_groups*sizeof(struct maxPermsLoc);

if (verbose) printf("Global memory allocated on GPU: %"PRIu64" Mb\n",(gpuAlloc+Mb-1)/Mb);
if (verbose) printf("\n");

return TRUE;
}

void cleanupOpenCL(int host)
{
if (host)
	{
	//	Free host memory

	for (int i=0;i<NUM_HOST_HEAPS;i++) if (host_heaps[i]) free(host_heaps[i]);
	if (host_blockSum) free(host_blockSum);
	if (host_inputIndices) free(host_inputIndices);
	if (host_maxPerms) free(host_maxPerms);
	if (host_nodesSearched) free(host_nodesSearched);
	if (perms) free(perms);
	if (mperm_res0) free(mperm_res0);
	};

//	Free OpenCL resources

for (int i=0;i<NUM_GPU_PS;i++)
	{
	if (gpu_prefixSum[i]) clReleaseMemObject(gpu_prefixSum[i]);
	if (gpu_blockSum[i]) clReleaseMemObject(gpu_blockSum[i]);
	};
	
if (gpu_maxPerms) clReleaseMemObject(gpu_maxPerms);
if (gpu_nodesSearched) clReleaseMemObject(gpu_nodesSearched);
for (int i=0;i<NUM_GPU_INDICES;i++) if (gpu_inputIndices[i]) clReleaseMemObject(gpu_inputIndices[i]);
for (int i=0;i<NUM_GPU_HEAPS;i++) if (gpu_heaps[i]) clReleaseMemObject(gpu_heaps[i]);
if (gpu_mperm_res0) clReleaseMemObject(gpu_mperm_res0);
if (gpu_perms) clReleaseMemObject(gpu_perms);

for (int i=0;i<nKernels;i++)
	{
	if (kernelPrograms && kernelPrograms[i]) clReleaseProgram(kernelPrograms[i]);
	if (kernels && kernels[i]) clReleaseKernel(kernels[i]);
	
	if (kernelSourceCode && kernelSourceCode[i]) free(kernelSourceCode[i]); 
	if (kernelFullSource && kernelFullSource[i]) free(kernelFullSource[i]); 
	};
	
for (int i=0;i<nHeaders;i++)
	{
	if (headerSourceCode && headerSourceCode[i]) free(headerSourceCode[i]); 
	};

if (headerLengths) free(headerLengths);
if (headerSourceCode) free(headerSourceCode);
if (kernelLengths) free(kernelLengths);
if (kernelSourceCode) free(kernelSourceCode);
if (kernelFullSource) free(kernelFullSource);

if (kernelPrograms) free(kernelPrograms);
if (kernels) free(kernels);
if (kernelWGS) free(kernelWGS);
if (kernelLMS) free(kernelLMS);
if (kernelPMS) free(kernelPMS);
if (kernelPWM) free(kernelPWM);

if (commands) clReleaseCommandQueue(commands);
if (context) clReleaseContext(context);
}

void checkIndices()
{
int ok = TRUE;
cl_uint *hi = (cl_uint *)malloc(nStringsPerHeap * sizeof(cl_uint));
int *flags = (int *)malloc(nStringsPerHeap * sizeof(int));

openCL( clEnqueueReadBuffer(commands, gpu_inputIndices[0], CL_TRUE,
			0, nStringsPerHeap*sizeof(cl_uint), hi, 0, NULL, NULL) );
			
for (int k=0;k<nStringsPerHeap;k++) flags[k]=0;
for (int k=0;k<nStringsPerHeap;k++)
	{
	cl_uint f = hi[k];
	if (f >= nStringsPerHeap)
		{
		printf("gpu_inputIndices[0] has entry at %d of %u, which is too high\n",k,f);
		ok=FALSE;
		};
	flags[f]++;
	};
for (int k=0;k<nStringsPerHeap;k++)
	{
	if (flags[k]!=1)
		{
		printf("The number %d appears %d times in gpu_inputIndices[0]\n",k,flags[k]);
		ok=FALSE;
		};
	};
free(flags);
free(hi);

if (ok) printf("gpu_inputIndices[0] checked, all OK\n");
}

void writeFromGPUtoDisk(cl_mem gpu_buffer, size_t n, char *name)
{
char *buffer = (char *)malloc(n);
openCL( clEnqueueReadBuffer(commands, gpu_buffer, CL_TRUE,
			0, n, buffer, 0, NULL, NULL) );
FILE *fp=fopen(name,"w");
if (fp==NULL)
	{
	printf("Unable to open file %s to write\n",name);
	exit(EXIT_FAILURE);
	};
if (fwrite(buffer,n,1,fp)!=1)
	{
	printf("Error writing to file %s\n",name);
	exit(EXIT_FAILURE);
	};
fclose(fp);
free(buffer);
}

void readToGPUFromDisk(cl_mem gpu_buffer, size_t n, char *name)
{
char *buffer = (char *)malloc(n);
FILE *fp=fopen(name,"r");
if (fp==NULL)
	{
	printf("Unable to open file %s to read\n",name);
	exit(EXIT_FAILURE);
	};
if (fread(buffer,n,1,fp)!=1)
	{
	printf("Error reading from file %s\n",name);
	exit(EXIT_FAILURE);
	};
fclose(fp);
openCL( clEnqueueWriteBuffer(commands, gpu_buffer, CL_TRUE,
			0, n, buffer, 0, NULL, NULL) );
free(buffer);
}

//	Do a search on the GPU starting from a specified number of strings, for a fixed total waste, and an initial pte.

int searchString(struct string *br, int nStart, cl_int totalWaste, cl_uint pte, cl_uint pro, cl_uint *maxPermsSeen, cl_ulong *totalNodesSearched, int verbose,
	char *prefix, int prefixLen, int liveSearch, int timeLimitSecs, int copyOnTimeOut, int timeStopSplitting)
{
int result = 0;
time_t startSearchTime;
time(&startSearchTime);

cl_uint nTrueInputs = nStart;

size_t local_ws;
int nWorkGroups;
size_t global_ws;

//	Initialise data in the heap with a single string

int inputHeap = 0;

openCL( clEnqueueWriteBuffer(commands, gpu_heaps[inputHeap], CL_TRUE,
			0, nTrueInputs*sizeof(struct string), br, 0, NULL, NULL) );


//	Initialise indices as the sequence 0,1,2,...

for (int i=0;i<NUM_GPU_INDICES;i++)
	{
	openCL( clEnqueueWriteBuffer(commands, gpu_inputIndices[i], CL_TRUE,
			0, nStringsPerHeap*sizeof(cl_uint), host_inputIndices, 0, NULL, NULL) );
	
	};

//	Initialise perms-to-exceed

cl_uint pte0 = pte;

double nsp = 0;
int threads=0, runs=0;
time_t t1, t2;
int totalRuns=0;

time(&t1);
while (TRUE)		//	Loop for repeated runs of stepQuota
	{
	totalRuns++;
	int timeOut=FALSE, doSplit=TRUE;
	
	//	Set the workgroup sizes

	global_ws = 1;
	while (global_ws < nTrueInputs) global_ws*=2;
	if (global_ws <= max_local_ws)
		{
		local_ws = global_ws;
		nWorkGroups = 1;
		}
	else
		{
		local_ws = max_local_ws;
		nWorkGroups = (int)(global_ws/local_ws);
		};

	if (liveSearch)
		{
		time_t timeNow;
		time(&timeNow);
		double timeSinceLastTimeCheck = difftime(timeNow, timeOfLastTimeCheck);
		if (timeSinceLastTimeCheck >= 1)
			{
			timeOfLastTimeCheck = timeNow;
			double timeSpentOnSearch = difftime(timeNow, startSearchTime);
			if (timeSpentOnSearch > timeLimitSecs)
				{
				timeOut=TRUE;
				}
			else
				{
				
				if (timeSpentOnSearch > timeStopSplitting) doSplit=FALSE;
				
				timeCheck(timeNow);
				if (done || cancelledTask)
					{
					result=0;
					break;
					};
				};
			};
		};

	int super = pte0+2 >= FN;
	int ocp = totalWaste >= FNM;
	if (pte0+1 >= pro)
		{
		if (verbose) printf("  We can't exceed %d permutations so we're done\n",pte0);
		result=0;
		break;				//	Can't exceed pte if next highest count has been ruled out
		};
	
	cl_uint nLocalThreads = (cl_uint) local_ws;
	cl_uint paddedBlockSize = nLocalThreads + CONFLICT_FREE_OFFSET(nLocalThreads-1);
	
	if (verbose) printf("  Running %d x %d = %d threads for %d true inputs, each taking up to %"PRIu64" steps each\n",
		nWorkGroups, (int)local_ws, (int)global_ws, nTrueInputs, stepQuota);
	
	int srchKI = SEARCH_KERNEL;
	int splitKI = SPLIT_KERNEL;
	
	if (ocp)
		{
		srchKI += OCP_KERNEL_OFFSET;
		splitKI += OCP_KERNEL_OFFSET;
		};
	if (super)
		{
		srchKI += SUPER_KERNEL_OFFSET;
		splitKI += SUPER_KERNEL_OFFSET;
		};
	if (timeOut)
		{
		srchKI += NODES_KERNEL_OFFSET;
		splitKI += NODES_KERNEL_OFFSET;
		};
		
	if (TEST_VERSION && totalWaste==1 && (!ocp) && (!super) && (!timeOut)) srchKI += TEST_KERNEL_OFFSET;

	cl_kernel sk = kernels[srchKI];

/*
__kernel void search(
	__constant unsigned char *perms,			//	0: Table of permutation info for N-digit strings
	__constant unsigned short *mperm_res0,		//	1: Table of maximum number of permutations for each waste
	int totalWaste,								//	2: Total waste we are allowing in each string		
	unsigned int pte,							//	3: Permutations to exceed
	unsigned int maxPermsSeen,					//	4: Maximum permutations seen in any string (not just this search)
	unsigned long stepQuota,					//	5: Maximum number of steps to take before we quit
	__global struct string *inputs,				//	6: String each thread starts from
	__global unsigned int *inputIndices,		//	7: Indices used to select strings from inputs[]
	__global struct string *outputs,			//	8: Final state of each thread's string
	__global struct string *bestStrings,		//	9: Best string found by each thread
	__local unsigned long *nsLocal,				//	10: Local buffer used for nodes searched
	__global unsigned long *nsGlobal,			//	11: Number of nodes searched by each thread
	__local struct stringStatus *psLocal,		//	12: Local buffer used for prefix sums
	__global struct stringStatus *psGlobal,		//	13: Cumulative sum of 0|1 for finished|unfinished searches
	__global unsigned int *blockSum,			//	14: Block totals for prefix sums
	__local struct maxPermsLoc *mpLocal,		//	15: Local buffer used to determine best string across workgroup
	__global struct maxPermsLoc *mpGlobal,		//	16: Info about best string in each block
	unsigned int nLocalThreads,					//	17: Number of local threads in each workgroup
	unsigned int nTrueInputs					//	18: Number of true inputs; threads will be padded out to a power of 2
#if SUPER
	,
	__global struct string *superperms,			//	19: Any superpermutations found
	__local struct stringStatus *SpsLocal,		//	20: Local buffer used for prefix sums
	__global struct stringStatus *SpsGlobal,	//	21: Cumulative sum of 0|1 for finished|unfinished searches
	__global unsigned int *SblockSum			//	22: Block totals for prefix sums
#endif
	)
*/

	int outputHeap = 1-inputHeap;
/*	
	if (totalRuns==1198369)
		{
		writeFromGPUtoDisk(gpu_heaps[inputHeap], nStringsPerHeap*sizeof(struct string), "inputHeap.dat");
		writeFromGPUtoDisk(gpu_inputIndices[0], nStringsPerHeap*sizeof(cl_uint), "inputIndices.dat");
		printf("Wrote GPU buffers to disk, nLocalThreads=%d, nTrueInputs=%d, inputHeap=%d, totalWaste=%d, pte0=%d, *maxPermsSeen=%d\n",
			nLocalThreads,nTrueInputs,inputHeap,totalWaste,pte0,*maxPermsSeen);
		};
*/
	openCL( clSetKernelArg(sk, 0, sizeof(gpu_perms), &gpu_perms) );
	openCL( clSetKernelArg(sk, 1, sizeof(gpu_mperm_res0), &gpu_mperm_res0) );
	openCL( clSetKernelArg(sk, 2, sizeof(totalWaste), &totalWaste) );
	openCL( clSetKernelArg(sk, 3, sizeof(pte0), &pte0) );
	openCL( clSetKernelArg(sk, 4, sizeof(*maxPermsSeen), maxPermsSeen) );
	openCL( clSetKernelArg(sk, 5, sizeof(stepQuota), &stepQuota) );
	openCL( clSetKernelArg(sk, 6, sizeof(gpu_heaps[inputHeap]), &gpu_heaps[inputHeap]) );
	openCL( clSetKernelArg(sk, 7, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
	openCL( clSetKernelArg(sk, 8, sizeof(gpu_heaps[outputHeap]), &gpu_heaps[outputHeap]) );
	openCL( clSetKernelArg(sk, 9, sizeof(gpu_heaps[2]), &gpu_heaps[2]) );
	openCL( clSetKernelArg(sk, 10, sizeof(cl_ulong)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 11, sizeof(gpu_nodesSearched), &gpu_nodesSearched) );
	openCL( clSetKernelArg(sk, 12, sizeof(struct stringStatus)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 13, sizeof(gpu_prefixSum[0]), &gpu_prefixSum[0]) );
	openCL( clSetKernelArg(sk, 14, sizeof(gpu_blockSum[0]), &gpu_blockSum[0]) );
	openCL( clSetKernelArg(sk, 15, sizeof(struct maxPermsLoc)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 16, sizeof(gpu_maxPerms), &gpu_maxPerms) );
	openCL( clSetKernelArg(sk, 17, sizeof(nLocalThreads), &nLocalThreads) );
	openCL( clSetKernelArg(sk, 18, sizeof(nTrueInputs), &nTrueInputs) );
	
	if (super)
		{
		openCL( clSetKernelArg(sk, 19, sizeof(gpu_heaps[3]), &gpu_heaps[3]) );
		openCL( clSetKernelArg(sk, 20, sizeof(struct stringStatus)*paddedBlockSize, NULL) );
		openCL( clSetKernelArg(sk, 21, sizeof(gpu_prefixSum[1]), &gpu_prefixSum[1]) );
		openCL( clSetKernelArg(sk, 22, sizeof(gpu_blockSum[1]), &gpu_blockSum[1]) );
		};

	openCL( clEnqueueNDRangeKernel(commands, sk, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
	openCL( clFinish(commands) );
	
	
	threads += global_ws;
	
	//	Extract results of the search from GPU
	
	cl_ulong nc=0;
	cl_uint totalUnfinished=0;
	unsigned int mp=0, mpw=0;
	
	//	Counts of nodes searched (for each block / local workgroup)

	openCL( clEnqueueReadBuffer(commands, gpu_nodesSearched, CL_TRUE,
		0, sizeof(cl_ulong)*nWorkGroups, host_nodesSearched, 0, NULL, NULL) );
	
	
	//	Details of maximum permutations achieved (for each block / local workgroup)
		
	openCL( clEnqueueReadBuffer(commands, gpu_maxPerms, CL_TRUE,
		0, sizeof(struct maxPermsLoc)*nWorkGroups, host_maxPerms, 0, NULL, NULL) );
	
	
	//	Number of unfinished searches (for each block / local workgroup)

	openCL( clEnqueueReadBuffer(commands, gpu_blockSum[0], CL_TRUE,
		0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );
	

	for (int z=0;z<nWorkGroups;z++)
		{
		//	Compute total of nodes searched, in this run
		nc += host_nodesSearched[z];
		
		//	Identify GPU-wide max permutations achieved, in this run
		
		if (host_maxPerms[z].perms > mp)
			{
			mp = host_maxPerms[z].perms;
			mpw = host_maxPerms[z].where;
			};
			
		//	Convert block sums into cumulative sums, and compute overall total

		cl_uint t = host_blockSum[z];
		host_blockSum[z]=totalUnfinished;
		totalUnfinished += t;
		};
	
	//	Write the cumulative sums of the block sums back to GPU, where they'll be needed by orderIndex

	openCL( clEnqueueWriteBuffer(commands, gpu_blockSum[0], CL_TRUE,
		0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );
	

	if (verbose) printf("  Unfinished strings remaining = %u\n",totalUnfinished);

	*totalNodesSearched += nc;
	if (verbose) printf("  Nodes searched in this run = %"PRIu64"\n",nc);
		
	if (mp > *maxPermsSeen)
		{
		*maxPermsSeen=mp;
		if (verbose) printf("  Max permutations seen = %d\n",*maxPermsSeen);
		
		//	Read the new best string from GPU
		
		openCL( clEnqueueReadBuffer(commands, gpu_heaps[2], CL_TRUE,
			mpw*sizeof(struct string), sizeof(struct string), &bestString, 0, NULL, NULL) );
		
		
		//	Need to adjust length to include final digit
		
		bestString.pos++;
		
		//	Validate string's waste and permutation counts
		
		static unsigned char fs[2*FN];
		int fsc = expandString(&bestString, fs, prefix, prefixLen,0);
		int pc = countPerms(fs, fsc);
		if (pc != bestString.perms)
			{
			printf("Mismatch between claimed permutations bestString.perms=%d and actual count, %d\n",bestString.perms,pc);
			print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, TRUE, FALSE);
			exit(EXIT_FAILURE);
			};
		int w = fsc - (NVAL-1) - pc;
		if (w != bestString.waste)
			{
			printf("Mismatch between claimed waste bestString.waste=%d and actual waste, %d\n",bestString.waste,w);
			print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, TRUE, FALSE);
			exit(EXIT_FAILURE);
			};
		if (verbose) print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, FALSE, FALSE);
		
		if (liveSearch)
			{
			bestSeenLen = expandString(&bestString, bestSeen, prefix, prefixLen, 1);
			bestSeenP = mp;
			if (mp > max_perm)
				{
				max_perm = mp;
				if (mp > pte0)
					{
					int pos = expandString(&bestString, curstr, prefix, prefixLen, 1);
					witnessCurrentString(pos);
					int tperm = setFlagsFromDigits(curstr,pos,0);
					maybeUpdateLowerBound(tperm,pos,tot_bl,max_perm);
					};
				};
			};
			
		if (*maxPermsSeen > pte0)
			{
			pte0 = *maxPermsSeen;
			if (pte0 == FN) pte0 = FN-1;
			};
		};
		
	//	Number of superpermutations found (for each block / local workgroup)

	if (super)
		{
		openCL( clEnqueueReadBuffer(commands, gpu_blockSum[1], CL_TRUE,
			0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );
		

		cl_uint superCount = 0;
		for (int z=0;z<nWorkGroups;z++)
			{
			//	Convert block sums into cumulative sums, and compute overall total

			cl_uint t = host_blockSum[z];
			host_blockSum[z]=superCount;
			superCount += t;
			};
		
		//	Write the cumulative sums of the block sums back to GPU, where they'll be needed by orderIndex

		openCL( clEnqueueWriteBuffer(commands, gpu_blockSum[1], CL_TRUE,
			0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );
		
		
		if (superCount != 0)
			{
			printf("Found %d superpermutations\n",superCount);
			
			/*
	__kernel void collectSuper(
	__global struct stringStatus *SpsGlobal,		//	Cumulative sum of 0|1 for superpermutations (summed only within each block)
	__global unsigned int *SblockSum,			//	Block totals for prefix sums
	__global struct string *superIn,
	__global struct string *superOut,
	unsigned int nLocalThreads					//	Number of threads in each local workgroup
	)
			*/
			
			cl_kernel ck = kernels[COLLECT_KERNEL];
			
			openCL( clSetKernelArg(ck, 0, sizeof(gpu_prefixSum[1]), &gpu_prefixSum[1]) );
			openCL( clSetKernelArg(ck, 1, sizeof(gpu_blockSum[1]), &gpu_blockSum[1]) );
			openCL( clSetKernelArg(ck, 2, sizeof(gpu_heaps[3]), &gpu_heaps[3]) );
			openCL( clSetKernelArg(ck, 3, sizeof(gpu_heaps[4]), &gpu_heaps[4]) );
			openCL( clSetKernelArg(ck, 4, sizeof(nLocalThreads), &nLocalThreads) );

			openCL( clEnqueueNDRangeKernel(commands, ck, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
			openCL( clFinish(commands) );
			
				
			openCL( clEnqueueReadBuffer(commands, gpu_heaps[4], CL_TRUE,
				0, superCount*sizeof(struct string), host_heaps[0], 0, NULL, NULL) );
			
			
			for (int z=0;z<superCount;z++)
				{
				host_heaps[0][z].pos++;
				print_string_data(stdout, &host_heaps[0][z], prefix, prefixLen, FALSE, TRUE, FALSE);
				
				if (liveSearch)
					{
					int pos = expandString(&host_heaps[0][z], curstr, prefix, prefixLen, 1);
					witnessCurrentString(pos);
					if (z!=superCount-1) sleepForSecs(1);
					};
				};
			};
		};
	
	//	Quit if we have no unfinished searches
	
	if (totalUnfinished==0)
		{
		result=0;
		break;
		};
	
	//	Arrange for the first totalUnfinished indices in gpu_inputIndices[0] to point
	//	to the strings with unfinished searches, and the rest (up to global_ws) to point
	//	to empty splots in the heap.

	cl_kernel ik = kernels[INDEX_KERNEL];

/*
__kernel void orderIndex(
	__global struct stringStatus *psGlobal,		//	Cumulative sum of 0|1 for finished|unfinished searches
	__global unsigned int *blockSum,			//	Block totals for prefix sums
	__global unsigned int *inputIndices,		//	Indices used to select strings from inputs[]
	unsigned int nLocalThreads,
	unsigned int totalUnfinished
	)
*/

	openCL( clSetKernelArg(ik, 0, sizeof(gpu_prefixSum[0]), &gpu_prefixSum[0]) );
	openCL( clSetKernelArg(ik, 1, sizeof(gpu_blockSum[0]), &gpu_blockSum[0]) );
	openCL( clSetKernelArg(ik, 2, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
	openCL( clSetKernelArg(ik, 3, sizeof(nLocalThreads), &nLocalThreads) );
	openCL( clSetKernelArg(ik, 4, sizeof(totalUnfinished), &totalUnfinished) );

	openCL( clEnqueueNDRangeKernel(commands, ik, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
	openCL( clFinish(commands) );
	
	
	//	If we need to split the task, move all the unfinished searches into a separate buffer
	
	if (timeOut)
		{
		if (copyOnTimeOut >= 0)
			{
	/*
		__kernel void delegate(
		__global unsigned int *inputIndices,		//	Indices used to select strings from inputs[]
		__global struct string *inputs,				//	Final state of each thread's string
		__global struct string *outputs,			//	Final state of each thread's string
		unsigned int totalUnfinished				//	Number of true inputs; threads will be padded out to a power of 2
		)
	*/	

			cl_kernel dk = kernels[DELEGATE_KERNEL];

			global_ws = 1;
			while (TRUE)
				{
				if (global_ws >= totalUnfinished) break;
				global_ws*=2;
				};

			if (global_ws <= max_local_ws) local_ws = global_ws;
			else local_ws = max_local_ws;

			openCL( clSetKernelArg(dk, 0, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
			openCL( clSetKernelArg(dk, 1, sizeof(gpu_heaps[outputHeap]), &gpu_heaps[outputHeap]) );
			openCL( clSetKernelArg(dk, 2, sizeof(gpu_heaps[copyOnTimeOut]), &gpu_heaps[copyOnTimeOut]) );
			openCL( clSetKernelArg(dk, 3, sizeof(totalUnfinished), &totalUnfinished) );

			openCL( clEnqueueNDRangeKernel(commands, dk, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
			openCL( clFinish(commands) );
			
			};
		
		result=totalUnfinished;
		break;
		};
	
	//	If we have any spare capacity, we split unfinished searches to make use of it.
	
	cl_uint nPreviousInputs = (cl_uint) global_ws;
	cl_uint nInputTarget = doSplit ? 2*totalUnfinished : totalUnfinished;
	if (nInputTarget > max_global_ws) nInputTarget = (cl_uint) max_global_ws;
	
	if (nInputTarget>totalUnfinished)
		{
		cl_kernel spk = kernels[splitKI];
/*
__kernel void split(
	__constant unsigned char *perms,			//	Table of permutation info for N-digit strings
	__global unsigned int *inputIndices,		//	Indices that will point to slots in outputs[] for next pass.
	__global struct string *outputs,			//	Heap of unfinished strings / slots we can reuse.
	unsigned int nInputTarget,					//	Number of inputs we want in next pass.
	unsigned int totalUnfinished,				//	Number of unfinished searches from last pass.
	unsigned int nPreviousInputs				//	Number of inputs in previous pass.
	)
*/

		global_ws = 1;
		while (TRUE)
			{
			if (global_ws >= nInputTarget-totalUnfinished) break;
			global_ws*=2;
			};

		if (global_ws <= max_local_ws) local_ws = global_ws;
		else local_ws = max_local_ws;

		openCL( clSetKernelArg(spk, 0, sizeof(gpu_perms), &gpu_perms));
		openCL( clSetKernelArg(spk, 1, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
		openCL( clSetKernelArg(spk, 2, sizeof(gpu_heaps[outputHeap]), &gpu_heaps[outputHeap]) );
		openCL( clSetKernelArg(spk, 3, sizeof(nInputTarget), &nInputTarget) );
		openCL( clSetKernelArg(spk, 4, sizeof(totalUnfinished), &totalUnfinished) );
		openCL( clSetKernelArg(spk, 5, sizeof(nPreviousInputs), &nPreviousInputs) );

		openCL( clEnqueueNDRangeKernel(commands, spk, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
		openCL( clFinish(commands) );
		
		};
	
	//	Next search will use current output heap for input
	
	inputHeap = outputHeap;
	
	nTrueInputs = (cl_uint) nInputTarget;
	
	nsp+=nc;
	runs++;
	
	if (totalRuns % 500 == 0)
		{
		time(&t2);
		double tsp = difftime(t2,t1);
		if (tsp > 10)
			{
			printf("Nodes searched per second = %.0lf, average number of threads = %.1lf\n",(nsp)/(tsp),((double)threads)/runs);
			nsp = 0;
			threads = 0;
			runs = 0;
			t1 = t2;
			};
		};
	};
	
return result;
}

//	Have a set of strings perform a limited search where they start by falling back one digit, and never add a new digit.
//
//	The initial strings are in gpu_heaps[inputHeap], as the first nTrueInputs contiguous entries;
//	the function result gives the number of unfinished outputs, which are contiguous in gpu_heaps[resultsHeap].

int fallBack(int inputHeap, int resultsHeap, int nTrueInputs,
	cl_int totalWaste, cl_uint pte0, cl_uint pro, cl_uint *maxPermsSeen, cl_ulong *totalNodesSearched, 
	char *prefix, int prefixLen)
{
size_t local_ws;
int nWorkGroups;
size_t global_ws;

//	Set the workgroup sizes

global_ws = 1;
while (global_ws < nTrueInputs) global_ws*=2;
if (global_ws <= max_local_ws)
	{
	local_ws = global_ws;
	nWorkGroups = 1;
	}
else
	{
	local_ws = max_local_ws;
	nWorkGroups = (int)(global_ws/local_ws);
	};

//	Initialise indices as the sequence 0,1,2,...

for (int i=0;i<NUM_GPU_INDICES;i++)
	{
	openCL( clEnqueueWriteBuffer(commands, gpu_inputIndices[i], CL_TRUE,
			0, nStringsPerHeap*sizeof(cl_uint), host_inputIndices, 0, NULL, NULL) );
	
	};

int super = pte0+2 >= FN;
int ocp = totalWaste >= FNM;
cl_uint nLocalThreads = (cl_uint) local_ws;
cl_uint paddedBlockSize = nLocalThreads + CONFLICT_FREE_OFFSET(nLocalThreads-1);
int srchKI = SEARCH_KERNEL + 2*NODES_KERNEL_OFFSET;
int splitKI = SPLIT_KERNEL + 2*NODES_KERNEL_OFFSET;

if (ocp)
	{
	srchKI += OCP_KERNEL_OFFSET;
	splitKI += OCP_KERNEL_OFFSET;
	};
if (super)
	{
	srchKI += SUPER_KERNEL_OFFSET;
	splitKI += SUPER_KERNEL_OFFSET;
	};

cl_kernel sk = kernels[srchKI];

/*
__kernel void search(
__constant unsigned char *perms,			//	0: Table of permutation info for N-digit strings
__constant unsigned short *mperm_res0,		//	1: Table of maximum number of permutations for each waste
int totalWaste,								//	2: Total waste we are allowing in each string		
unsigned int pte,							//	3: Permutations to exceed
unsigned int maxPermsSeen,					//	4: Maximum permutations seen in any string (not just this search)
unsigned long stepQuota,					//	5: Maximum number of steps to take before we quit
__global struct string *inputs,				//	6: String each thread starts from
__global unsigned int *inputIndices,		//	7: Indices used to select strings from inputs[]
__global struct string *outputs,			//	8: Final state of each thread's string
__global struct string *bestStrings,		//	9: Best string found by each thread
__local unsigned long *nsLocal,				//	10: Local buffer used for nodes searched
__global unsigned long *nsGlobal,			//	11: Number of nodes searched by each thread
__local struct stringStatus *psLocal,		//	12: Local buffer used for prefix sums
__global struct stringStatus *psGlobal,		//	13: Cumulative sum of 0|1 for finished|unfinished searches
__global unsigned int *blockSum,			//	14: Block totals for prefix sums
__local struct maxPermsLoc *mpLocal,		//	15: Local buffer used to determine best string across workgroup
__global struct maxPermsLoc *mpGlobal,		//	16: Info about best string in each block
unsigned int nLocalThreads,					//	17: Number of local threads in each workgroup
unsigned int nTrueInputs					//	18: Number of true inputs; threads will be padded out to a power of 2
#if SUPER
,
__global struct string *superperms,			//	19: Any superpermutations found
__local struct stringStatus *SpsLocal,		//	20: Local buffer used for prefix sums
__global struct stringStatus *SpsGlobal,	//	21: Cumulative sum of 0|1 for finished|unfinished searches
__global unsigned int *SblockSum			//	22: Block totals for prefix sums
#endif
)
*/

int outputHeap = 1;
cl_ulong smallQuota=1;

openCL( clSetKernelArg(sk, 0, sizeof(gpu_perms), &gpu_perms) );
openCL( clSetKernelArg(sk, 1, sizeof(gpu_mperm_res0), &gpu_mperm_res0) );
openCL( clSetKernelArg(sk, 2, sizeof(totalWaste), &totalWaste) );
openCL( clSetKernelArg(sk, 3, sizeof(pte0), &pte0) );
openCL( clSetKernelArg(sk, 4, sizeof(*maxPermsSeen), maxPermsSeen) );
openCL( clSetKernelArg(sk, 5, sizeof(smallQuota), &smallQuota) );
openCL( clSetKernelArg(sk, 6, sizeof(gpu_heaps[inputHeap]), &gpu_heaps[inputHeap]) );
openCL( clSetKernelArg(sk, 7, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
openCL( clSetKernelArg(sk, 8, sizeof(gpu_heaps[outputHeap]), &gpu_heaps[outputHeap]) );
openCL( clSetKernelArg(sk, 9, sizeof(gpu_heaps[2]), &gpu_heaps[2]) );
openCL( clSetKernelArg(sk, 10, sizeof(cl_ulong)*paddedBlockSize, NULL) );
openCL( clSetKernelArg(sk, 11, sizeof(gpu_nodesSearched), &gpu_nodesSearched) );
openCL( clSetKernelArg(sk, 12, sizeof(struct stringStatus)*paddedBlockSize, NULL) );
openCL( clSetKernelArg(sk, 13, sizeof(gpu_prefixSum[0]), &gpu_prefixSum[0]) );
openCL( clSetKernelArg(sk, 14, sizeof(gpu_blockSum[0]), &gpu_blockSum[0]) );
openCL( clSetKernelArg(sk, 15, sizeof(struct maxPermsLoc)*paddedBlockSize, NULL) );
openCL( clSetKernelArg(sk, 16, sizeof(gpu_maxPerms), &gpu_maxPerms) );
openCL( clSetKernelArg(sk, 17, sizeof(nLocalThreads), &nLocalThreads) );
openCL( clSetKernelArg(sk, 18, sizeof(nTrueInputs), &nTrueInputs) );

if (super)
	{
	openCL( clSetKernelArg(sk, 19, sizeof(gpu_heaps[3]), &gpu_heaps[3]) );
	openCL( clSetKernelArg(sk, 20, sizeof(struct stringStatus)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 21, sizeof(gpu_prefixSum[1]), &gpu_prefixSum[1]) );
	openCL( clSetKernelArg(sk, 22, sizeof(gpu_blockSum[1]), &gpu_blockSum[1]) );
	};

openCL( clEnqueueNDRangeKernel(commands, sk, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
openCL( clFinish(commands) );


//	Extract results of the search from GPU

cl_ulong nc=0;
cl_uint totalUnfinished=0;
unsigned int mp=0, mpw=0;

//	Counts of nodes searched (for each block / local workgroup)

openCL( clEnqueueReadBuffer(commands, gpu_nodesSearched, CL_TRUE,
	0, sizeof(cl_ulong)*nWorkGroups, host_nodesSearched, 0, NULL, NULL) );


//	Details of maximum permutations achieved (for each block / local workgroup)
	
openCL( clEnqueueReadBuffer(commands, gpu_maxPerms, CL_TRUE,
	0, sizeof(struct maxPermsLoc)*nWorkGroups, host_maxPerms, 0, NULL, NULL) );


//	Number of unfinished searches (for each block / local workgroup)

openCL( clEnqueueReadBuffer(commands, gpu_blockSum[0], CL_TRUE,
	0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );


for (int z=0;z<nWorkGroups;z++)
	{
	//	Compute total of nodes searched, in this run
	nc += host_nodesSearched[z];
	
	//	Identify GPU-wide max permutations achieved, in this run
	
	if (host_maxPerms[z].perms > mp)
		{
		mp = host_maxPerms[z].perms;
		mpw = host_maxPerms[z].where;
		};
		
	//	Convert block sums into cumulative sums, and compute overall total

	cl_uint t = host_blockSum[z];
	host_blockSum[z]=totalUnfinished;
	totalUnfinished += t;
	};

//	Write the cumulative sums of the block sums back to GPU, where they'll be needed by orderIndex

openCL( clEnqueueWriteBuffer(commands, gpu_blockSum[0], CL_TRUE,
	0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );


*totalNodesSearched += nc;
	
if (mp > *maxPermsSeen)
	{
	*maxPermsSeen=mp;
	
	//	Read the new best string from GPU
	
	openCL( clEnqueueReadBuffer(commands, gpu_heaps[2], CL_TRUE,
		mpw*sizeof(struct string), sizeof(struct string), &bestString, 0, NULL, NULL) );
	
	
	//	Need to adjust length to include final digit
	
	bestString.pos++;
	
	//	Validate string's waste and permutation counts
	
	static unsigned char fs[2*FN];
	int fsc = expandString(&bestString, fs, prefix, prefixLen,0);
	int pc = countPerms(fs, fsc);
	if (pc != bestString.perms)
		{
		printf("Mismatch between claimed permutations bestString.perms=%d and actual count, %d\n",bestString.perms,pc);
		print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, TRUE, FALSE);
		exit(EXIT_FAILURE);
		};
	int w = fsc - (NVAL-1) - pc;
	if (w != bestString.waste)
		{
		printf("Mismatch between claimed waste bestString.waste=%d and actual waste, %d\n",bestString.waste,w);
		print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, TRUE, FALSE);
		exit(EXIT_FAILURE);
		};
	
	bestSeenLen = expandString(&bestString, bestSeen, prefix, prefixLen, 1);
	bestSeenP = mp;
	if (mp > max_perm)
		{
		max_perm = mp;
		if (mp > pte0)
			{
			int pos = expandString(&bestString, curstr, prefix, prefixLen, 1);
			witnessCurrentString(pos);
			int tperm = setFlagsFromDigits(curstr,pos,0);
			maybeUpdateLowerBound(tperm,pos,tot_bl,max_perm);
			};
		};
	};
	
//	Number of superpermutations found (for each block / local workgroup)

if (super)
	{
	openCL( clEnqueueReadBuffer(commands, gpu_blockSum[1], CL_TRUE,
		0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );
	

	cl_uint superCount = 0;
	for (int z=0;z<nWorkGroups;z++)
		{
		//	Convert block sums into cumulative sums, and compute overall total

		cl_uint t = host_blockSum[z];
		host_blockSum[z]=superCount;
		superCount += t;
		};
	
	//	Write the cumulative sums of the block sums back to GPU, where they'll be needed by orderIndex

	openCL( clEnqueueWriteBuffer(commands, gpu_blockSum[1], CL_TRUE,
		0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, NULL) );
	
	
	if (superCount != 0)
		{
		printf("Found %d superpermutations\n",superCount);
		
		/*
__kernel void collectSuper(
__global struct stringStatus *SpsGlobal,		//	Cumulative sum of 0|1 for superpermutations (summed only within each block)
__global unsigned int *SblockSum,			//	Block totals for prefix sums
__global struct string *superIn,
__global struct string *superOut,
unsigned int nLocalThreads					//	Number of threads in each local workgroup
)
		*/
		
		cl_kernel ck = kernels[COLLECT_KERNEL];
		
		openCL( clSetKernelArg(ck, 0, sizeof(gpu_prefixSum[1]), &gpu_prefixSum[1]) );
		openCL( clSetKernelArg(ck, 1, sizeof(gpu_blockSum[1]), &gpu_blockSum[1]) );
		openCL( clSetKernelArg(ck, 2, sizeof(gpu_heaps[3]), &gpu_heaps[3]) );
		openCL( clSetKernelArg(ck, 3, sizeof(gpu_heaps[4]), &gpu_heaps[4]) );
		openCL( clSetKernelArg(ck, 4, sizeof(nLocalThreads), &nLocalThreads) );

		openCL( clEnqueueNDRangeKernel(commands, ck, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
		openCL( clFinish(commands) );
		
			
		openCL( clEnqueueReadBuffer(commands, gpu_heaps[4], CL_TRUE,
			0, superCount*sizeof(struct string), host_heaps[0], 0, NULL, NULL) );
		
		
		for (int z=0;z<superCount;z++)
			{
			host_heaps[0][z].pos++;
			print_string_data(stdout, &host_heaps[0][z], prefix, prefixLen, FALSE, TRUE, FALSE);
			
			int pos = expandString(&host_heaps[0][z], curstr, prefix, prefixLen, 1);
			witnessCurrentString(pos);
			if (z!=superCount-1) sleepForSecs(1);
			};
		};
	};

//	Quit if we have no unfinished searches

if (totalUnfinished==0) return 0;

//	Arrange for the first totalUnfinished indices in gpu_inputIndices[0] to point
//	to the strings with unfinished searches, and the rest (up to global_ws) to point
//	to empty splots in the heap.

cl_kernel ik = kernels[INDEX_KERNEL];

/*
__kernel void orderIndex(
__global struct stringStatus *psGlobal,		//	Cumulative sum of 0|1 for finished|unfinished searches
__global unsigned int *blockSum,			//	Block totals for prefix sums
__global unsigned int *inputIndices,		//	Indices used to select strings from inputs[]
unsigned int nLocalThreads,
unsigned int totalUnfinished
)
*/
openCL( clSetKernelArg(ik, 0, sizeof(gpu_prefixSum[0]), &gpu_prefixSum[0]) );
openCL( clSetKernelArg(ik, 1, sizeof(gpu_blockSum[0]), &gpu_blockSum[0]) );
openCL( clSetKernelArg(ik, 2, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
openCL( clSetKernelArg(ik, 3, sizeof(nLocalThreads), &nLocalThreads) );
openCL( clSetKernelArg(ik, 4, sizeof(totalUnfinished), &totalUnfinished) );

openCL( clEnqueueNDRangeKernel(commands, ik, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
openCL( clFinish(commands) );


/*
	__kernel void delegate(
	__global unsigned int *inputIndices,		//	Indices used to select strings from inputs[]
	__global struct string *inputs,				//	Final state of each thread's string
	__global struct string *outputs,			//	Final state of each thread's string
	unsigned int totalUnfinished				//	Number of true inputs; threads will be padded out to a power of 2
	)
*/	

cl_kernel dk = kernels[DELEGATE_KERNEL];

global_ws = 1;
while (TRUE)
	{
	if (global_ws >= totalUnfinished) break;
	global_ws*=2;
	};

if (global_ws <= max_local_ws) local_ws = global_ws;
else local_ws = max_local_ws;

openCL( clSetKernelArg(dk, 0, sizeof(gpu_inputIndices[0]), &gpu_inputIndices[0]) );
openCL( clSetKernelArg(dk, 1, sizeof(gpu_heaps[outputHeap]), &gpu_heaps[outputHeap]) );
openCL( clSetKernelArg(dk, 2, sizeof(gpu_heaps[resultsHeap]), &gpu_heaps[resultsHeap]) );
openCL( clSetKernelArg(dk, 3, sizeof(totalUnfinished), &totalUnfinished) );

openCL( clEnqueueNDRangeKernel(commands, dk, 1, NULL, &global_ws, &local_ws, 0, NULL, NULL) );
openCL( clFinish(commands) );

	
return totalUnfinished;
}

//	Given an ASCII prefix string, convert it to a "string" structure as used by the GPU kernels

void prefixToStringStructure(char *prefix, int prefixLen, struct string *s)
{
for (int k=0;k<MSL;k++) s->digits[k]=0;
for (int k=0;k<NVAL;k++) s->oneCycleBins[k]=0;
s->oneCycleBins[NVAL] = FNM;
for (int k=0;k<=FNM+NVAL;k++) s->digits[OCP_OFFSET+k] = NVAL << PERMUTATION_SHIFT;

s->perms=0;
unsigned short lastN=0;
unsigned char d=0;
for (int k=0;k<prefixLen;k++)
	{
	d = prefix[k]-'1';
	lastN = (lastN/NVAL) + NN1*d;
	unsigned char P = perms[lastN];
	unsigned short pNum = (P>=FNM) ? FN : (d*FNM+P);
	
	//	Count any new permutation we visit
	
	if (P<FNM && (s->digits[pNum] & PERMUTATION_BITS)==0)
		{
		s->perms++;
		
		//	Reduce the number of unvisited permutations in this 1-cycle, and adjust
		//	counts in bins
		
		int prevC = s->digits[OCP_OFFSET+P] >> PERMUTATION_SHIFT;
		s->digits[OCP_OFFSET+P] -= PERMUTATION_LSB;
		s->oneCycleBins[prevC]--;
		s->oneCycleBins[prevC-1]++;
		};
	
	//	Set the flag, either for permutation or "no permutation"
	
	s->digits[pNum] |= PERMUTATION_LSB;
	
	//	Store the last N digits in the string.
	
	int j = k + NVAL - prefixLen;
	if (j >= 0)
		{
		s->digits[j] |= d;
		};
	};

s->lastN1 = lastN / NVAL;
s->waste=prefixLen - (NVAL-1) - s->perms;

//	Initialise first character after the N from the prefix to be the cyclic successor to the last digit of the prefix.

s->digits[NVAL] |= (d+1)%NVAL;
s->pos = NVAL;
s->rootDepth = NVAL;
}

//	Do a search on the GPU for a range of known values, to validate the setup

void validationChecks(char *prefix, int prefixLen, int waste1, int waste2, int verbose)
{
struct string br;
prefixToStringStructure(prefix, prefixLen, &br);

cl_uint maxPermsSeen=0;
totalNodesSearched=0;
cl_int totalWaste=waste1;
cl_uint pte=mperm_res[totalWaste-1]+2*(NVAL-4);
while (totalWaste <= waste2)						//	Loop for values of waste
	{
	if (pte >= FN) pte = FN-1;
	cl_uint pro = mperm_res[totalWaste-1]+NVAL+1;	//	Permutations previously ruled out for this waste
	if (pro > FN+1) pro = FN+1;
	
	while (TRUE)									//	Loop for values of pte, which might include backtracking if we aim too high
		{
		searchString(&br, 1, totalWaste, pte, pro, &maxPermsSeen, &totalNodesSearched, verbose,
			prefix, prefixLen, FALSE, 0, -1, 0);
		
		if (maxPermsSeen > pte) break;				//	We hit our target for perms to exceed
		
		pro = pte+1;								//	We ruled out a permutation count
//		printf("For totalWaste=%d, RULED OUT %d permutations\n",totalWaste,pro);
		pte--;										//	Lower our target and repeat
		};
	
	//	Verify value against known results
	
	if (knownN && totalWaste < numKnownW && maxPermsSeen != knownN[2*totalWaste+1])
		{
		printf("Mismatch between maxPermsSeen=%d found by program and known result %d\n",maxPermsSeen,knownN[2*totalWaste+1]);
		exit(EXIT_FAILURE);
		};
		
	printf("For totalWaste=%d, maxPermsSeen=%d\n",totalWaste,maxPermsSeen);
//	print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, TRUE, FALSE);

	//	Update mperm_res, both in host and in GPU
	
	mperm_res[totalWaste] = maxPermsSeen;
	openCL( clEnqueueWriteBuffer(commands, gpu_mperm_res0, CL_TRUE,
		(MPERM_OFFSET+totalWaste)*sizeof(*mperm_res0), sizeof(*mperm_res0), mperm_res+totalWaste, 0, NULL, NULL) );
	
		
	if (maxPermsSeen==FN) break;
	
	pte = maxPermsSeen + 2*(NVAL-4);
	totalWaste++;
	};
}

//	Do a long search on the GPU to get time benchmarks

void benchMarking(char *prefix, int totalWaste, int pte)
{
int prefixLen = (int)strlen(prefix);
struct string br;
prefixToStringStructure(prefix, prefixLen, &br);
printf("Running benchmarking with prefix %s, totalWaste=%d, perms_to_exceed=%d\n",prefix,totalWaste,pte);

cl_uint maxPermsSeen=0;
totalNodesSearched=0;
searchString(&br, 1, totalWaste, pte, FN+1, &maxPermsSeen, &totalNodesSearched, FALSE,
			prefix, prefixLen, FALSE, 0, -1, 0);
printf("Best string found: ");
print_string_data(stdout, &bestString, prefix, prefixLen, FALSE, TRUE, FALSE);
}

//	Do a search on the GPU, for a single prefix and total waste value

void searchPrefix(char *prefix, int prefixLen, cl_int totalWaste, cl_uint pte, cl_uint pro)
{
struct string br;
prefixToStringStructure(prefix, prefixLen, &br);

bestString = br;
cl_uint maxPermsSeen=br.perms;

if (pte >= FN) pte = FN-1;
if (pro > FN+1) pro = FN+1;

//	Update mperm_res in GPU

openCL( clEnqueueWriteBuffer(commands, gpu_mperm_res0, CL_TRUE, 0, mpermSizeBytes, mperm_res0, 0, NULL, NULL) );


//	Search from the string we've constructed

totalNodesSearched=0;
subTreesDelegated=0;
subTreesLocal=0;

//	Do a search based on current prefix

int inputHeap=5;
int unfinished = searchString(&br, 1, totalWaste, pte, pro, &maxPermsSeen, &totalNodesSearched, FALSE,
	prefix, prefixLen, TRUE, timeBeforeSplit, inputHeap, timeBeforeSplit-30);
	
while (unfinished > 0)
	{
	printf("\nSplitting current task, dealing with %d unfinished searches ...\n",unfinished);

	//	We have some strings whose searches haven't finished, placed in gpu_heaps[5].
	//	Extract these unfinished strings into host memory.

	openCL( clEnqueueReadBuffer(commands, gpu_heaps[inputHeap], CL_TRUE,
		0, unfinished*sizeof(struct string), host_heaps[1], 0, NULL, NULL) );
	

	//	searchString() will have guaranteed (via a call to the kernel search() with NODES_EXIT true) that each unfinished string
	//	has just gained a new digit at cs.digits[cs.pos], and so the prefix where the digits up to cs.pos-1 are fixed describes
	//	a completely unsearched subtree that can be delegated; it just needs to be copied with rootDepth set to pos.
	
	#define MSS 256
	#define RES_HEAP 2
	
	for (int z=0;z<unfinished;z+=MSS)
		{
		if (maxPermsSeen > pte) pte = maxPermsSeen;
		if (pte >= FN) pte = FN-1;
		if (pte+1 >= pro) break;
		
		int nsrch = MSS;
		if (z+nsrch > unfinished) nsrch=unfinished-z;
		
		//	For each unfinished string, see if we can search the subtree locally within the time limit; if not, delegate it.
		
		struct string *ufs = host_heaps[1]+z;
		for (int j=0;j<nsrch;j++)
			{
			ufs[j].rootDepth = ufs[j].pos;
			ufs[j].marker=j;
			};
		
		int tis = nsrch*timeInSubtree;
		if (tis > SUBTREE_TIME_CEILING) tis = SUBTREE_TIME_CEILING;
		 
		int subtree = searchString(ufs, nsrch, totalWaste, pte, pro, &maxPermsSeen, &totalNodesSearched, FALSE,
			prefix, prefixLen, TRUE, tis, RES_HEAP, 2*tis);
			
		if (subtree == 0) subTreesLocal+=nsrch;		//	All searches completed locally within the time limit
		else										//	Some searches did not finish, so need to delegate to another task
			{
			openCL( clEnqueueReadBuffer(commands, gpu_heaps[RES_HEAP], CL_TRUE,
				0, subtree*sizeof(struct string), host_heaps[0], 0, NULL, NULL) );
			
			
			static char flags[MSS];
			for (int f=0;f<nsrch;f++) flags[f]=FALSE;
			for (int k=0;k<subtree;k++)
				{
				int f=host_heaps[0][k].marker;
				flags[f]=TRUE;
				};

			for (int f=0;f<nsrch;f++)
			if (flags[f])
				{
				int pos = expandString(ufs+f, curstr, prefix, prefixLen, 1);
				computeBranchOrder(curstr,pos,curi);
				int sres=splitTask(pos);
				if (sres>=2) done=TRUE;
				if (sres==3) cancelledTask=TRUE;
				subTreesDelegated++;
				if (done || cancelledTask) return;
				}
			else subTreesLocal++;
			};
		
		};

	printf("Completed %d subtrees locally, %d delegated so far ...\n",subTreesLocal,subTreesDelegated);
	if (pte+1 >= pro) break;
	
	//	We now need to advance all the strings to the point where they are about to search another subtree, or they finish.
	
	int resultsHeap = 5+6-inputHeap;
	
	sleepForSecs(1);
	unfinished = fallBack(inputHeap, resultsHeap, unfinished,
		totalWaste, pte, pro, &maxPermsSeen, &totalNodesSearched, 
		prefix, prefixLen);
	
	inputHeap = resultsHeap;
	};
}

//	Main program
//	------------

int main(int argc, const char * argv[])
{
static char buffer[BUFFER_SIZE];
FILE *fp;
int justTest=FALSE;
timeQuotaMins=0;
timeQuotaHardMins=0;
longRunner=FALSE;
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

if (TEST_VERSION)
	{
	printf("On CPU, sizeof(struct string)=%d, sizeof(struct maxPermsLoc)=%d, sizeof(struct stringStatus)=%d\n",
		(int)sizeof(struct string),
		(int)sizeof(struct maxPermsLoc),
		(int)sizeof(struct stringStatus)
		);
	}
else
	{
	printf("Random seed is: %d\n", rseed);
	srand(rseed);

	while(TRUE)
		{
		programInstance=rand();
		sprintf(SERVER_RESPONSE_FILE_NAME,SERVER_RESPONSE_FILE_NAME_TEMPLATE,programInstance);
		sprintf(LOG_FILE_NAME,LOG_FILE_NAME_TEMPLATE,programInstance);
		sprintf(STOP_FILE_NAME,STOP_FILE_NAME_TEMPLATE,programInstance);
		sprintf(QUIT_FILE_NAME,QUIT_FILE_NAME_TEMPLATE,programInstance);
		
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
	};

//	Process command line arguments

for (int i=1;i<argc;i++)
	{
	if (strcmp(argv[i],"test")==0) justTest=TRUE;
	else if (strcmp(argv[i],"longRunner")==0) longRunner=TRUE;
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
	else if (strcmp(argv[i],"gpuName")==0)
		{
		if (i+1 >= argc)
			{
			printf("Expected an argument after the gpuName option\n");
			exit(EXIT_FAILURE);
			};
		gpuName = argv[i+1];
		i++;
		}
	else if (strcmp(argv[i],"gpuPlatform")==0)
		{
		if (i+1 >= argc)
			{
			printf("Expected an argument after the gpuPlatform option\n");
			exit(EXIT_FAILURE);
			};
		gpuPlatform = argv[i+1];
		i++;
		}
	else if (strcmp(argv[i],"gpuDeviceNumber")==0)
		{
		if (i+1 >= argc)
			{
			printf("Expected an argument after the gpuDeviceNumber option\n");
			exit(EXIT_FAILURE);
			};
		if (sscanf(argv[i+1],"%d",&gpuDeviceNumber)!=1)
			{
			printf("Expected a number after gpuDeviceNumber option\n");
			exit(EXIT_FAILURE);
			};
		i++;
		}
	else
		{
		printf("Unknown option %s\n",argv[i]);
		exit(EXIT_FAILURE);
		};
	};
	
initOpenCL(gpuName,gpuPlatform,gpuDeviceNumber,TRUE,TRUE);

printf("Starting GPU validation checks\n");
validationChecks("123456",NVAL,1,90,VERBOSE);
printf("Successfully completed validation checks!\n\n");

if (TEST_VERSION)
	{
	benchMarking("123456123451623451263451236451234651324651342651346251346521436521463521465321465231462531462351462315462314562341562431562413562415362451362453162453612456",
	118,628);
	exit(0);
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

if (justTest)
	{
	cleanupOpenCL(TRUE);
	exit(0);
	};

//	Register with the server, offering to do actual work

registerClient();

sprintf(buffer,
	"To stop the program automatically BETWEEN tasks, create a file %s or %s in the working directory",
	STOP_FILE_NAME,STOP_FILE_ALL);
logString(buffer);

sprintf(buffer,
	"To stop the program automatically EVEN DURING tasks, create a file %s or %s in the working directory\n",
	QUIT_FILE_NAME,QUIT_FILE_ALL);
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
		cleanupOpenCL(TRUE);
		exit(0);
		};
	
	#endif
	
	//	Check for STOP/QUIT files
	
	for (int k=0;k<4;k++)
		{
		fp = fopen(sqFiles[k],"r");
		if (fp!=NULL)
			{
			sprintf(buffer,"Detected the presence of the file %s, so stopping.\n",sqFiles[k]);
			logString(buffer);
			unregisterClient();
			cleanupOpenCL(TRUE);
			exit(0);
			};
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
			cleanupOpenCL(TRUE);
			exit(0);
			};
		};
		
	//	If we did a very quick task, maybe sleep
	
	if (serverPressure>0 && startedCurrentTask>0)
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

if (n!=NVAL)
	{
	printf("This program needs to be recompiled (with the symbol NVAL in NInfo.h set to %d) in order to work with N=%d\n",n,n);
	cleanupOpenCL(TRUE);
	exit(EXIT_FAILURE);
	};

//	Storage for current string

MFREE(curstr)
CHECK_MEM( curstr = (unsigned char *)malloc(2*fn*sizeof(char)) )
MFREE(curi)
CHECK_MEM( curi = (unsigned char *)malloc(2*fn*sizeof(char)) )
MFREE(asciiString)
CHECK_MEM( asciiString = (char *)malloc(2*fn*sizeof(char)) )
MFREE(asciiString2)
CHECK_MEM( asciiString2 = (char *)malloc(2*fn*sizeof(char)) )
MFREE(bestSeen)
CHECK_MEM( bestSeen = (unsigned char *)malloc(2*fn*sizeof(char)) )

//	Storage for things associated with different numbers of wasted characters

maxW = fn;

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
}

//	Given a sequence of digits of specified length, set the unvisited[] and oneCycleCounts[]/oneCycleBins[]
//
//	offs subtracted from each digit should give a digit 1...N

int setFlagsFromDigits(const unsigned char *digits, int digitsLen, int offs)
{
//	Initialise all permutations as unvisited

for (int i=0; i<maxInt; i++) unvisited[i] = TRUE;

//	Initialise 1-cycle information

for (int i=0;i<maxInt;i++) oneCycleCounts[i]=n;
for (int b=0;b<n;b++) oneCycleBins[b]=0;
oneCycleBins[n]=noc;

//	Start the current string with the specified digits;
//	this will involve visiting various permutations and changes to 1-cycle counts

int tperm0=0;
int pf=0;
for (int j0=0;j0<digitsLen;j0++)
	{
	int d = digits[j0]-offs;
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
return tperm0;
}

void doTask()
{
static char buffer[BUFFER_SIZE];

tot_bl = currentTask.w_value;

//	Set baseline times

time(&startedCurrentTask);
timeOfLastTimeReport = timeOfLastTimeCheck = startedCurrentTask;

timeBeforeSplit = currentTask.timeBeforeSplit;
maxTimeInSubtree = currentTask.maxTimeInSubtree;
timeInSubtree = maxTimeInSubtree;
timeBetweenServerCheckins = currentTask.timeBetweenServerCheckins;

cancelledTask=FALSE;
done=FALSE;
max_perm = currentTask.perm_to_exceed;
isSuper = (max_perm==fn);

for (int k=0;k<currentTask.prefixLen;k++) bestSeen[k]=currentTask.prefix[k]-'0';
bestSeenLen = currentTask.prefixLen;
bestSeenP = 0;

if (isSuper || max_perm+1 < currentTask.prev_perm_ruled_out)
	{
	searchPrefix(currentTask.prefix, currentTask.prefixLen, tot_bl, currentTask.perm_to_exceed, currentTask.prev_perm_ruled_out);
	};
	
for (int k=0;k<bestSeenLen;k++) asciiString[k] = '0'+bestSeen[k];
asciiString[bestSeenLen] = '\0';

#if !NO_SERVER

//	Finish with current task with the server

if (!cancelledTask)
while (TRUE)
	{
	sprintf(buffer,"action=finishTask&id=%u&access=%u&str=%s&pro=%u&team=%s&nodeCount=%"PRId64,
		currentTask.task_id, currentTask.access_code, asciiString, max_perm+1, teamName, totalNodesSearched);
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
	bestSeenP,totalNodesSearched,tskMin,tskSec);
logString(buffer);
if (subTreesDelegated>0 || subTreesLocal>0)
	{
	sprintf(buffer,"Delegated %d sub-trees, completed %d locally",subTreesDelegated,subTreesLocal);
	logString(buffer);
	};
sprintf(buffer,"--------------------------------------------------------\n");
logString(buffer);
}

void timeCheck(time_t timeNow)
{
static char buffer[BUFFER_SIZE];

double timeSpentOnTask = difftime(timeNow, startedCurrentTask);
double timeSinceLastTimeReport = difftime(timeNow, timeOfLastTimeReport);
double timeSinceLastServerCheckin = difftime(timeNow, timeOfLastServerCheckin);

//	Check for QUIT files

for (int k=2;k<4;k++)
	{
	FILE *fp = fopen(sqFiles[k],"r");
	if (fp!=NULL)
		{
		sprintf(buffer,"Detected the presence of the file %s, so stopping.\n",sqFiles[k]);
		logString(buffer);
		unregisterClient();
		cleanupOpenCL(TRUE);
		exit(0);
		};
	};

if (timeQuotaHardMins > 0)
	{
	double elapsedTime = difftime(timeNow, startedRunning);
	if (elapsedTime / 60 > timeQuotaHardMins)
		{
		logString("A 'timeLimitHard' quota has been reached, so the program will relinquish the current task with the server then quit.\n");
		unregisterClient();
		cleanupOpenCL(TRUE);
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
	printf("\n");
	
	timeOfLastTimeReport = timeNow;
	};

if (timeSinceLastServerCheckin > timeBetweenServerCheckins)
	{
	//	When we check in for this task, we might be told it's redundant
	
	int sres=checkIn();
	if (sres>=2) done=TRUE;
	if (sres==3) cancelledTask=TRUE;
	};

//	Taper off time in subtrees if we have been running too long

if ((!longRunner) && timeSpentOnTask > TAPER_THRESHOLD)
	{
	int oldTimeInSubtree = timeInSubtree;
	timeInSubtree = (int)(maxTimeInSubtree * exp(-(timeSpentOnTask-TAPER_THRESHOLD)/TAPER_DECAY));
	if (timeInSubtree < 1) timeInSubtree=1;
	if (timeInSubtree != oldTimeInSubtree)
		{
		sprintf(buffer,"Task taking too long, will only spend %d seconds in subtrees ...",timeInSubtree);
		logString(buffer);
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

#if !TEST_VERSION
FILE *fp = fopen(LOG_FILE_NAME,"at");
if (fp==NULL)
	{
	printf("Error: Unable to open log file %s to append (%s)\n",LOG_FILE_NAME, strerror(errno));
	exit(EXIT_FAILURE);
	};
fprintf(fp,"%s %s\n",tsb, s);
fclose(fp);
#endif
}

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
//	Pre-empty the response file so it does not end up with any misleading content from a previous command if the
//	current command fails.

FILE *fp = fopen(SERVER_RESPONSE_FILE_NAME,"wt");
if (fp==NULL)
	{
	printf("Error: Unable to write to server response file %s (%s)\n",SERVER_RESPONSE_FILE_NAME, strerror(errno));
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
	printf("Error: Unable to open server response file %s to read (%s)\n",SERVER_RESPONSE_FILE_NAME, strerror(errno));
	exit(EXIT_FAILURE);
	};
fseek(fp,0,SEEK_END);
if (ftell(fp)==0) res=-1;
fclose(fp);

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
	printf("Unable to read from server response file %s (%s)\n",SERVER_RESPONSE_FILE_NAME, strerror(errno));
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
	
	if (strncmp(buffer,"Pressure: ",10)==0)
		{
		if (sscanf(buffer+10, "%d", &serverPressure) !=1) serverPressure=0;
		};
	
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
	printf("Unable to read from server response file %s (%s)\n",SERVER_RESPONSE_FILE_NAME, strerror(errno));
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
	printf("Unable to read from server response file %s (%s)\n",SERVER_RESPONSE_FILE_NAME, strerror(errno));
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
//	waste of 1. 

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
