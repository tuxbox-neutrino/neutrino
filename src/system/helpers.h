
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

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>

int my_system(const char * cmd);
int my_system(int argc, const char *arg, ...); /* argc is number of arguments including command */

FILE* my_popen( pid_t& pid, const char *cmdstring, const char *type);
int run_pty(pid_t &pid, const char *cmdstring);

int safe_mkdir(const char * path);
inline int safe_mkdir(std::string path) { return safe_mkdir(path.c_str()); }
//int mkdirhier(const char *pathname, mode_t mode = 0755);
//inline int mkdirhier(std::string path, mode_t mode = 0755) { return mkdirhier(path.c_str(), mode); }
off_t file_size(const char *filename);
bool file_exists(const char *filename);
void wakeup_hdd(const char *hdd_dir);
int check_dir(const char * dir, bool allow_tmp = false);
bool get_fs_usage(const char * dir, uint64_t &btotal, uint64_t &bused, long *bsize=NULL);
bool get_mem_usage(unsigned long &total, unsigned long &free);
int mySleep(int sec);

std::string find_executable(const char *name);
/* basically what "foo=`command`" does in the shell */
std::string backtick(std::string command);

bool hdd_get_standby(const char * fname);
void hdd_flush(const char * fname);

std::string getPathName(std::string &path);
std::string getBaseName(std::string &path);
std::string getFileName(std::string &file);
std::string getFileExt(std::string &file);
std::string getNowTimeStr(const char* format);
std::string trim(std::string &str, const std::string &trimChars = " \n\r\t");
std::string cutString(const std::string str, int msgFont, const int width);
std::string strftime(const char *format, const struct tm *tm);
std::string strftime(const char *format, time_t when, bool gm = false);
time_t toEpoch(std::string &date);
std::string& str_replace(const std::string &search, const std::string &replace, std::string &text);
std::string& htmlEntityDecode(std::string& text);

struct helpersDebugInfo {
	std::string msg;
	std::string file;
	std::string func;
	int line;
};

class CFileHelpers
{
	private:
		uint32_t FileBufMaxSize;
		int fd1, fd2;

		char* initFileBuf(char* buf, uint32_t size);
		char* deleteFileBuf(char* buf);
		bool ConsoleQuiet;
		helpersDebugInfo DebugInfo;
		void setDebugInfo(const char* msg, const char* file, const char* func, int line);
		void printDebugInfo();

	public:
		CFileHelpers();
		~CFileHelpers();
		static CFileHelpers* getInstance();
		bool doCopyFlag;

		void clearDebugInfo();
		void readDebugInfo(helpersDebugInfo* di);
		void setConsoleQuiet(bool q) { ConsoleQuiet = q; };
		bool getConsoleQuiet() { return ConsoleQuiet; };

		bool cp(const char *Src, const char *Dst, const char *Flags="");
		bool copyFile(const char *Src, const char *Dst, mode_t forceMode=0);
		bool copyDir(const char *Src, const char *Dst, bool backupMode=false);
		static bool createDir(std::string& Dir, mode_t mode = 0755);
		static bool createDir(const char *Dir, mode_t mode = 0755){std::string dir = std::string(Dir);return createDir(dir, mode);}
		static bool removeDir(const char *Dir);
		static uint64_t getDirSize(const char *dir);
		static uint64_t getDirSize(const std::string& dir){return getDirSize(dir.c_str());};
};

#if 0
uint32_t GetWidth4FB_HW_ACC(const uint32_t _x, const uint32_t _w, const bool max=true);
#endif

#if __cplusplus < 201103L
std::string to_string(int);
std::string to_string(unsigned int);
std::string to_string(long);
std::string to_string(unsigned long);
std::string to_string(long long);
std::string to_string(unsigned long long);
#else
/* hack... */
#define to_string(x) std::to_string(x)
#endif

std::string itoa(int value, int base);

inline int atoi(std::string &s) { return atoi(s.c_str()); }
inline int atoi(const std::string &s) { return atoi(s.c_str()); }
inline int access(std::string &s, int mode) { return access(s.c_str(), mode); }
inline int access(const std::string &s, int mode) { return access(s.c_str(), mode); }

inline void cstrncpy(char *dest, const char * const src, size_t n) { n--; strncpy(dest, src, n); dest[n] = 0; }
inline void cstrncpy(char *dest, const std::string &src, size_t n) { n--; strncpy(dest, src.c_str(), n); dest[n] = 0; }

std::vector<std::string> split(const std::string &s, char delim);

bool split_config_string(const std::string &str, std::map<std::string,std::string> &smap);

std::string getJFFS2MountPoint(int mtdPos);
std::string Lang2ISO639_1(std::string& lang);
std::string readLink(std::string lnk);

#endif
