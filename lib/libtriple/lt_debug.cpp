/* libtriple debug functions */

#include <stdarg.h>
#include <stdio.h>

int cnxt_debug = 0;

void lt_debug(const char *fmt, ...)
{
	if (! cnxt_debug)
		return;

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

