#include <config.h>

#include <string.h>

#include "global.h"


extern char **environ;

int env_debug()
{
#ifdef DEBUG
	return 1;
#else
	int i;
	for (i = 0; environ[i] != NULL; i++) {
		if (strcmp(environ[i], "DEBUG") == '=')
			return 1;
	}
	return 0;
#endif
}
