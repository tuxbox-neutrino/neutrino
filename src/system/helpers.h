
#ifndef __system_helpers__
#define __system_helpers__

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

#include <string>

int my_system(const char * cmd);
int my_system(int argc, const char *arg, ...); /* argc is number of arguments including command */

FILE* my_popen( pid_t& pid, const char *cmdstring, const char *type);
int safe_mkdir(char * path);
off_t file_size(const char *filename);
bool file_exists(const char *filename);
void wakeup_hdd(const char *hdd_dir);
int check_dir(const char * dir);
bool get_fs_usage(const char * dir, long &total, long &used, long *bsize=NULL);
bool get_mem_usage(unsigned long &total, unsigned long &free);

std::string getPathName(std::string &path);
std::string getBaseName(std::string &path);
std::string getFileName(std::string &file);
std::string getFileExt(std::string &file);
std::string getNowTimeStr(const char* format);
std::string trim(std::string &str, const std::string &trimChars = " \n\r\t");

class CFileHelpers
{
	private:
		int FileBufSize;
		char *FileBuf;
		int fd1, fd2;

	public:
		CFileHelpers();
		~CFileHelpers();
		static CFileHelpers* getInstance();
		bool doCopyFlag;

		bool copyFile(const char *Src, const char *Dst, mode_t mode);
		bool copyDir(const char *Src, const char *Dst, bool backupMode=false);
		bool createDir(const char *Dir, mode_t mode);
		bool removeDir(const char *Dir);

};

#endif
