#include <cstdio>
#include <cstdarg>
#include "Debug.hpp"

Debug::Debug() :
	file_(""), level_(0),	fp_(stdout)
{
}
	
Debug::~Debug()
{
	if (fp_ && fp_ != stdout) {
		fclose(fp_);
	}
}

void Debug::set_level(int level)
{
	level_ = level;
}

FILE* Debug::set_file(char* file)
{
	FILE* fp = fopen(file, "a");
	if (!fp) {
		return NULL;
	}
	fp_ = fp;
	return fp_;
}

void Debug::print(int level, const char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	// if (level < level_) {
		vfprintf(fp_, fmt, argp);
	// }
	va_end(argp);
}

