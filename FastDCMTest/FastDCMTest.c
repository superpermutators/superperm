//
//  FastDCMTest.c
//
//  Author:		Greg Egan
//	Version:	4.2
//	Date:		27 June 2019
//
//	This is a TEST version of the DistributedChaffinMethod client that uses
//	OpenCL to run a parallel version of the search on a GPU.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>
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

#ifdef __linux__

//	For Linux

#include <CL/opencl.h>

#else

//	For MacOS

#define CL_SILENCE_DEPRECATION 1
#include <OpenCL/opencl.h>

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

#define PROFILE TRUE

#define CHECK_MEM(p) if ((p)==NULL) {printf("Insufficient CPU memory\n"); exit(EXIT_FAILURE);};

#if PROFILE

#define PROFILING_FLAGS CL_QUEUE_PROFILING_ENABLE

cl_event profilingEvent;
cl_ulong commandStart, commandEnd;
cl_ulong searchKernelTime = 0, indexKernelTime = 0, splitKernelTime, writeToGPUTime = 0, readFromGPUTime = 0;
cl_ulong totalNodesSearched=0;

#define PE (&profilingEvent)

#define RECORD_TIME(t) \
openCL( clGetEventProfilingInfo(profilingEvent, CL_PROFILING_COMMAND_START, sizeof(commandStart), &commandStart, NULL) ); \
openCL( clGetEventProfilingInfo(profilingEvent, CL_PROFILING_COMMAND_END, sizeof(commandEnd), &commandEnd, NULL) ); \
t += commandEnd - commandStart;

#else

#define PROFILING_FLAGS 0
#define PE (NULL)
#define RECORD_TIME(t)

#endif

//	Functions

void cleanupOpenCL(void);

//	Table of strings corresponding to each permutation number

char pstrings[FN+NVAL][NVAL+1];

//	Details of GPU

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

#define SEARCH_KERNEL 0
#define INDEX_KERNEL 1
#define SPLIT_KERNEL 2

const char *kernelFiles[][3] = {
	{"Kernels/searchKernel.txt", "search", ""},
	{"Kernels/orderIndexKernel.txt", "orderIndex", ""},
	{"Kernels/splitKernel.txt", "split", ""}
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

cl_uint *host_blockSum = NULL;
cl_uint *host_inputIndices = NULL;
struct maxPermsLoc *host_maxPerms = NULL;
cl_ulong *host_nodesSearched = NULL;

//	GPU memory

#define NUM_GPU_HEAPS 3

cl_mem gpu_perms=NULL, gpu_mperm_res0=NULL;
cl_mem gpu_heaps[NUM_GPU_HEAPS];
cl_mem gpu_inputIndices=NULL;
cl_mem gpu_nodesSearched=NULL;
cl_mem gpu_prefixSum=NULL;
cl_mem gpu_blockSum=NULL;
cl_mem gpu_maxPerms=NULL;

//	Quota for nodes searched

#define DEFAULT_NODE_QUOTA 1000

cl_ulong nodeQuota = DEFAULT_NODE_QUOTA;

//	Known values from previous calculations

int known3[][2]={{0,3},{1,6}};
int known4[][2]={{0,4},{1,8},{2,12},{3,14},{4,18},{5,20},{6,24}};
int known5[][2]={{0,5},{1,10},{2,15},{3,20},{4,23},{5,28},{6,33},{7,36},{8,41},{9,46},{10,49},{11,53},{12,58},{13,62},{14,66},{15,70},{16,74},{17,79},{18,83},{19,87},{20,92},{21,96},{22,99},{23,103},{24,107},{25,111},{26,114},{27,116},{28,118},{29,120}};
int known6[][2]={{0,6},{1,12},{2,18},{3,24},{4,30},{5,34},{6,40},{7,46},{8,52},{9,56},{10,62},{11,68},{12,74},{13,78},{14,84},{15,90},{16,94},{17,100},{18,106},{19,112},{20,116},{21,122},{22,128},{23,134},{24,138},{25,144},{26,150},{27,154},{28,160},{29,166},{30,172},{31,176},{32,182},{33,188},{34,192},{35,198},{36,203},{37,209},{38,214},{39,220},{40,225},{41,230},{42,236},{43,241},{44,246},{45,252},{46,257},{47,262},{48,268},{49,274},{50,279},{51,284},{52,289},{53,295},{54,300},{55,306},{56,311},{57,316},{58,322},{59,327},{60,332},{61,338},{62,344},{63,349},{64,354},{65,360},{66,364},{67,370},{68,375},{69,380},{70,386},{71,391},{72,396},{73,402},{74,407},{75,412},{76,418},{77,423},{78,429},{79,434},{80,439},{81,445},{82,450},{83,455},{84,461},{85,465},{86,470},{87,476},{88,481},{89,486},{90,492},{91,497},{92,502},{93,507},{94,512},{95,518},{96,523},{97,528},{98,534},{99,539},{100,543},{101,548},{102,552},{103,558},{104,564},{105,568},{106,572},{107,578},{108,583},{109,589},{110,594},{111,599},{112,604},{113,608},{114,613},{115,618},{116,621},{117,625}};

int *knownN=NULL;
int numKnownW=0;

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
cleanupOpenCL();
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


int initOpenCL(const char *gpuChoice)
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
CHECK_MEM( perms = (cl_uchar *) malloc(permsSizeBytes) )
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
		perms[s] = pcount[digits[NVAL-1]]++;
		
		//	Create string version of permutation
		
		unsigned short pNum = digits[NVAL-1]*FNM+perms[s];
		for (int k=0;k<NVAL;k++) pstrings[pNum][k]='1'+digits[k];
		pstrings[pNum][NVAL]='\0';
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

			//	We've hit the first length, l, with a repetition, so l-1 is longest without
			
			if (!repFree)
				{
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
CHECK_MEM( mperm_res0 = (cl_ushort *)malloc(mpermSizeBytes) )
cbNeeded += mpermSizeBytes;

for (int k=0;k<MPERM_OFFSET;k++) mperm_res0[k]=0;
mperm_res = mperm_res0+MPERM_OFFSET;
mperm_res[0]=NVAL;
if (knownN)
	for (int k=0;k<numKnownW;k++)
		mperm_res[k] = knownN[2*k+1];

//	See what OpenCL platforms and GPUs are available

#define MAX_CL_PLATFORMS 5
#define MAX_CL_DEVICES 5

int foundPlatformAndGPU=FALSE;
cl_platform_id platforms[MAX_CL_PLATFORMS];
cl_device_id devices[MAX_CL_PLATFORMS][MAX_CL_DEVICES];
cl_uint num_platforms, num_devices[MAX_CL_PLATFORMS];

openCL( clGetPlatformIDs(MAX_CL_PLATFORMS,
                        platforms,
                        &num_platforms) );
                        
#define MAX_CL_INFO 2048
static char openCL_info[MAX_CL_INFO];

printf("Found %u OpenCL platform%s on this system:\n", num_platforms, num_platforms==1?"":"s");
if (num_platforms == 0) exit(EXIT_FAILURE);

for (int i=0;i<num_platforms;i++)
	{
	printf("Platform %d:\n",i+1);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, MAX_CL_INFO, openCL_info, NULL) );
	printf("Profile: %s\n",openCL_info);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, MAX_CL_INFO, openCL_info, NULL) );
	printf("Version: %s\n",openCL_info);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, MAX_CL_INFO, openCL_info, NULL) );
	printf("Name: %s\n",openCL_info);
	
	openCL( clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, MAX_CL_INFO, openCL_info, NULL) );
	printf("Vendor: %s\n",openCL_info);
	
	//	Get the devices available on this platform
	
	openCL( clGetDeviceIDs(platforms[i],
                      CL_DEVICE_TYPE_GPU,
                      MAX_CL_DEVICES,
                      devices[i],
                      &num_devices[i]) );
                      
	printf("Found %u GPU device%s for this platform:\n",num_devices[i],num_devices[i]==1?"":"s");
	for (int j=0; j<num_devices[i]; j++)
		{
		printf("\tDevice %d:\n",j+1);
		
		int gpuOK = TRUE, gpuDeviceNameOK = TRUE;
		
		//	Details of GPU

		cl_ulong gms;				//	Global memory size
		cl_ulong mma;				//	Maximum memory allocation
		cl_ulong lms;				//	Local memory size
		cl_ulong cbs;				//	Constant buffer size
		cl_uint cu;					//	Number of compute units
		size_t mws;					//	Maximum workgroup size
		cl_bool ca, la;

		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_NAME, MAX_CL_INFO, openCL_info, NULL) );
		printf("\tDevice name: %s\n",openCL_info);

		if (gpuChoice)
			{
			gpuDeviceNameOK = (strncmp(gpuChoice,openCL_info,strlen(gpuChoice))==0);
			};

		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_VENDOR, MAX_CL_INFO, openCL_info, NULL) );
		printf("\tDevice vendor: %s\n",openCL_info);
		
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_COMPILER_AVAILABLE, sizeof(ca), &ca, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_LINKER_AVAILABLE, sizeof(la), &la, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(gms), &gms, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(mma), &mma, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(lms), &lms, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cbs), &cbs, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cu), &cu, NULL) );
		openCL( clGetDeviceInfo(devices[i][j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(mws), &mws, NULL) );
		printf("\tCompiler available: %s\n", ca == CL_TRUE ? "Yes" : "No");
		printf("\tLinker available: %s\n", ca == CL_TRUE ? "Yes" : "No");
		printf("\tGlobal memory size = %u Mb\n", (unsigned int)(gms/Mb));
		printf("\tMaximum memory allocation = %u Mb\n", (unsigned int)(mma/Mb));
		printf("\tLocal memory size = %u Kb\n", (unsigned int)(lms/Kb));
		printf("\tConstant buffer size = %u Kb\n", (unsigned int)(cbs/Kb));
		printf("\tCompute units = %u\n", (unsigned int)cu);
		printf("\tMaximum workgroup size = %u\n", (unsigned int)mws);
		
		if (!gpuDeviceNameOK)
			{
			printf("\t[This device name does not start with the user-specified \"%s\", so it has been ruled out]\n",gpuChoice);
			gpuOK = FALSE;
			};
		
		if (ca != CL_TRUE && la != CL_TRUE)
			{
			printf("\t[This device does not have a compiler/linker available]\n");
			gpuOK = FALSE;
			};
			
		if (cbs < cbNeeded)
			{
			printf("\t[The program needs %"PRIu64" bytes of constant memory buffer (%u Kb), but the GPU only permits %u Kb]\n",
				cbNeeded, (unsigned int)cbNeeded/Kb, (unsigned int)cbs/Kb);
			gpuOK = FALSE;
			};
			
		if (gpuOK)
			{
			printf("\t[This GPU meets all the requirements to run the program]\n");
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
				}
			else
				{
				printf("\t[But a previously listed GPU would run at least as many threads, so it will be used instead.]\n");
				};
			};
		};
	};
printf("\n");

if (!foundPlatformAndGPU)
	{
	printf("No suitable OpenCL platform / GPU device was found\n");
	exit(EXIT_FAILURE);
	};

//	Get an OpenCL context

cl_int clErr;
cl_context_properties cprop[] = {CL_CONTEXT_PLATFORM, (cl_context_properties) gpu_platform, 0};
context = clCreateContext(cprop, 1, &gpu_device, NULL, NULL, &clErr);
openCL(clErr);
printf("Created openCL context\n");

commands = clCreateCommandQueue(context, gpu_device, PROFILING_FLAGS, &clErr);
openCL(clErr);
printf("Created openCL command queue\n\n");

//	Read in / create headers and prepend them to kernel source code

size_t headerLengthTotal = 0;
nHeaders = sizeof(headerFiles) / sizeof(headerFiles[0]);
CHECK_MEM( headerLengths = (size_t *)malloc(nHeaders * sizeof(size_t)) )
CHECK_MEM( headerSourceCode = (char **)calloc(nHeaders, sizeof(char *)) )
for (int i=0;i<nHeaders;i++)
	{
	headerSourceCode[i] = readTextFile(headerFiles[i],headerLengths+i);
	printf("Read header file %s (%lu bytes)\n",headerFiles[i],headerLengths[i]);
	headerLengthTotal += headerLengths[i];
	};
	
nKernels = sizeof(kernelFiles) / (sizeof(kernelFiles[0]));
CHECK_MEM( kernelLengths = (size_t *)malloc(nKernels * sizeof(size_t)) )
CHECK_MEM( kernelSourceCode = (char **)calloc(nKernels, sizeof(char *)) )
CHECK_MEM( kernelFullSource = (char **)calloc(nKernels, sizeof(char *)) )
for (int i=0;i<nKernels;i++)
	{
	kernelSourceCode[i] = readTextFile(kernelFiles[i][0],kernelLengths+i);
	printf("Read kernel file %s (%lu bytes)\n",kernelFiles[i][0],kernelLengths[i]);
	
	size_t klen = strlen(kernelFiles[i][2]) + headerLengthTotal + kernelLengths[i];
	
	CHECK_MEM( kernelFullSource[i] = (char *)malloc((klen+1) * sizeof(char)) )
	
	size_t ptr = 0;
	
	strcpy(kernelFullSource[i]+ptr, kernelFiles[i][2]);
	ptr += strlen(kernelFiles[i][2]);
	
	for (int j=0;j<nHeaders;j++)
		{
		strcpy(kernelFullSource[i]+ptr, headerSourceCode[j]);
		ptr += headerLengths[j];
		};
	strcpy(kernelFullSource[i]+ptr, kernelSourceCode[i]);
	};
	
printf("\n");
	
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
	printf("Building kernel \"%s()\" (kernel %d of %d) ...\n",kernelFiles[i][1],i+1,nKernels);
	kernelPrograms[i] = clCreateProgramWithSource(context, 1, (const char **) kernelFullSource+i, NULL, &clErr);
	openCL(clErr);
	printf("  Compiling ... ");
	
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
	
	printf("Successfully compiled kernel \"%s()\", work group size=%lu, local memory size=%llu, private memory size=%llu, preferred work group size multiple=%lu\n",
		kernelFiles[i][1],kernelWGS[i],kernelLMS[i],kernelPMS[i],kernelPWM[i]);
	
	if (kernelWGS[i] < min_kernel_ws) min_kernel_ws = kernelWGS[i];
	};
printf("\n");

log_min_kernel_ws = 0;
while ((1<<log_min_kernel_ws) < min_kernel_ws) log_min_kernel_ws++;
if ((1<<log_min_kernel_ws) > min_kernel_ws) log_min_kernel_ws--;
min_kernel_ws = (1<<log_min_kernel_ws);

//	Allocate memory and load data

printf("Allocating memory for constant tables in GPU ...\n");
gpu_perms = clCreateBuffer(context, CL_MEM_READ_ONLY, permsSizeBytes, NULL, &clErr);
openCL(clErr);
gpu_mperm_res0 = clCreateBuffer(context, CL_MEM_READ_ONLY, mpermSizeBytes, NULL, &clErr);
openCL(clErr);

openCL( clEnqueueWriteBuffer(commands, gpu_perms, CL_TRUE, 0, permsSizeBytes, perms, 0, NULL, PE) );
RECORD_TIME(writeToGPUTime)
openCL( clEnqueueWriteBuffer(commands, gpu_mperm_res0, CL_TRUE, 0, mpermSizeBytes, mperm_res0, 0, NULL, PE) );
RECORD_TIME(writeToGPUTime)

printf("  Constant tables successfully loaded into GPU\n\n");

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

printf("Aim to run %d x %d = %d threads\n\n",max_groups,max_local_ws,max_global_ws);

nStringsPerHeap = max_global_ws;

//	Host memory

CHECK_MEM( host_blockSum = (cl_uint *)malloc(max_groups*sizeof(cl_uint)) )

CHECK_MEM( host_inputIndices = (cl_uint *)malloc(nStringsPerHeap*sizeof(cl_uint)) )
for (int k=0;k<nStringsPerHeap;k++) host_inputIndices[k] = k;

CHECK_MEM( host_maxPerms = (struct maxPermsLoc *)malloc(max_groups*sizeof(struct maxPermsLoc)) )

CHECK_MEM( host_nodesSearched = (cl_ulong *)malloc(max_groups*sizeof(cl_ulong)) )

printf("Trying to allocate memory in the GPU ...\n");

//	GPU memory

for (int i=0;i<NUM_GPU_HEAPS;i++)
	{
	gpu_heaps[i] = clCreateBuffer(context, CL_MEM_READ_WRITE, nStringsPerHeap*sizeof(struct string), NULL, &clErr);
	openCL(clErr);
	gpuAlloc += nStringsPerHeap*sizeof(struct string);
	};
	
gpu_inputIndices = clCreateBuffer(context, CL_MEM_READ_WRITE, nStringsPerHeap*sizeof(cl_uint), NULL, &clErr);
openCL(clErr);
gpuAlloc += nStringsPerHeap*sizeof(cl_uint);

gpu_nodesSearched = clCreateBuffer(context, CL_MEM_READ_WRITE, max_groups*sizeof(cl_ulong), NULL, &clErr);
openCL(clErr);
gpuAlloc += max_groups*sizeof(cl_ulong);

gpu_prefixSum = clCreateBuffer(context, CL_MEM_READ_WRITE, nStringsPerHeap*sizeof(struct stringStatus), NULL, &clErr);
openCL(clErr);
gpuAlloc += nStringsPerHeap*sizeof(struct stringStatus);

gpu_blockSum = clCreateBuffer(context, CL_MEM_READ_WRITE, max_groups*sizeof(cl_uint), NULL, &clErr);
openCL(clErr);
gpuAlloc += max_groups*sizeof(cl_uint);

gpu_maxPerms = clCreateBuffer(context, CL_MEM_READ_WRITE, max_groups*sizeof(struct maxPermsLoc), NULL, &clErr);
openCL(clErr);
gpuAlloc += max_groups*sizeof(struct maxPermsLoc);

printf("Global memory allocated on GPU: %"PRIu64" Mb\n",(gpuAlloc+Mb-1)/Mb);
printf("\n");

return TRUE;
}

void cleanupOpenCL()
{
//	Free host memory

if (host_blockSum) free(host_blockSum);
if (host_inputIndices) free(host_inputIndices);
if (host_maxPerms) free(host_maxPerms);
if (host_nodesSearched) free(host_nodesSearched);

//	Free OpenCL resources

if (gpu_prefixSum) clReleaseMemObject(gpu_prefixSum);
if (gpu_blockSum) clReleaseMemObject(gpu_blockSum);
if (gpu_maxPerms) clReleaseMemObject(gpu_maxPerms);
if (gpu_nodesSearched) clReleaseMemObject(gpu_nodesSearched);
if (gpu_inputIndices) clReleaseMemObject(gpu_inputIndices);
for (int i=0;i<NUM_GPU_HEAPS;i++) if (gpu_heaps[i]) clReleaseMemObject(gpu_heaps[i]);
if (gpu_mperm_res0) clReleaseMemObject(gpu_mperm_res0);
if (gpu_perms) clReleaseMemObject(gpu_perms);
for (int i=0;i<nKernels;i++)
	{
	if (kernelPrograms && kernelPrograms[i]) clReleaseProgram(kernelPrograms[i]);
	if (kernels && kernels[i]) clReleaseKernel(kernels[i]);
	};
if (kernelPrograms) free(kernelPrograms);
if (kernels) free(kernels);
if (kernelWGS) free(kernelWGS);
if (commands) clReleaseCommandQueue(commands);
if (context) clReleaseContext(context);
}

//	Do a search on the GPU starting from a single string, for a fixed total waste, and an initial pte;
//	we report back the number of nodes searched and the maximum permutation count seen.

void searchString(struct string *br, cl_int totalWaste, cl_uint pte, cl_uint pro, cl_uint *maxPermsSeen, cl_ulong *totalNodesSearched, int verbose)
{
struct string bestString;
*maxPermsSeen = 0;

//	Initialise workgroup sizes

size_t local_ws = 1;
int nWorkGroups = 1;
size_t global_ws = 1;
cl_uint nTrueInputs = 1;

//	Initialise data in the heap with a single string

int inputHeap = 0;

openCL( clEnqueueWriteBuffer(commands, gpu_heaps[inputHeap], CL_TRUE,
			0, sizeof(struct string), br, 0, NULL, PE) );
RECORD_TIME(writeToGPUTime)

//	Initialise indices as the sequence 0,1,2,...
			
openCL( clEnqueueWriteBuffer(commands, gpu_inputIndices, CL_TRUE,
			0, global_ws*sizeof(cl_uint), host_inputIndices, 0, NULL, PE) );
RECORD_TIME(writeToGPUTime)

//	Initialise perms-to-exceed

cl_uint pte0 = pte;

cl_ulong tsp = 0;
cl_ulong nsp = 0;

int threads=0;
int runs=0;

while (TRUE)		//	Loop for repeated runs of nodeQuota
	{
	cl_ulong t0 = searchKernelTime+indexKernelTime+splitKernelTime+writeToGPUTime+readFromGPUTime;
	
	if (verbose) printf("--- totalWaste = %d, pte = %d, permutations ruled out = %d ----\n",totalWaste, pte0, pro);
	if (pte0+1 >= pro)
		{
		if (verbose) printf("  We can't exceed %d permutations (this has been ruled out) so we're done\n",pte0);
		break;				//	Can't exceed pte if next highest count has been ruled out
		};
		
	cl_uint nLocalThreads = (cl_uint) local_ws;
	cl_uint paddedBlockSize = nLocalThreads + CONFLICT_FREE_OFFSET(nLocalThreads-1);
	
	if (verbose) printf("  Running %d x %d = %d threads for %d true inputs, each searching up to %"PRIu64" nodes each\n",
		nWorkGroups, (int)local_ws, (int)global_ws, nTrueInputs, nodeQuota);
	
	//	Do a node-limited search on all current input strings

	cl_kernel sk = kernels[SEARCH_KERNEL];
/*
__kernel void search(
	__constant unsigned char *perms,			//	0: Table of permutation info for N-digit strings
	__constant unsigned short *mperm_res0,		//	1: Table of maximum number of permutations for each waste
	int totalWaste,								//	2: Total waste we are allowing in each string		
	unsigned int pte,							//	3: Permutations to exceed
	unsigned int maxPermsSeen,					//	4: Maximum permutations seen in any string (not just this search)
	unsigned long nodeQuota,					//	5: Maximum number of nodes to check before we quit
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
	)
*/
	openCL( clSetKernelArg(sk, 0, sizeof(gpu_perms), &gpu_perms) );
	openCL( clSetKernelArg(sk, 1, sizeof(gpu_mperm_res0), &gpu_mperm_res0) );
	openCL( clSetKernelArg(sk, 2, sizeof(totalWaste), &totalWaste) );
	openCL( clSetKernelArg(sk, 3, sizeof(pte0), &pte0) );
	openCL( clSetKernelArg(sk, 4, sizeof(*maxPermsSeen), maxPermsSeen) );
	openCL( clSetKernelArg(sk, 5, sizeof(nodeQuota), &nodeQuota) );
	openCL( clSetKernelArg(sk, 6, sizeof(gpu_heaps[inputHeap]), &gpu_heaps[inputHeap]) );
	openCL( clSetKernelArg(sk, 7, sizeof(gpu_inputIndices), &gpu_inputIndices) );
	openCL( clSetKernelArg(sk, 8, sizeof(gpu_heaps[1-inputHeap]), &gpu_heaps[1-inputHeap]) );
	openCL( clSetKernelArg(sk, 9, sizeof(gpu_heaps[2]), &gpu_heaps[2]) );
	openCL( clSetKernelArg(sk, 10, sizeof(cl_ulong)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 11, sizeof(gpu_nodesSearched), &gpu_nodesSearched) );
	openCL( clSetKernelArg(sk, 12, sizeof(struct stringStatus)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 13, sizeof(gpu_prefixSum), &gpu_prefixSum) );
	openCL( clSetKernelArg(sk, 14, sizeof(gpu_blockSum), &gpu_blockSum) );
	openCL( clSetKernelArg(sk, 15, sizeof(struct maxPermsLoc)*paddedBlockSize, NULL) );
	openCL( clSetKernelArg(sk, 16, sizeof(gpu_maxPerms), &gpu_maxPerms) );
	openCL( clSetKernelArg(sk, 17, sizeof(nLocalThreads), &nLocalThreads) );
	openCL( clSetKernelArg(sk, 18, sizeof(nTrueInputs), &nTrueInputs) );

	openCL( clEnqueueNDRangeKernel(commands, sk, 1, NULL, &global_ws, &local_ws, 0, NULL, PE) );
	openCL( clFinish(commands) );
	RECORD_TIME(searchKernelTime)
	
	//	Extract results of the search from GPU
	
	cl_ulong nc=0;
	cl_uint totalUnfinished=0;
	unsigned int mp=0, mpw=0;
	
	//	Counts of nodes searched (for each block / local workgroup)

	openCL( clEnqueueReadBuffer(commands, gpu_nodesSearched, CL_TRUE,
		0, sizeof(cl_ulong)*nWorkGroups, host_nodesSearched, 0, NULL, PE) );
	RECORD_TIME(readFromGPUTime)
	
	//	Details of maximum permutations achieved (for each block / local workgroup)
		
	openCL( clEnqueueReadBuffer(commands, gpu_maxPerms, CL_TRUE,
		0, sizeof(struct maxPermsLoc)*nWorkGroups, host_maxPerms, 0, NULL, PE) );
	RECORD_TIME(readFromGPUTime)
	
	//	Number of unfininished searches (for each block / local workgroup)

	openCL( clEnqueueReadBuffer(commands, gpu_blockSum, CL_TRUE,
		0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, PE) );
	RECORD_TIME(readFromGPUTime)

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

	openCL( clEnqueueWriteBuffer(commands, gpu_blockSum, CL_TRUE,
		0, sizeof(cl_uint)*nWorkGroups, host_blockSum, 0, NULL, PE) );
	RECORD_TIME(writeToGPUTime)

	if (verbose) printf("  Unfinished strings remaining = %u\n",totalUnfinished);

	*totalNodesSearched += nc;
	if (verbose) printf("  Nodes searched in this run = %"PRIu64"\n",nc);
		
	if (mp > *maxPermsSeen)
		{
		*maxPermsSeen=mp;
		if (verbose) printf("  Max permutations seen = %d\n",*maxPermsSeen);
		if (*maxPermsSeen > pte0) pte0 = *maxPermsSeen;
		
		//	Read the new best string from GPU
		
		openCL( clEnqueueReadBuffer(commands, gpu_heaps[2], CL_TRUE,
			mpw*sizeof(struct string), sizeof(struct string), &bestString, 0, NULL, PE) );
		RECORD_TIME(readFromGPUTime)
		
		//	Need to adjust length to include final digit
		
		bestString.pos++;
		};
		
	//	Quit if we have no unfinished searches
	
	if (totalUnfinished==0) break;
	
	//	Arrange for the first totalUnfinished indices in gpu_inputIndices to point
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
	openCL( clSetKernelArg(ik, 0, sizeof(gpu_prefixSum), &gpu_prefixSum) );
	openCL( clSetKernelArg(ik, 1, sizeof(gpu_blockSum), &gpu_blockSum) );
	openCL( clSetKernelArg(ik, 2, sizeof(gpu_inputIndices), &gpu_inputIndices) );
	openCL( clSetKernelArg(ik, 3, sizeof(nLocalThreads), &nLocalThreads) );
	openCL( clSetKernelArg(ik, 4, sizeof(totalUnfinished), &totalUnfinished) );

	openCL( clEnqueueNDRangeKernel(commands, ik, 1, NULL, &global_ws, &local_ws, 0, NULL, PE) );
	openCL( clFinish(commands) );
	RECORD_TIME(indexKernelTime)
	
	//	If we have any spare capacity, we split unfinished searches to make use of it.
	
	cl_uint nPreviousInputs = (cl_uint) global_ws;
	cl_uint nInputTarget = 2*totalUnfinished;
	if (nInputTarget > max_global_ws) nInputTarget = (cl_uint) max_global_ws;
	
	if (nInputTarget>totalUnfinished)
		{
		cl_kernel spk = kernels[SPLIT_KERNEL];
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
		openCL( clSetKernelArg(spk, 1, sizeof(gpu_inputIndices), &gpu_inputIndices) );
		openCL( clSetKernelArg(spk, 2, sizeof(gpu_heaps[1-inputHeap]), &gpu_heaps[1-inputHeap]) );
		openCL( clSetKernelArg(spk, 3, sizeof(nInputTarget), &nInputTarget) );
		openCL( clSetKernelArg(spk, 4, sizeof(totalUnfinished), &totalUnfinished) );
		openCL( clSetKernelArg(spk, 5, sizeof(nPreviousInputs), &nPreviousInputs) );

		openCL( clEnqueueNDRangeKernel(commands, spk, 1, NULL, &global_ws, &local_ws, 0, NULL, PE) );
		openCL( clFinish(commands) );
		RECORD_TIME(splitKernelTime)
		};
	
	//	Next search will use current output heap for input
	
	inputHeap = 1-inputHeap;
	
	//	Set the workgroup sizes
	
	global_ws = 1;
	while (global_ws < nInputTarget) global_ws*=2;
	
	if (global_ws <= max_local_ws)
		{
		local_ws = global_ws;
		nWorkGroups = 1;
		}
	else
		{
		local_ws = max_local_ws;
		nWorkGroups = (int)global_ws / local_ws;
		};
		
	nTrueInputs = (cl_uint) nInputTarget;
	cl_ulong t1 = searchKernelTime+indexKernelTime+splitKernelTime+writeToGPUTime+readFromGPUTime;
	cl_ulong dt = t1-t0;
	
	tsp+=dt;
	nsp+=nc;
	threads += global_ws;
	runs++;
	
	if (verbose) printf("Nodes searched per second of GPU time = %lld\n",(nc*1000000000L)/(dt));
	else if (tsp > 10000000000L)
		{
		printf("Nodes searched per second of GPU time = %lld, average number of threads = %.1lf\n",(nsp*1000000000L)/(tsp),((double)threads)/runs);
		tsp = 0;
		nsp = 0;
		threads = 0;
		runs = 0;
		};
	};
		
}

void searchPrefix(char *prefix, int prefixLen, int waste1, int waste2, int verbose)
{
//	Set up initial string structure, based on prefix.

struct string br;
for (int k=0;k<MSL;k++) br.digits[k]=0;
br.perms=0;
unsigned short lastN=0;
unsigned char d=0;
for (int k=0;k<prefixLen;k++)
	{
	d = prefix[k]-'1';
	lastN = (lastN/NVAL) + NN1*d;
	unsigned char P = perms[lastN];
	unsigned short pNum = (P>=FNM) ? FN : (d*FNM+P);
	
	//	Count any new permutation we visit
	
	if (P<FNM && (br.digits[pNum] & PERMUTATION_BITS)==0)
		{
		br.perms++;
		};
	
	//	Set the flag, either for permutation or "no permutation"
	
	br.digits[pNum] = PERMUTATION_LSB;
	
	//	Store the last N digits in the string.
	
	int j = k + NVAL - prefixLen;
	if (j >= 0)
		{
		br.digits[j] |= d;
		};
	};

br.lastN1 = lastN / NVAL;
br.waste=prefixLen - (NVAL-1) - br.perms;

//	Initialise first character after the N from the prefix to be the cyclic successor to the last digit of the prefix.

br.digits[NVAL] |= (d+1)%NVAL;
br.pos = NVAL;
br.rootDepth = NVAL;

/*
printf("Input string: \n");
print_string_data(stdout, &br, prefix, prefixLen, TRUE, FALSE, TRUE);
printf("\n");
*/

cl_uint maxPermsSeen=0;
totalNodesSearched=0;
cl_int totalWaste=waste1;
cl_uint pte=mperm_res[totalWaste-1]+2*(NVAL-4);
while (totalWaste <= waste2)							//	Loop for values of waste
	{
	cl_uint pro = mperm_res[totalWaste-1]+NVAL+1;	//	Permutations previously ruled out for this waste
	if (pro > FN+1) pro = FN+1;
	
	while (TRUE)									//	Loop for values of pte, which might include backtracking if we aim too high
		{
		searchString(&br, totalWaste, pte, pro, &maxPermsSeen, &totalNodesSearched, verbose);
		
		if (maxPermsSeen > pte) break;				//	We hit our target for perms to exceed
		
		pro = pte+1;								//	We ruled out a permutation count
		printf("For totalWaste=%d, RULED OUT %d permutations\n",totalWaste,pro);
		pte--;										//	Lower our target and repeat
		};
	
	//	Verify value against known results
	
	int matchOK = TRUE;
	if (knownN && totalWaste < numKnownW && maxPermsSeen != knownN[2*totalWaste+1])
		{
		printf("Mismatch between maxPermsSeen=%d found by program and known result %d\n",maxPermsSeen,knownN[2*totalWaste+1]);
		matchOK = FALSE;
		}
	else printf("For totalWaste=%d, maxPermsSeen=%d\n",totalWaste,maxPermsSeen);

	if (!matchOK) exit(EXIT_FAILURE);
	
	//	Update mperm_res, both in host and in GPU
	
	mperm_res[totalWaste] = maxPermsSeen;
	openCL( clEnqueueWriteBuffer(commands, gpu_mperm_res0, CL_TRUE,
		(MPERM_OFFSET+totalWaste)*sizeof(*mperm_res0), sizeof(*mperm_res0), mperm_res+totalWaste, 0, NULL, PE) );
	RECORD_TIME(writeToGPUTime)
		
	if (maxPermsSeen==FN) break;
	
	pte = maxPermsSeen + 2*(NVAL-4);
	if (pte >= FN) pte = FN-1;
	totalWaste++;
	};
	
printf("Total nodes searched = %"PRIu64"\n",totalNodesSearched);
}

void initForN()
{
if (NVAL==3) {knownN = &known3[0][0]; numKnownW=sizeof(known3)/(sizeof(int))/2;}
else if (NVAL==4) {knownN = &known4[0][0]; numKnownW=sizeof(known4)/(sizeof(int))/2;}
else if (NVAL==5) {knownN = &known5[0][0]; numKnownW=sizeof(known5)/(sizeof(int))/2;}
else if (NVAL==6) {knownN = &known6[0][0]; numKnownW=sizeof(known6)/(sizeof(int))/2;}
else knownN = NULL;
}

int main (int argc, const char * argv[])
{
initForN();

//	Check for a choice of GPU

const char *gpuChoice = NULL;

for (int i=1;i<argc;i++)
	{
	if (strcmp(argv[i],"gpuName")==0 && i+1<argc) gpuChoice = argv[i+1];
	};

initOpenCL(gpuChoice);

printf("Starting validation checks\n");
searchPrefix("123456",NVAL,1,80,VERBOSE);
printf("\n");

printf("Starting benchmarking calculations\n");
searchPrefix("123456123451623451263451236451234651324651342651346251346521345621345261345216345213645231645236142536124536214532",
strlen("123456123451623451263451236451234651324651342651346251346521345621345261345216345213645231645236142536124536214532"),118,118,
	VERBOSE);

cleanupOpenCL();

#if PROFILE

cl_ulong totalGPUTime = 0;

#define fmt "%40s = %12lld\n"

#define PTIME(t) printf(fmt,#t,t); totalGPUTime+=t;

PTIME(searchKernelTime)
PTIME(indexKernelTime)
PTIME(splitKernelTime)
PTIME(writeToGPUTime)
PTIME(readFromGPUTime)

printf("\n"fmt,"totalGPUTime",totalGPUTime);
printf("\nNodes searched per second overall = %lld\n",(totalNodesSearched*1000000000L)/(totalGPUTime));

#endif
return 0;
}
