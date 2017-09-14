/*
	Neutrino-HD

	License: GPL

	(C) 2012-2013 the neutrino-hd developers
	(C) 2012-2017 Stefan Seyfried

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>	/* forkpty*/
#include <sys/ioctl.h>
#include <inttypes.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <sys/time.h>	/* gettimeofday */
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <algorithm>
#include <mntent.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#include "debug.h"
#include <global.h>
#include <driver/fontrenderer.h>
//#include <driver/framebuffer.h>
#include <system/helpers.h>
#include <gui/update_ext.h>
#include <libmd5sum.h>
#define MD5_DIGEST_LENGTH 16
using namespace std;

int mySleep(int sec) {
	struct timeval timeout;

	timeout.tv_sec = sec;
	timeout.tv_usec = 0;
	return select(0,0,0,0, &timeout);
}

off_t file_size(const char *filename)
{
	struct stat stat_buf;
	if(::stat(filename, &stat_buf) == 0)
	{
		return stat_buf.st_size;
	} else
	{
		return 0;
	}
}

bool file_exists(const char *filename)
{
	struct stat stat_buf;
	if(::stat(filename, &stat_buf) == 0)
	{
		return true;
	} else
	{
		return false;
	}
}

void  wakeup_hdd(const char *hdd_dir)
{
	if(!check_dir(hdd_dir) && hdd_get_standby(hdd_dir)){
		std::string wakeup_file = hdd_dir;
		wakeup_file += "/.wakeup";
		int fd = open(wakeup_file.c_str(), O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR);
		if (fd >= 0) {
			unsigned char buf[512];
			memset(buf, 0xFF, sizeof(buf));
			for (int i = 0; i < 20; i++) {
				if (write(fd, buf, sizeof(buf)) < 0) {
					perror("write to .wakeup");
					break;
				}
			}
			fdatasync(fd);
			close(fd);
		}
		hdd_flush(hdd_dir);
		remove(wakeup_file.c_str());
	}
}
//use for script with full path
int my_system(const char * cmd)
{
	if (!file_exists(cmd))
		return -1;

	return my_system(1, cmd);
}

int my_system(int argc, const char *arg, ...)
{
	int i = 0, ret, childExit = 0;
#define ARGV_MAX 64
	/* static right now but could be made dynamic if necessary */
	int argv_max = ARGV_MAX;
	const char *argv[ARGV_MAX];
	va_list args;
	argv[0] = arg;
	va_start(args, arg);

	while(++i < argc)
	{
		if (i == argv_max)
		{
			fprintf(stderr, "my_system: too many arguments!\n");
			va_end(args);
			return -1;
		}
		argv[i] = va_arg(args, const char *);
	}
	argv[i] = NULL; /* sentinel */
	//fprintf(stderr,"%s:", __func__);for(i=0;argv[i];i++)fprintf(stderr," '%s'",argv[i]);fprintf(stderr,"\n");

	pid_t pid;
	int maxfd = getdtablesize();// sysconf(_SC_OPEN_MAX);
	switch (pid = vfork())
	{
		case -1: /* can't vfork */
			perror("vfork");
			ret = -errno;
			break;
		case 0: /* child process */
			ret = 0;
			for(i = 3; i < maxfd; i++)
				close(i);
			if (setsid() == -1)
				perror("my_system setsid");
			if (execvp(argv[0], (char * const *)argv))
			{
				ret = -errno;
				if (errno != ENOENT) /* don't complain if argv[0] only does not exist */
					fprintf(stderr, "ERROR: my_system \"%s\": %m\n", argv[0]);
			}
			_exit(ret); // terminate c h i l d proces s only
		default: /* parent returns to calling process */
			ret = 0;
			waitpid(pid, &childExit, 0);
			if (WEXITSTATUS(childExit) != 0)
				ret = (signed char)WEXITSTATUS(childExit);
			break;
	}
	va_end(args);
	return ret;
}

FILE* my_popen( pid_t& pid, const char *cmdstring, const char *type)
{
	int     pfd[2] ={-1,-1};
	FILE    *fp = NULL;

	/* only allow "r" or "w" */
	if ((type[0] != 'r' && type[0] != 'w') || type[1] != 0) {
		errno = EINVAL;     /* required by POSIX */
		return(NULL);
	}

	if (pipe(pfd) < 0)
		return(NULL);   /* errno set by pipe() */

	if ((pid = vfork()) < 0) {
		return(NULL);   /* errno set by vfork() */
	} else if (pid == 0) {                           /* child */
		if (*type == 'r') {
			close(pfd[0]);
			if (pfd[1] != STDOUT_FILENO) {
				dup2(pfd[1], STDOUT_FILENO);
				close(pfd[1]);
			}
		} else {
			close(pfd[1]);
			if (pfd[0] != STDIN_FILENO) {
				dup2(pfd[0], STDIN_FILENO);
				close(pfd[0]);
			}
		}
		int maxfd = getdtablesize();
		for(int i = 3; i < maxfd; i++)
			close(i);
		if (setsid() == -1)
			perror("my_popen setsid");
		execl("/bin/sh", "sh", "-c", cmdstring, (char *)0);
		exit(0);
	 }

	/* parent continues... */
	if (*type == 'r') {
		close(pfd[1]);
		if ((fp = fdopen(pfd[0], type)) == NULL)
			return(NULL);
	} else {
		close(pfd[0]);
		if ((fp = fdopen(pfd[1], type)) == NULL)
			return(NULL);
	}
	return(fp);
}

int run_pty(pid_t &pid, const char *cmdstring)
{
	int master = -1;
	if ((pid = forkpty(&master, NULL, NULL, NULL)) < 0)
		return -1;
	else if (pid == 0) {
		int maxfd = getdtablesize();
		for(int i = 3; i < maxfd; i++)
			close(i);
		execl("/bin/sh", "sh", "-c", cmdstring, (char *)0);
		exit(0);
	}
	return master;
}

#if 0
int mkdirhier(const char *pathname, mode_t mode)
{
	int res = -1;
	if (!pathname || !*pathname)
		return res;
	char path[strlen(pathname) + 1];
	strcpy(path, pathname);
	char *p = path;
	while ((p = strchr(p + 1, '/'))) {
		*p = 0;
		res = mkdir(path, mode);
		if (res < 0 && errno != EEXIST)
			break;
		*p = '/';
	}
	res = mkdir(path, mode);
	if (errno == EEXIST)
		res = 0;
	return res;
}
# endif

int safe_mkdir(const char * path)
{
	struct statfs s;
	size_t l = strlen(path);
	char d[l + 3];
	strncpy(d, path, l);

	// skip trailing slashes
	while (l > 0 && d[l - 1] == '/')
		l--;
	// find last slash
	while (l > 0 && d[l - 1] != '/')
		l--;
	if (!l)
		return -1;
	// append a single dot
	d[l++] = '.';
	d[l] = 0;

	if(statfs(d, &s) || (s.f_type == 0x72b6 /* jffs2 */))
		return -1;
	return mkdir(path, 0755);
}

/* function used to check is this dir writable, i.e. not flash, for record etc */
int check_dir(const char * dir, bool allow_tmp)
{
	/* default to return, if statfs fail */
	int ret = -1;
	struct statfs s;
	if (::statfs(dir, &s) == 0) {
		switch (s.f_type) {
			case 0x858458f6L: 	// ramfs
			case 0x1021994L: 	// tmpfs
				if(allow_tmp)
					ret = 0;//ok
			case 0x72b6L:		// jffs2
				break;
			default:
				ret = 0;	// ok
		}
		if(ret == -1)
			printf("Wrong Filessystem Type: 0x%llx\n", (unsigned long long)s.f_type);
	}
	return ret;
}

bool get_fs_usage(const char * dir, uint64_t &btotal, uint64_t &bused, long *bsize/*=NULL*/)
{
	btotal = bused = 0;
	struct statfs s;

	if (::statfs(dir, &s) == 0 && s.f_blocks) {
		btotal = s.f_blocks;
		bused = s.f_blocks - s.f_bfree;
		if (bsize != NULL)
			*bsize = s.f_bsize;
		//printf("fs (%s): total %llu used %llu\n", dir, btotal, bused);
		return true;
	}
	return false;
}

bool get_mem_usage(unsigned long &kbtotal, unsigned long &kbfree)
{
	unsigned long cached = 0, buffers = 0;
	kbtotal = kbfree = 0;

	FILE * f = fopen("/proc/meminfo", "r");
	if (!f)
		return false;

	char buffer[256];
	while (fgets(buffer, 255, f)) {
		if (!strncmp(buffer, "Mem", 3)) {
			if (!strncmp(buffer+3, "Total", 5))
				kbtotal = strtoul(buffer+9, NULL, 10);
			else if (!strncmp(buffer+3, "Free", 4))
				kbfree = strtoul(buffer+8, NULL, 10);
		}
		else if (!strncmp(buffer, "Buffers", 7)) {
			buffers = strtoul(buffer+8, NULL, 10);
		}
		else if (!strncmp(buffer, "Cached", 6)) {
			cached = strtoul(buffer+7, NULL, 10);
			break;
		}
	}
	fclose(f);
	kbfree = kbfree + cached + buffers;
	printf("mem: total %ld cached %ld free %ld\n", kbtotal, cached, kbfree);
	return true;
}

std::string find_executable(const char *name)
{
	struct stat s;
	char *tmpPath = getenv("PATH");
	char *p, *n, *path;
	if (tmpPath)
		path = strdupa(tmpPath);
	else
		path = strdupa("/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin:/usr/local/sbin");
	if (name[0] == '/') { /* full path given */
		if (!access(name, X_OK) && !stat(name, &s) && S_ISREG(s.st_mode))
			return std::string(name);
		return "";
	}

	p = path;
	while (p) {
		n = strchr(p, ':');
		if (n)
			*n++ = '\0';
		if (*p != '\0') {
			std::string tmp = std::string(p) + "/" + std::string(name);
			const char *f = tmp.c_str();
			if (!access(f, X_OK) && !stat(f, &s) && S_ISREG(s.st_mode))
				return tmp;
		}
		p = n;
	}
	return "";
}

std::string backtick(std::string command)
{
	char *buf = NULL;
	size_t n = 0;
	pid_t pid;
	FILE *p = my_popen(pid, command.c_str(), "r");
	if (! p)
		return "";
	std::string out = "";
	while (getline(&buf, &n, p) >= 0)
		out.append(std::string(buf));
	free(buf);
	fclose(p);
	waitpid(pid, NULL, 0);
	return out;
}

std::string _getPathName(std::string &path, std::string sep)
{
	size_t pos = path.find_last_of(sep);
	if (pos == std::string::npos)
		return path;
	return path.substr(0, pos);
}

std::string _getBaseName(std::string &path, std::string sep)
{
	size_t pos = path.find_last_of(sep);
	if (pos == std::string::npos)
		return path;
	if (path.length() == pos +1)
		return "";
	return path.substr(pos+1);
}

std::string getPathName(std::string &path)
{
	return _getPathName(path, "/");
}

std::string getBaseName(std::string &path)
{
	return _getBaseName(path, "/");
}

std::string getFileName(std::string &file)
{
	return _getPathName(file, ".");
}

std::string getFileExt(std::string &file)
{
	return _getBaseName(file, ".");
}


std::string getNowTimeStr(const char* format)
{
	char tmpStr[256];
	struct timeval tv;
	struct tm t;
	gettimeofday(&tv, NULL);        
	strftime(tmpStr, sizeof(tmpStr), format, localtime_r(&tv.tv_sec, &t));
	return (std::string)tmpStr;
}

std::string trim(std::string &str, const std::string &trimChars /*= " \n\r\t"*/)
{
	std::string result = str.erase(str.find_last_not_of(trimChars) + 1);
	return result.erase(0, result.find_first_not_of(trimChars));
}

std::string cutString(const std::string str, int msgFont, const int width)
{
	Font *msgFont_ = g_Font[msgFont];
	std::string ret = str;
	ret = trim(ret);
	int sw = msgFont_->getRenderWidth(ret);
	if (sw <= width)
		return ret;
	else {
		std::string z = "...";
		int zw = msgFont_->getRenderWidth(z);
		if (width <= 2*zw)
			return ret;
		do {
			ret = ret.substr(0, ret.length()-1);
			sw = msgFont_->getRenderWidth(ret);
		} while (sw+zw > width);
		ret = trim(ret) + z;
	}
	return ret;
}

std::string strftime(const char *format, const struct tm *tm)
{
	char buf[4096];
	*buf = 0;
	strftime(buf, sizeof(buf), format, tm);
	return std::string(buf);
}

std::string strftime(const char *format, time_t when, bool gm)
{
	struct tm *t = gm ? gmtime(&when) : localtime(&when);
	return strftime(format, t);
}

time_t toEpoch(std::string &date)
{
	struct tm t;
	memset(&t, 0, sizeof(t));
	char *p = strptime(date.c_str(), "%Y-%m-%d", &t);
	if(p)
		return mktime(&t);

	return 0;

}

std::string& str_replace(const std::string &search, const std::string &replace, std::string &text)
{
	if (search.empty() || text.empty())
		return text;

	size_t searchLen = search.length();
	while (1) {
		size_t pos = text.find(search);
		if (pos == std::string::npos)
			break;
		text.replace(pos, searchLen, replace);
	}
	return text;
}

/*
 * ported from:
 * https://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c
 *
 * You must delete the result if result is non-NULL
 */
const char *cstr_replace(const char *search, const char *replace, const char *text)
{
	const char *result;	// the return string
	const char *ins;	// the next insert point
	char *tmp;		// varies
	int len_search;		// length of search (the string to remove)
	int len_replace;	// length of replace (the string to replace search with)
	int len_front;		// distance between search and end of last search
	int count;		// number of replacements

	// sanity checks and initialization
	if (!text || !search)
		return NULL;
	len_search = strlen(search);
	if (len_search == 0)
		return NULL; // empty search causes infinite loop during count
	if (!replace)
		replace = "";
	len_replace = strlen(replace);

	// count the number of replacements needed
	ins = text;
	for (count = 0; (tmp = (char*)strstr(ins, search)); ++count)
		ins = tmp + len_search;

	int len_tmp = strlen(text) + (len_replace - len_search) * count + 1;
	tmp = new char[len_tmp];
	memset(tmp, '\0', len_tmp);
	result = (const char*)tmp;

	if (!result)
		return NULL;

	// first time through the loop, all the variable are set correctly
	// from here on,
	//    tmp points to the end of the result string
	//    ins points to the next occurrence of search in text
	//    text points to the remainder of text after "end of search"
	while (count--) {
		ins = strstr(text, search);
		len_front = ins - text;
		tmp = strncpy(tmp, text, len_front) + len_front;
		tmp = strncpy(tmp, replace, len_replace) + len_replace;
		text += len_front + len_search; // move to next "end of search"
	}
	strncpy(tmp, text, strlen(text));
	return result;
}

std::string& htmlEntityDecode(std::string& text)
{
	struct decode_table {
		const char* code;
		const char* htmlCode;
	};
	decode_table dt[] =
	{
		{"\n", "&#x0a;"},
		{"\n", "&#x0d;"},
		{" ",  "&nbsp;"},
		{"&",  "&amp;"},
		{"<",  "&lt;"},
		{">",  "&gt;"},
		{"\"", "&quot;"},
		{"'",  "&apos;"},
		{"€",  "&euro;"},
		{"–",  "&#8211;"},
		{"“",  "&#8220;"},
		{"”",  "&#8221;"},
		{"„",  "&#8222;"},
		{"•",  "&#8226;"},
		{"…",  "&#8230;"},
		{NULL,  NULL}
	};
	for (int i = 0; dt[i].code != NULL; i++)
		text = str_replace(dt[i].htmlCode, dt[i].code, text);

	return text;
}	

CFileHelpers::CFileHelpers()
{
	FileBufMaxSize	= 0xFFFF;
	ConsoleQuiet	= false;
	clearDebugInfo();
}

CFileHelpers::~CFileHelpers()
{
}

char* CFileHelpers::initFileBuf(char* buf, uint32_t size)
{
	if (buf == NULL)
		buf = new char[size];
	return buf;
}

char* CFileHelpers::deleteFileBuf(char* buf)
{
	if (buf != NULL)
		delete [] buf;
	buf = NULL;
	return buf;
}

CFileHelpers* CFileHelpers::getInstance()
{
	static CFileHelpers* FileHelpers = NULL;
	if(!FileHelpers)
		FileHelpers = new CFileHelpers();
	return FileHelpers;
}

void CFileHelpers::clearDebugInfo()
{
	DebugInfo.msg.clear();
	DebugInfo.file.clear();
	DebugInfo.func.clear();
	DebugInfo.line = 0;
}

void CFileHelpers::setDebugInfo(const char* msg, const char* file, const char* func, int line)
{
	DebugInfo.msg  = msg;
	DebugInfo.file = file;
	DebugInfo.func = func;
	DebugInfo.line = line;
}

void CFileHelpers::readDebugInfo(helpersDebugInfo* di)
{
	di->msg  = DebugInfo.msg;
	di->file = DebugInfo.file;
	di->func = DebugInfo.func;
	di->line = DebugInfo.line;
}

void CFileHelpers::printDebugInfo()
{
	if (!ConsoleQuiet)
		printf(">>>> [%s:%d] %s\n", DebugInfo.func.c_str(), DebugInfo.line, DebugInfo.msg.c_str());
}

bool CFileHelpers::cp(const char *Src, const char *Dst, const char *Flags/*=""*/)
{
	clearDebugInfo();
	if ((Src == NULL) || (Dst == NULL)) {
		setDebugInfo("One or more parameters are NULL", __path_file__, __func__, __LINE__);
		printDebugInfo();
		return false;
	}

	std::string src = Src;
	src = trim(src);
	if (src.find_first_of("/") != 0)
		src = "./" + src;
	size_t pos = src.find_last_of("/");
	if (pos == src.length()-1)
		src = src.substr(0, pos);

	std::string dst = Dst;
	dst = trim(dst);
	if (dst.find_first_of("/") != 0)
		dst = "./" + dst;
	pos = dst.find_last_of("/");
	if (pos == dst.length()-1)
		dst = dst.substr(0, pos);

	bool wildcards      = (src.find("*") != std::string::npos);
	bool recursive      = ((strchr(Flags, 'r') != NULL) || (strchr(Flags, 'a') != NULL));
	bool no_dereference = ((strchr(Flags, 'd') != NULL) || (strchr(Flags, 'a') != NULL));

	static struct stat FileInfo;
	char buf[PATH_MAX];
	if (wildcards == false) {
		if (!file_exists(src.c_str())) {
			setDebugInfo("Source file not exist", __path_file__, __func__, __LINE__);
			printDebugInfo();
			return false;
		}
		if (lstat(src.c_str(), &FileInfo) == -1) {
			setDebugInfo("lstat error", __path_file__, __func__, __LINE__);
			printDebugInfo();
			return false;
		}

		pos = src.find_last_of("/");
		std::string fname = src.substr(pos);

		static struct stat FileInfo2;
		// is symlink
		if (S_ISLNK(FileInfo.st_mode)) {
			int len = readlink(src.c_str(), buf, sizeof(buf)-1);
			if (len != -1) {
				buf[len] = '\0';
				if (!no_dereference) { /* copy */
					std::string buf_ = (std::string)buf;
					char buf2[PATH_MAX + 1];
					if (buf[0] != '/')
						buf_ = getPathName(src) + "/" + buf_;
					buf_ = (std::string)realpath(buf_.c_str(), buf2);
					//printf("\n>>>> RealPath: %s\n \n", buf_.c_str());
					if (file_exists(dst.c_str()) && (lstat(dst.c_str(), &FileInfo2) != -1)){
						if (S_ISDIR(FileInfo2.st_mode))
							copyFile(buf_.c_str(), (dst + fname).c_str());
						else {
							unlink(dst.c_str());
							copyFile(buf_.c_str(), dst.c_str());
						}
					}
					else
						copyFile(buf_.c_str(), dst.c_str());
				}
				else { /* link */
					if (file_exists(dst.c_str()) && (lstat(dst.c_str(), &FileInfo2) != -1)){
						if (S_ISDIR(FileInfo2.st_mode))
							symlink(buf, (dst + fname).c_str());
						else {
							unlink(dst.c_str());
							symlink(buf, dst.c_str());
						}
					}
					else
						symlink(buf, dst.c_str());
				}
			}
		}
		// is directory
		else if (S_ISDIR(FileInfo.st_mode)) {
			if (recursive)
				copyDir(src.c_str(), dst.c_str());
			else {
				setDebugInfo("'recursive flag' must be set to copy dir.", __path_file__, __func__, __LINE__);
				printDebugInfo();
				return false;
			}
		}
		// is file
		else if (S_ISREG(FileInfo.st_mode)) {
			if (file_exists(dst.c_str()) && (lstat(dst.c_str(), &FileInfo2) != -1)){
				if (S_ISDIR(FileInfo2.st_mode))
					copyFile(src.c_str(), (dst + fname).c_str());
				else {
					unlink(dst.c_str());
					copyFile(src.c_str(), dst.c_str());
				}
			}
			else
				copyFile(src.c_str(), dst.c_str());
		}
		else {
			setDebugInfo("Currently unsupported st_mode.", __path_file__, __func__, __LINE__);
			printDebugInfo();
			return false;
		}
	}
	else {
		setDebugInfo("Wildcard feature not yet realized.", __path_file__, __func__, __LINE__);
		printDebugInfo();
		return false;
	}

	return true;
}

bool CFileHelpers::copyFile(const char *Src, const char *Dst, mode_t forceMode/*=0*/)
{
	/*
	set mode for Dst
	----------------
	when forceMode==0 (default) then
	    when Dst exists
	        mode = mode from Dst
	    else
	        mode = mode from Src
	else
	    mode = forceMode
	*/
	mode_t mode = forceMode & 0x0FFF;
	if (mode == 0) {
		static struct stat FileInfo;
		const char *f = Dst;
		if (!file_exists(Dst))
			f = Src;
		if (lstat(f, &FileInfo) == -1)
			return false;
		mode = FileInfo.st_mode & 0x0FFF;
	}

	if ((fd1 = open(Src, O_RDONLY)) < 0)
		return false;
	if (file_exists(Dst))
		unlink(Dst);
	if ((fd2 = open(Dst, O_WRONLY | O_CREAT, mode)) < 0) {
		close(fd1);
		return false;
	}

	char* FileBuf = NULL;
	uint32_t block;
	off64_t fsizeSrc64 = lseek64(fd1, 0, SEEK_END);
	lseek64(fd1, 0, SEEK_SET);
	off64_t fsize64 = fsizeSrc64;
	uint32_t FileBufSize = (fsizeSrc64 < (off_t)FileBufMaxSize) ? (uint32_t)fsizeSrc64 : FileBufMaxSize;
	FileBuf = initFileBuf(FileBuf, FileBufSize);
	block = FileBufSize;
	//printf("#####[%s] fsizeSrc64: %lld 0x%010llX - large file\n", __func__, fsizeSrc64, fsizeSrc64);
	while (fsize64 > 0) {
		if (fsize64 < (off64_t)FileBufSize)
			block = (uint32_t)fsize64;
		read(fd1, FileBuf, block);	/* FIXME: short read??? */
		write(fd2, FileBuf, block);	/* FIXME: short write?? */
		fsize64 -= block;
	}
	lseek64(fd2, 0, SEEK_SET);
	off64_t fsizeDst64 = lseek64(fd2, 0, SEEK_END);
	if (fsizeSrc64 != fsizeDst64) {
		close(fd1);
		close(fd2);
		FileBuf = deleteFileBuf(FileBuf);
		return false;
	}
	close(fd1);
	close(fd2);

	FileBuf = deleteFileBuf(FileBuf);
	return true;
}

bool CFileHelpers::copyDir(const char *Src, const char *Dst, bool backupMode)
{
	DIR *Directory;
	struct dirent *CurrentFile;
	static struct stat FileInfo;
	char srcPath[PATH_MAX];
	char dstPath[PATH_MAX];
	char buf[PATH_MAX];

	//open directory
	if ((Directory = opendir(Src)) == NULL)
		return false;
	if (lstat(Src, &FileInfo) == -1) {
		closedir(Directory);
		return false;
	}
	// create directory
		// is symlink
	if (S_ISLNK(FileInfo.st_mode)) {
		int len = readlink(Src, buf, sizeof(buf)-1);
		if (len != -1) {
			buf[len] = '\0';
			symlink(buf, Dst);
		}
	}
	else {
		// directory
		if (!createDir(Dst, FileInfo.st_mode & 0x0FFF)) {
			closedir(Directory);
			return false;
		}
	}

	// read directory
	while ((CurrentFile = readdir(Directory)) != NULL) {
		// ignore '.' and '..'
		if (strcmp(CurrentFile->d_name, ".") && strcmp(CurrentFile->d_name, "..")) {
			strcpy(srcPath, Src);
			strcat(srcPath, "/");
			strcat(srcPath, CurrentFile->d_name);
			if (lstat(srcPath, &FileInfo) == -1) {
				closedir(Directory);
				return false;
			}
			strcpy(dstPath, Dst);
			strcat(dstPath, "/");
			strcat(dstPath, CurrentFile->d_name);
			// is symlink
			if (S_ISLNK(FileInfo.st_mode)) {
				int len = readlink(srcPath, buf, sizeof(buf)-1);
				if (len != -1) {
					buf[len] = '\0';
					symlink(buf, dstPath);
				}
			}
			// is directory
			else if (S_ISDIR(FileInfo.st_mode)) {
				copyDir(srcPath, dstPath);
			}
			// is file
			else if (S_ISREG(FileInfo.st_mode)) {
				std::string save = "";
				(void)backupMode; /* squelch unused parameter warning */
#if ENABLE_EXTUPDATE
				if (backupMode && (CExtUpdate::getInstance()->isBlacklistEntry(srcPath)))
					save = ".save";
#endif
				copyFile(srcPath, (dstPath + save).c_str()); /* mode is set by copyFile */
			}
		}
	}
	closedir(Directory);
	return true;
}

// returns:	 true - success.
//		 false - errno is set
bool CFileHelpers::createDir(string& Dir, mode_t mode)
{
	CFileHelpers* fh = CFileHelpers::getInstance();
	fh->clearDebugInfo();
	int res = 0;
	for(string::iterator iter = Dir.begin() ; iter != Dir.end();) {
		string::iterator newIter = find(iter, Dir.end(), '/' );
		string newPath = string( Dir.begin(), newIter );
		if(!newPath.empty() && !file_exists(newPath.c_str())) {
			res = mkdir( newPath.c_str(), mode);
			if (res == -1) {
				if (errno == EEXIST) {
					res = 0;
				} else {
					// We can assume that if an error
					// occured, following will fail too,
					// so break here.
					if (!fh->getConsoleQuiet())
						dprintf(DEBUG_NORMAL, "[CFileHelpers %s] creating directory %s: %s\n", __func__, newPath.c_str(), strerror(errno));
					char buf[1024];
					memset(buf, '\0', sizeof(buf));
					snprintf(buf, sizeof(buf)-1, "creating directory %s: %s", newPath.c_str(), strerror(errno));
					fh->setDebugInfo(buf, __path_file__, __func__, __LINE__);
					break;
				}
			}
		}
		iter = newIter;
		if(newIter != Dir.end())
			++ iter;
	}

	return (res == 0 ? true : false);
}

bool CFileHelpers::removeDir(const char *Dir)
{
	CFileHelpers* fh = CFileHelpers::getInstance();
	fh->clearDebugInfo();
	DIR *dir;
	struct dirent *entry;
	char path[PATH_MAX];

	dir = opendir(Dir);
	if (dir == NULL) {
		if (errno == ENOENT)
			return true;
		if (!fh->getConsoleQuiet())
			dprintf(DEBUG_NORMAL, "[CFileHelpers %s] remove directory %s: %s\n", __func__, Dir, strerror(errno));
		char buf[1024];
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf)-1, "remove directory %s: %s", Dir, strerror(errno));
		fh->setDebugInfo(buf, __path_file__, __func__, __LINE__);
		return false;
	}
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			snprintf(path, (size_t) PATH_MAX, "%s/%s", Dir, entry->d_name);
			if (entry->d_type == DT_DIR)
				removeDir(path);
			else
				unlink(path);
		}
	}
	closedir(dir);
	rmdir(Dir);

	errno = 0;
	return true;
}

u_int64_t CFileHelpers::getDirSize(const char *dirname)
{
	DIR *dir;
	char fullDirName[500];
	struct dirent *dirPnt;
	struct stat cur_file;
	uint64_t total_size = 0;

	//open current dir
	sprintf(fullDirName, "%s/", dirname);
	if((dir = opendir(fullDirName)) == NULL) {
		fprintf(stderr, "Couldn't open %s\n", fullDirName);
		return 0;
	}

	//go through the directory
	while( (dirPnt = readdir(dir)) != NULL ) {
		if(strcmp((*dirPnt).d_name, "..") == 0 || strcmp((*dirPnt).d_name, ".") == 0)
			continue;

		//create current filepath
		sprintf(fullDirName, "%s/%s", dirname, (*dirPnt).d_name);
		if(stat(fullDirName, &cur_file) == -1)
			continue;

		if(cur_file.st_mode & S_IFREG) //file...
			total_size += cur_file.st_size;
		else if(cur_file.st_mode & S_IFDIR) //dir...
			total_size += getDirSize(fullDirName);
	}
	closedir(dir);

	return total_size;
}

static int hdd_open_dev(const char * fname)
{
	FILE * fp;
	struct mntent * mnt;
	dev_t dev;
	struct stat st;
	int fd = -1;

	if (stat(fname, &st) != 0) {
		perror(fname);
		return fd;
	}

	dev = st.st_dev;
	fp = setmntent("/proc/mounts", "r");
	if (fp == NULL) {
		perror("setmntent");
		return fd;
	}

	while ((mnt = getmntent(fp)) != NULL) {
		if (stat(mnt->mnt_fsname, &st) != 0)
			continue;
		if (S_ISBLK(st.st_mode) && st.st_rdev == dev) {
			printf("[hdd] file [%s] -> dev [%s]\n", fname, mnt->mnt_fsname);
			fd = open(mnt->mnt_fsname, O_RDONLY|O_NONBLOCK);
			if (fd < 0)
				perror(mnt->mnt_fsname);
			break;
		}
	}
	endmntent(fp);
	return fd;
}

bool hdd_get_standby(const char * fname)
{
	bool standby = false;

	int fd = hdd_open_dev(fname);
	if (fd >= 0) {
		unsigned char args[4] = {WIN_CHECKPOWERMODE1,0,0,0};
		int ret = ioctl(fd, HDIO_DRIVE_CMD, args);
		if (ret) {
			args[0] = WIN_CHECKPOWERMODE2;
			ret = ioctl(fd, HDIO_DRIVE_CMD, args);
		}
		if ((ret == 0) && (args[2] != 0xFF))
			standby = true;

		printf("[hdd] %s\n", standby ? "standby" : "active");
		close(fd);
	}
	return standby;
}

void hdd_flush(const char * fname)
{
	int fd = hdd_open_dev(fname);
	if (fd >= 0) {
		printf("[hdd] flush buffers...\n");
		fsync(fd);
		if (ioctl(fd, BLKFLSBUF, NULL))
			perror("BLKFLSBUF");
		else
			ioctl(fd, HDIO_DRIVE_CMD, NULL);
		close(fd);
	}
}

/* split string like PARAM1=value1 PARAM2=value2 into map */
bool split_config_string(const std::string &str, std::map<std::string,std::string> &smap)
{
	smap.clear();
	std::string::size_type start = 0;
	std::string::size_type end = 0;
	while ((end = str.find(" ", start)) != std::string::npos) {
		std::string param = str.substr(start, end - start);
		std::string::size_type i = param.find("=");
		if (i != std::string::npos) {
			smap[param.substr(0,i).c_str()] = param.substr(i+1).c_str();
		}
		start = end + 1;
	}
	return !smap.empty();
}

#if 0
/* align for hw blit */
uint32_t GetWidth4FB_HW_ACC(const uint32_t _x, const uint32_t _w, const bool max)
{
	uint32_t ret = _w;
	static uint32_t xRes = 0;
	if (xRes == 0)
		xRes = CFrameBuffer::getInstance()->getScreenWidth(true);
	if ((_x + ret) >= xRes)
		ret = xRes-_x-1;
	if (ret%4 == 0)
		return ret;

	int add = (max) ? 3 : 0;
	ret = ((ret + add) / 4) * 4;
	if ((_x + ret) >= xRes)
		ret -= 4;

	return ret;
}
#endif

std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> vec;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
		vec.push_back(item);
	return vec;
}

#if __cplusplus < 201103L
std::string to_string(int i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

std::string to_string(unsigned int i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

std::string to_string(long i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

std::string to_string(unsigned long i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

std::string to_string(long long i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

std::string to_string(unsigned long long i)
{
	std::stringstream s;
	s << i;
	return s.str();
}
#endif

/**
 * C++ version 0.4 std::string style "itoa":
 * Contributions from Stuart Lowe, Ray-Yuan Sheu,

 * Rodrigo de Salvo Braz, Luc Gallant, John Maloney
 * and Brian Hunt
 */
std::string itoa(int value, int base)
{
	std::string buf;

	// check that the base if valid
	if (base < 2 || base > 16) return buf;

	enum { kMaxDigits = 35 };
	buf.reserve( kMaxDigits ); // Pre-allocate enough space.

	int quotient = value;

	// Translating number to string with base:
	do {
		buf += "0123456789abcdef"[ std::abs( quotient % base ) ];
		quotient /= base;
	} while ( quotient );

	// Append the negative sign
	if ( value < 0) buf += '-';

	std::reverse( buf.begin(), buf.end() );
	return buf;
}

std::string getJFFS2MountPoint(int mtdPos)
{
	FILE* fd = fopen("/proc/mounts", "r");
	if (!fd) return "";
	int iBlock;
	char lineRead[1024], sMount[512], sFs[512];
	memset(lineRead, '\0', sizeof(lineRead));
	while (fgets(lineRead, sizeof(lineRead)-1, fd)) {
		sscanf(lineRead, "/dev/mtdblock%d %511s %511s", &iBlock, sMount, sFs);
		if ((iBlock == mtdPos) && (strstr(sMount, "/") != NULL) && (strstr(sFs, "jffs2") != NULL)) {
			fclose(fd);
			return sMount;
		}
		memset(lineRead, '\0', sizeof(lineRead));
	}
	fclose(fd);
	return "";
}

std::string Lang2ISO639_1(std::string& lang)
{
	std::string ret = "";
	if ((lang == "deutsch") || (lang == "bayrisch") || (lang == "ch-baslerdeutsch") || (lang == "ch-berndeutsch"))
		ret = "de";
	else if (lang == "english")
		ret = "en";
	else if (lang == "nederlands")
		ret = "nl";
	else if (lang == "slovak")
		ret = "sk";
	else if (lang == "bosanski")
		ret = "bs";
	else if (lang == "czech")
		ret = "cs";
	else if (lang == "francais")
		ret = "fr";
	else if (lang == "italiano")
		ret = "it";
	else if (lang == "polski")
		ret = "pl";
	else if (lang == "portugues")
		ret = "pt";
	else if (lang == "russkij")
		ret = "ru";
	else if (lang == "suomi")
		ret = "fi";
	else if (lang == "svenska")
		ret = "sv";

	return ret;
}

// returns the pid of the first process found in /proc
int getpidof(const char *process)
{
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;

	if ((dp = opendir("/proc")) == NULL)
	{
		fprintf(stderr, "Cannot open directory /proc\n");
		return -1;
	}

	while ((entry = readdir(dp)) != NULL)
	{
		// get information about the file/folder
		lstat(entry->d_name, &statbuf);
		// files under /proc which start with a digit are processes
		if (S_ISDIR(statbuf.st_mode) && isdigit(entry->d_name[0]))
		{
			// 14 chars for /proc//status + 0
			char procpath[14 + strlen(entry->d_name)];
			char procname[50];
			FILE *file;

			sprintf(procpath, "/proc/%s/status", entry->d_name);

			if (! (file = fopen(procpath, "r")) ) {
				continue;
			}

			fscanf(file,"%*s %s", procname);
			fclose(file);

			// only 15 char available
			if (strncmp(procname, process, 15) == 0) {
				return atoi(entry->d_name);
			}
		}
	}
	closedir (dp);
	return 0;
}

std::string filehash(const char *file)
{
#if 0
	int fd;
	int i;
	unsigned long size;
	struct stat s_stat;
	unsigned char hash[MD5_DIGEST_LENGTH];
	void *buff;
	std::ostringstream os;
 
	memset(hash, 0, MD5_DIGEST_LENGTH);

	fd = open(file, O_RDONLY | O_NONBLOCK);
	if (fd > 0)
	{
		// Get the size of the file by its file descriptor
		fstat(fd, &s_stat);
		size = s_stat.st_size;

		buff = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
		MD5((const unsigned char *)buff, size, hash);
		munmap(buff, size);

		// Print the MD5 sum as hex-digits.
		for(i = 0; i < MD5_DIGEST_LENGTH; ++i) {
			os.width(2);
			os.fill('0');
			os << std::hex << static_cast<int>(hash[i]);
		}
		close(fd);
	}
	return os.str();
#endif
	int i;
	unsigned char hash[MD5_DIGEST_LENGTH];
	std::ostringstream os;

	md5_file(file, 1, (unsigned char*) hash);
	// Print the MD5 sum as hex-digits.
	for(i = 0; i < MD5_DIGEST_LENGTH; ++i) {
		os.width(2);
		os.fill('0');
		os << std::hex << static_cast<int>(hash[i]);
	}
	return os.str();
}

std::string get_path(const char *path)
{
	if(path[0] == '/' && strstr(path,"/var") == 0)
	{
		std::string varc = "/var";
		varc += path;

		if(file_exists(varc.c_str()))
			return varc;
	}

	return path;
}

string readLink(string lnk)
{
	char buf[PATH_MAX];
	memset(buf, 0, sizeof(buf)-1);
	if (readlink(lnk.c_str(), buf, sizeof(buf)-1) != -1)
		return (string)buf;

	return "";
}
