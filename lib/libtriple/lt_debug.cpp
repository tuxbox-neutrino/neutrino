/* libtriple debug functions */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int cnxt_debug = 0; /* compat, unused */

int debuglevel = -1;

static const char* lt_facility[] = {
	"audio ",
	"video ",
	"demux ",
	"play  ",
	"power ",
	"init  ",
	"ca    ",
	"record",
	NULL
};

void _lt_info(int facility, const void *func, const char *fmt, ...)
{
	/* %p does print "(nil)" instead of 0x00000000 for NULL */
	fprintf(stderr, "[LT:%08lx:%s] ", (long) func, lt_facility[facility]);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}


void _lt_debug(int facility, const void *func, const char *fmt, ...)
{
	if (debuglevel < 0)
		fprintf(stderr, "lt_debug: debuglevel not initialized!\n");

	if (! ((1 << facility) & debuglevel))
		return;

	fprintf(stderr, "[LT:%08lx:%s] ", (long)func, lt_facility[facility]);
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void lt_debug_init(void)
{
	int i = 0;
	char *tmp = getenv("TRIPLE_DEBUG");
	if (! tmp)
		debuglevel = 0;
	else
		debuglevel = (int) strtol(tmp, NULL, 0);

	if (debuglevel == 0)
	{
		fprintf(stderr, "libtriple debug options can be set by exporting TRIPLE_DEBUG.\n");
		fprintf(stderr, "The following values (or bitwise OR combinations) are valid:\n");
		while (lt_facility[i]) {
			fprintf(stderr, "\tcomponent: %s  0x%02x\n", lt_facility[i], 1 << i);
			i++;
		}
		fprintf(stderr, "\tall components:    0x%02x\n", (1 << i) - 1);
	} else {
		fprintf(stderr, "libtriple debug is active for the following components:\n");
		while (lt_facility[i]) {
			if (debuglevel & (1 << i))
				fprintf(stderr, "%s ", lt_facility[i]);
			i++;
		}
		fprintf(stderr, "\n");
	}
}
