/*
 * helper functions to interact with files, usually in the proc filesystem
 *
 * (C) 2012 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
 *
 * License: GPLv2 or later
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cctype>	/* isspace */
#include <cstdio>	/* sscanf */
#include <cstring>

#include <system/helpers.h>

#include "proc_tools.h"

int proc_put(const char *path, const char *value, const int len)
{
	int ret, ret2;
	int pfd = open(path, O_WRONLY);
	if (pfd < 0)
		return pfd;
	ret = write(pfd, value, len);
	ret2 = close(pfd);
	if (ret2 < 0)
		return ret2;
	return ret;
}

int proc_put(const char *path, const char *value)
{
	return proc_put(path, value, strlen(value));
}

int proc_put(const char *path, std::string value)
{
	return proc_put(path, value.c_str());
}

int proc_put(const char *path, int value)
{
	return proc_put(path, to_string(value).c_str());
}

int proc_put(const char *path, unsigned int value)
{
	return proc_put(path, to_string(value).c_str());
}

int proc_put(const char *path, long value)
{
	return proc_put(path, to_string(value).c_str());
}

int proc_put(const char *path, unsigned long value)
{
	return proc_put(path, to_string(value).c_str());
}

int proc_put(const char *path, long long value)
{
	return proc_put(path, to_string(value).c_str());
}

int proc_put(const char *path, unsigned long long value)
{
	return proc_put(path, to_string(value).c_str());
}

int proc_put(const char *path, bool state)
{
	return proc_put(path, state ? "1" : "0", 1);
}

int proc_get(const char *path, char *value, const int len)
{
	int ret, ret2;
	int pfd = open(path, O_RDONLY);
	if (pfd < 0)
		return pfd;
	ret = read(pfd, value, len);
	value[len-1] = '\0'; /* make sure string is terminated */
	if (ret >= 0)
	{
		while (ret > 0 && isspace(value[ret-1]))
			ret--;		/* remove trailing whitespace */
		value[ret] = '\0';	/* terminate, even if ret = 0 */
	}
	ret2 = close(pfd);
	if (ret2 < 0)
		return ret2;
	return ret;
}

unsigned int proc_get_hex(const char *path)
{
	unsigned int n, ret = 0;
	char buf[16];
	n = proc_get(path, buf, 16);
	if (n > 0)
		sscanf(buf, "%x", &ret);
	return ret;
}
