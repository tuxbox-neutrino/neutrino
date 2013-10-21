/*
	update with settings - Neutrino-GUI

	Copyright (C) 2001 Steffen Hehn 'McClean'
	and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Copyright (C) 2012-2013 M. Liebmann (micha-bbg)

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CEXTUPDATE__
#define __CEXTUPDATE__
#if ENABLE_EXTUPDATE

#include <gui/widget/menue.h>
#include <system/helpers.h>

#include <string>

class CExtUpdate
{
	 private:
		enum
		{
			RESET_UNLOAD	= 1,
			CLOSE_FD1	= 2,
			CLOSE_FD2	= 4,
			CLOSE_F1	= 8,
			DELETE_MTDBUF	= 16
		};
		std::string imgFilename;
		std::string mtdRamError;
		int mtdNumber;
		int fd1, fd2;
		FILE *f1;
		char *MTDBuf;
		std::string mtdramDriver;
		std::string backupList, defaultBackup;
		std::string mountPkt;
		CFileHelpers* FileHelpers;
		std::vector<std::string> copyList, blackList, deleteList;

		bool flashErrorFlag;
		long bsize;
		uint64_t total, used;
		uint64_t free1, free2, free3;

		bool applySettings(void);
		bool readBackupList(const std::string & dstPath);
		bool copyFileList(const std::string & fileList, const std::string & dstPath);
		bool deleteFileList(const std::string & fileList);
		bool readConfig(const std::string & Config);
		bool findConfigEntry(std::string & line, std::string find);
		bool isMtdramLoad();
		bool checkSpecialFolders(std::string line, bool copy);

		FILE * fUpdate;
		char updateLogBuf[1024];
		std::string fLogfile;
		int fLogEnabled;
		void updateLog(const char *buf);

 	public:
		enum
		{
			MODE_EXPERT	= 0,
			MODE_SOFTUPDATE	= 1
		};
		enum {
			SOFTUPDATE_NAME_DEFAULT,
			SOFTUPDATE_NAME_HOSTNAME_TIME,
			SOFTUPDATE_NAME_ORGNAME_TIME
		};
		CExtUpdate();
		~CExtUpdate();
		static CExtUpdate* getInstance();

		bool applySettings(std::string & filename, int mode);
		bool ErrorReset(bool modus, const std::string & msg1="", const std::string & msg2="");
		bool isBlacklistEntry(const std::string & file);

};

#ifdef UPDATE_DEBUG_TIMER
static struct timeval timer_tv, timer_tv2;
static unsigned int timer_msec;
#define DBG_TIMER_START() gettimeofday(&timer_tv, NULL);
#define DBG_TIMER_STOP(label)												\
	gettimeofday(&timer_tv2, NULL);											\
	timer_msec = ((timer_tv2.tv_sec - timer_tv.tv_sec) * 1000) + ((timer_tv2.tv_usec - timer_tv.tv_usec) / 1000);	\
	printf("##### [%s] %s: %u msec\n", __FILE__, label, timer_msec);
#else
#define DBG_TIMER_START()
#define DBG_TIMER_STOP(label)
#endif // UPDATE_DEBUG_TIMER

#ifdef UPDATE_DEBUG
#define DBG_MSG(fmt, args...) printf("#### [%s:%s:%d] " fmt "\n", __FILE__, __FUNCTION__, __LINE__ , ## args);
#else
#define DBG_MSG(fmt, args...)
#endif // UPDATE_DEBUG

#define WRITE_UPDATE_LOG(fmt, args...) \
		snprintf(updateLogBuf, sizeof(updateLogBuf), "[update:%d] " fmt, __LINE__ , ## args); \
		updateLog(updateLogBuf);

#endif

#endif // __CEXTUPDATE__
