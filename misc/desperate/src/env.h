#include <stdio.h>

#ifndef GLOBAL_H
#define GLOBAL_H

#define S(X) _S(X)
#define _S(X) #X


int env_debug(void);

#ifdef DEBUG
#define PRINT_DEBUG(...) {if (env_debug()) {\
	fprintf(stderr, S(EXECNAME) ": [DEBUG] ");\
	fprintf(stderr, "in " S(__FILE__) ":" S(__LINE__) " %s(): ", __func__);\
	fprintf(stderr, __VA_ARGS__);}}
#else
#define PRINT_DEBUG(...) /* debug statement removed */
#endif

#define PRINT_ERROR(...) {\
	fprintf(stderr, S(EXECNAME) ": [ERROR] ");\
	if (env_debug()) {fprintf(stderr, "in " S(__FILE__) ":" \
				  S(__LINE__) " %s(): ", __func__);}\
	fprintf(stderr, __VA_ARGS__);}

#define PRINT_INFO(...) {\
	fprintf(stdout, S(EXECNAME) ": [INFO] ");\
	if (env_debug()) {fprintf(stdout, "in " S(__FILE__) ":" \
				  S(__LINE__) " %s(): ", __func__);}\
	fprintf(stdout, __VA_ARGS__);}

#define MESSAGE(STR) fprintf(stdout, "%s: %s\n", S(EXECNAME), STR)

#define FMESSAGE(...) {\
	fprintf(stdout, S(EXECNAME) ": ");\
	fprintf(stdout, __VA_ARGS__);}

#endif /* GLOBAL_H */
