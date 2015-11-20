#ifndef _STACKTRACE_H_
#define _STACKTRACE_H_

void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63);
void install_crash_handler();

#endif
