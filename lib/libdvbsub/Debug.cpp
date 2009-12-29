#include <sys/timeb.h>
#include <time.h>
#include <cstdio>
#include <cstdarg>
#include "Debug.hpp"

Debug::Debug() :
	file_(const_cast<char *>("")), level_(0),	fp_(stdout)
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
	struct timeb tp;
	char buf[1024];
	char tbuf[20];
	int len;
	struct tm tv;

	if (level < level_) {
		ftime(&tp);
		localtime_r (&tp.time, &tv);
		strftime (tbuf, 14, "%H:%M:%S", &tv);
		len = sprintf(buf, "[ %s.%03d ] ", tbuf, tp.millitm);

		va_start(argp, fmt);
		//vfprintf(fp_, fmt, argp);
		vsnprintf (&buf[len], 512, fmt, argp);
		va_end(argp);
		fprintf(fp_, "%s", buf);
	}
}

