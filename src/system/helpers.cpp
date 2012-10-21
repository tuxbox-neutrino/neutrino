/*
	Neutrino-HD

	License: GPL

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
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <zapit/debug.h>

#include <system/helpers.h>

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
	if(!check_dir(hdd_dir)){
		std::string wakeup_file = hdd_dir;
		wakeup_file += "/.wakeup";
		remove(wakeup_file.c_str());
		creat(wakeup_file.c_str(),S_IREAD|S_IWRITE);
		sync();
	}
}
//use for script with full path
int my_system(const char * cmd)
{
	if (!file_exists(cmd))
		return -1;

	return my_system(cmd, NULL);
}

int my_system(const char * cmd, const char * arg1, const char * arg2, const char * arg3, const char * arg4, const char * arg5, const char * arg6)
{
	int i=0 ,ret=0, childExit=0;
	pid_t pid;
	int maxfd = getdtablesize();// sysconf(_SC_OPEN_MAX);
	switch (pid = vfork())
	{
		case -1: /* can't vfork */
			perror("vfork");
			return -1;
		case 0: /* child process */
			for(i = 3; i < maxfd; i++)
				close(i);
			if(execlp(cmd, cmd, arg1, arg2, arg3, arg4, arg5, arg6, NULL))
			{
				std::string txt = "ERROR: my_system \"" + (std::string) cmd + "\"";
				perror(txt.c_str());
				ret = -1;
			}
			_exit (0); // terminate c h i l d proces s only	
		default: /* parent returns to calling process */
			break;
	}
	waitpid(pid, &childExit, 0);
	if(childExit != 0)
		ret = childExit;
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

int safe_mkdir(char * path)
{
	struct statfs s;
	int ret = 0;
	if(!strncmp(path, "/hdd", 4)) {
		ret = statfs("/hdd", &s);
		if((ret != 0) || (s.f_type == 0x72b6))
			ret = -1;
		else
			mkdir(path, 0755);
	} else
		mkdir(path, 0755);
	return ret;
}

/* function used to check is this dir writable, i.e. not flash, for record etc */
int check_dir(const char * dir)
{
	/* default to return, if statfs fail */
	int ret = -1;
	struct statfs s;
	if (::statfs(dir, &s) == 0) {
		switch (s.f_type)	/* f_type is long */
		{
			case 0xEF53L:		/*EXT2 & EXT3*/
			case 0x6969L:		/*NFS*/
			case 0xFF534D42L:	/*CIFS*/
			case 0x517BL:		/*SMB*/
			case 0x52654973L:	/*REISERFS*/
			case 0x65735546L:	/*fuse for ntfs*/
			case 0x58465342L:	/*xfs*/
			case 0x4d44L:		/*msdos*/
			case 0x0187:		/* AUTOFS_SUPER_MAGIC */
			case 0x858458f6L: 	/*ramfs*/
#if 0
			case 0x72b6L:		/*jffs2*/
#endif
				ret = 0;
				break; //ok
			default:
				fprintf(stderr, "%s Unknow File system type: %i\n" ,dir ,s.f_type);
				break; // error
		}
	}
	return ret;
}

bool get_fs_usage(const char * dir, long &btotal, long &bused)
{
	btotal = bused = 0;
	struct statfs s;

	if (::statfs(dir, &s) == 0 && s.f_blocks) {
		btotal = s.f_blocks;
		bused = s.f_blocks - s.f_bfree;
		//printf("fs (%s): total %ld used %ld\n", dir, btotal, bused);
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

std::string trim(std::string &str, const std::string &trimChars /*= " \n\r\t"*/)
{
	std::string result = str.erase(str.find_last_not_of(trimChars) + 1);
	return result.erase(0, result.find_first_not_of(trimChars));
}

CFileHelpers::CFileHelpers()
{
	FileBufSize	= 0xFFFF;
	FileBuf		= (char*)malloc(FileBufSize);
	doCopyFlag	= true;
}

CFileHelpers::~CFileHelpers()
{
	if (FileBuf != NULL)
		free(FileBuf);
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
	doCopyFlag = true;
	unlink(Dst);
	if ((fd1 = open(Src, O_RDONLY)) < 0)
		return false;
	if ((fd2 = open(Dst, O_WRONLY | O_CREAT)) < 0) {
		close(fd2);
		return false;
	}

	long block;
	off64_t fsizeSrc64 = lseek64(fd1, 0, SEEK_END);
	lseek64(fd1, 0, SEEK_SET);
	if (fsizeSrc64 > 0x7FFFFFF0) { // > 2GB
		off64_t fsize64 = fsizeSrc64;
		block = FileBufSize;
		//printf("#####[%s] fsizeSrc64: %lld 0x%010llX - large file\n", __FUNCTION__, fsizeSrc64, fsizeSrc64);
		while(fsize64 > 0) {
			if(fsize64 < (off64_t)FileBufSize)
				block = (long)fsize64;
			read(fd1, FileBuf, block);
			write(fd2, FileBuf, block);
			fsize64 -= block;
			if (!doCopyFlag)
				break;
		}
		if (doCopyFlag) {
			lseek64(fd2, 0, SEEK_SET);
			off64_t fsizeDst64 = lseek64(fd2, 0, SEEK_END);
			if (fsizeSrc64 != fsizeDst64)
				return false;
		}
	}
	else { // < 2GB
		long fsizeSrc = lseek(fd1, 0, SEEK_END);
		lseek(fd1, 0, SEEK_SET);
		long fsize = fsizeSrc;
		block = FileBufSize;
		//printf("#####[%s] fsizeSrc: %ld 0x%08lX - normal file\n", __FUNCTION__, fsizeSrc, fsizeSrc);
		while(fsize > 0) {
			if(fsize < (long)FileBufSize)
				block = fsize;
			read(fd1, FileBuf, block);
			write(fd2, FileBuf, block);
			fsize -= block;
			if (!doCopyFlag)
				break;
		}
		if (doCopyFlag) {
			lseek(fd2, 0, SEEK_SET);
			long fsizeDst = lseek(fd2, 0, SEEK_END);
			if (fsizeSrc != fsizeDst)
				return false;
		}
	}
	close(fd1);
	close(fd2);

	if (!doCopyFlag) {
		sync();
		unlink(Dst);
		return false;
	}

	chmod(Dst, mode);
	return true;
}

bool CFileHelpers::copyDir(const char *Src, const char *Dst)
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
				copyFile(srcPath, dstPath, FileInfo.st_mode & 0x0FFF);
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
		else {
			if (ret == 0)
				return true;
			if (errno == EEXIST)
				return true;
			else
				return false;
		}
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
