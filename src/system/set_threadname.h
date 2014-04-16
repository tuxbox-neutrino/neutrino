#ifndef __set_threadname_h__
#define __set_threadname_h__
#include <sys/types.h>
#include <sys/prctl.h>
#include <string.h>

inline void set_threadname(const char *name)
{
	char threadname[17];
	strncpy(threadname, name, sizeof(threadname));
	threadname[16] = 0;
	prctl (PR_SET_NAME, (unsigned long)threadname);
}
#endif
