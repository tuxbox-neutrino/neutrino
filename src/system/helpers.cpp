/*
	Neutrino-HD

	License: GPL

	(C) 2012-2013 the neutrino-hd developers
	(C) 2012,2013 Stefan Seyfried

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
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <sys/time.h>	/* gettimeofday */
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <mntent.h>
#include <linux/hdreg.h>
#include <linux/fs.h>

#include <system/helpers.h>
#include <gui/update_ext.h>

void mySleep(int sec) {
	struct timeval timeout;

	timeout.tv_sec = sec;
	timeout.tv_usec = 0;
	select(0,0,0,0, &timeout);
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
	int i = 0, ret = 0, childExit = 0;
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
		path = strdupa("/bin:/usr/bin:/sbin:/usr/sbin");
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
	gettimeofday(&tv, NULL);        
	strftime(tmpStr, sizeof(tmpStr), format, localtime(&tv.tv_sec));
	return (std::string)tmpStr;
}

std::string trim(std::string &str, const std::string &trimChars /*= " \n\r\t"*/)
{
	std::string result = str.erase(str.find_last_not_of(trimChars) + 1);
	return result.erase(0, result.find_first_not_of(trimChars));
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

std::string& htmlEntityDecode(std::string& text)
{
	struct decode_table {
		const char* code;
		const char* htmlCode;
	};
	decode_table dt[] =
	{
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
	FileBufSize	= 0xFFFF;
	FileBuf		= new char[FileBufSize];
}

CFileHelpers::~CFileHelpers()
{
	if (FileBuf != NULL)
		delete [] FileBuf;
}

CFileHelpers* CFileHelpers::getInstance()
{
	static CFileHelpers* FileHelpers = NULL;
	if(!FileHelpers)
		FileHelpers = new CFileHelpers();
	return FileHelpers;
}

bool CFileHelpers::copyFile(const char *Src, const char *Dst, mode_t mode)
{
	unlink(Dst);
	if ((fd1 = open(Src, O_RDONLY)) < 0)
		return false;
	if ((fd2 = open(Dst, O_WRONLY | O_CREAT, mode)) < 0) {
		close(fd1);
		return false;
	}

	uint32_t block;
	off64_t fsizeSrc64 = lseek64(fd1, 0, SEEK_END);
	lseek64(fd1, 0, SEEK_SET);
	off64_t fsize64 = fsizeSrc64;
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
		return false;
	}
	close(fd1);
	close(fd2);

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
		if (createDir(Dst, FileInfo.st_mode & 0x0FFF) == false) {
			if (errno != EEXIST) {
				closedir(Directory);
				return false;
			}
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
				copyFile(srcPath, (dstPath + save).c_str(), FileInfo.st_mode & 0x0FFF);
			}
		}
	}
	closedir(Directory);
	return true;
}

bool CFileHelpers::createDir(const char *Dir, mode_t mode)
{
	char dirPath[PATH_MAX];
	DIR *dir;
	if ((dir = opendir(Dir)) != NULL) {
		closedir(dir);
		errno = EEXIST;
		return false;
	}

	int ret = -1;
	while (ret == -1) {
		strcpy(dirPath, Dir);
		ret = mkdir(dirPath, mode);
		if ((errno == ENOENT) && (ret == -1)) {
			char * pos = strrchr(dirPath,'/');
			if (pos != NULL) {
				pos[0] = '\0';
				createDir(dirPath, mode);
			}
		}
		else
			return !ret || (errno == EEXIST);
	}
	errno = 0;
	return true;
}

bool CFileHelpers::removeDir(const char *Dir)
{
	DIR *dir;
	struct dirent *entry;
	char path[PATH_MAX];

	dir = opendir(Dir);
	if (dir == NULL) {
		printf("Error opendir()\n");
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

std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> vec;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
		vec.push_back(item);
	return vec;
}

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

