#include <inttypes.h>
#include <sys/time.h>
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
	struct timeval tv;
	char buf[1024];
	char tbuf[20];
	int len;

	if (level < level_) {
		gettimeofday(&tv, NULL);
		strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&tv.tv_sec));
		len = sprintf(buf, "[ %s.%03" PRIdMAX " ] ", tbuf, static_cast<intmax_t>(tv.tv_usec / 1000));

		va_start(argp, fmt);
		vsnprintf(&buf[len], sizeof(buf) - len, fmt, argp);
		va_end(argp);
		fprintf(fp_, "%s", buf);
	}
}
