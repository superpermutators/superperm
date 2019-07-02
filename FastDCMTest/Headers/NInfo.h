//  NInfo.h
//
//	Define:
//
//	NVAL = N, the number of digits we are working with.
//	FN = N!
//	FNM = (N-1)!
//	NN1 = N^(N-1)
//	MSL = upper bound on minimum superperm length
//	MAX_WASTE_VALS = Maximum # of waste values to consider (one plus maximum waste, allowing for 0)

/*
#define NVAL 5
#define FN 120
#define FNM 24
#define NN1 625
#define MSL 153
#define MAX_WASTE_VALS 30
*/


#define NVAL 6
#define FN 720
#define FNM 120
#define NN1 7776
#define MSL 872
#define MAX_WASTE_VALS 148


//	Offset into mperm_res0 table where mperm_res starts, to allow for negative indices to catch edge cases

#define MPERM_OFFSET (2*NVAL)

//	Offet into digits[] for unvisited permutations in 1-cycles

#define OCP_OFFSET (FN+1)
