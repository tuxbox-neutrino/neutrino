/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2015 CoolStream International Ltd

        print_stacktrace function taken from:
        stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
        published under the WTFPL v2.0

        License: GPLv2

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation;

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define __USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <cxxabi.h>
#include <signal.h>
#include <ucontext.h>

#ifdef HAVE_BACKTRACE
#include <execinfo.h>

/** Print a demangled stack backtrace of the caller function to FILE* out. */
void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63)
{
    fprintf(out, "stack trace:\n");

    // storage array for stack trace address data
    void* addrlist[max_frames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

    if (addrlen == 0) {
	fprintf(out, "  <empty, possibly corrupt>\n");
	return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);

    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; i++)
    {
	char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

	// find parentheses and +address offset surrounding the mangled name:
	// ./module(function+0x15c) [0x8048a6d]
	for (char *p = symbollist[i]; *p; ++p)
	{
	    if (*p == '(')
		begin_name = p;
	    else if (*p == '+')
		begin_offset = p;
	    else if (*p == ')' && begin_offset) {
		end_offset = p;
		break;
	    }
	}

	if (begin_name && begin_offset && end_offset
	    && begin_name < begin_offset)
	{
	    *begin_name++ = '\0';
	    *begin_offset++ = '\0';
	    *end_offset = '\0';

	    // mangled name is now in [begin_name, begin_offset) and caller
	    // offset in [begin_offset, end_offset). now apply
	    // __cxa_demangle():

	    int status;
	    char* ret = abi::__cxa_demangle(begin_name,
					    funcname, &funcnamesize, &status);
	    if (status == 0) {
		funcname = ret; // use possibly realloc()-ed string
		fprintf(out, "  %s : %s+%s [%p]\n",
			symbollist[i], funcname, begin_offset, addrlist[i]);
	    }
	    else {
		// demangling failed. Output function name as a C function with
		// no arguments.
		fprintf(out, "  %s : %s()+%s [%p]\n",
			symbollist[i], begin_name, begin_offset, addrlist[i]);
	    }
	}
	else
	{
	    // couldn't parse the line? print the whole line.
	    fprintf(out, "  %s\n", symbollist[i]);
	}
    }

    free(funcname);
    free(symbollist);
}

static void crash_handler(int signum, siginfo_t * si, void *ctx)
{
	void *trace[16];
	int trace_size = 0;
	ucontext_t *ct = (ucontext_t *) ctx;
	if (si) {}
#if 0
	printf("signal %d, siginfo->si_addr is %p\n", signum, si->si_addr);
	printf("reg[%02d]       = 0x%lx\n",0 , ct->uc_mcontext.arm_r0);
	printf("reg[%02d]       = 0x%lx\n",1 , ct->uc_mcontext.arm_r1);
	printf("reg[%02d]       = 0x%lx\n",2 , ct->uc_mcontext.arm_r2);
	printf("reg[%02d]       = 0x%lx\n",3 , ct->uc_mcontext.arm_r3);
	printf("reg[%02d]       = 0x%lx\n",4 , ct->uc_mcontext.arm_r4);
	printf("reg[%02d]       = 0x%lx\n",5 , ct->uc_mcontext.arm_r5);
	printf("reg[%02d]       = 0x%lx\n",6 , ct->uc_mcontext.arm_r6);
	printf("reg[%02d]       = 0x%lx\n",7 , ct->uc_mcontext.arm_r7);
	printf("reg[%02d]       = 0x%lx\n",8 , ct->uc_mcontext.arm_r8);
	printf("reg[%02d]       = 0x%lx\n",9 , ct->uc_mcontext.arm_r9);
	printf("reg[%02d]       = 0x%lx\n",10 , ct->uc_mcontext.arm_r10);
	printf("FP       = 0x%lx\n", ct->uc_mcontext.arm_fp);
	printf("IP       = 0x%lx\n", ct->uc_mcontext.arm_ip);
	printf("SP       = 0x%lx\n", ct->uc_mcontext.arm_sp);
	printf("LR       = 0x%lx\n", ct->uc_mcontext.arm_lr);
	printf("PC       = 0x%lx\n", ct->uc_mcontext.arm_pc);
	printf("CPSR       = 0x%lx\n", ct->uc_mcontext.arm_cpsr);
	printf("Fault Address       = 0x%lx\n", ct->uc_mcontext.fault_address);
	printf("Trap no       = 0x%lx\n", ct->uc_mcontext.trap_no);
	printf("Err Code       = 0x%lx\n", ct->uc_mcontext.error_code);
	printf("Old Mask       = 0x%lx\n", ct->uc_mcontext.oldmask);
#endif

	trace_size = backtrace(trace, 16);
	trace[1] = (void *) ct->uc_mcontext.arm_pc;
#if 0
	char **messages = backtrace_symbols(trace, trace_size);
	/* skip first stack frame (points here) */
	printf("Execution path: (%d)\n", trace_size);
	for (int i = 0; i < trace_size; ++i)
		printf("[#%d] %s\n", i, messages[i]);
#endif
	backtrace_symbols_fd(trace, trace_size, fileno(stdout));
	if (signum != SIGUSR1)
		abort();
}

void install_crash_handler()
{
	struct sigaction sa;

	sa.sa_sigaction = crash_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	/* for testing */
	sigaction(SIGUSR1, &sa, NULL);

	void *trace[16];
	backtrace(trace, 16);
}
#else
void print_stacktrace(FILE *out = stderr, unsigned int max_frames = 63)
{
	(void) out;
	(void) max_frames;
}

void install_crash_handler()
{
}
#endif
