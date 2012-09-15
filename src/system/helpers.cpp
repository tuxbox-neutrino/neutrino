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
        int i=0 ,ret=0;
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
			if(execlp(cmd, cmd, arg1, arg2, arg3,  arg4, arg5, arg6, NULL))
			{
				std::string txt = "ERROR: my_system \"" + (std::string) cmd + "\"";
				perror(txt.c_str());
				ret = -1;
			}
			_exit (0); // terminate c h i l d proces s only	
		default: /* parent returns to calling process */
			break;
	}
	waitpid(pid, 0, 0);
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

int check_dir(const char * newdir)
{
  
  	struct statfs s;
	if (::statfs(newdir, &s) == 0) {
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
			case 0x858458f6: 	/*ramfs*/
				return 0;//ok
			default:
				fprintf( stderr,"%s Unknow File system type: %i\n",newdir ,s.f_type);
				break;
		}
	}
	return 1;	// error
}
