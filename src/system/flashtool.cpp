/*
	Neutrino-GUI  -   DBoxII-Project

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
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libmd5sum.h>
#include <system/flashtool.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#include <linux/version.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,7)
//#include <linux/compiler.h>
#include <mtd/mtd-user.h>
#else
#include <mtd/mtd-user.h>
//#include <linux/mtd/mtd.h>
#endif

//#include <libcramfs.h>

#include <driver/encoding.h>
#include <global.h>


CFlashTool::CFlashTool()
{
	statusViewer = NULL;
	mtdDevice = 	"";
	ErrorMessage = 	"";
}

CFlashTool::~CFlashTool()
{
}

const std::string & CFlashTool::getErrorMessage(void) const
{
	return ErrorMessage;
}

void CFlashTool::setMTDDevice( const std::string & mtddevice )
{
	mtdDevice = mtddevice;
	printf("flashtool.cpp: set mtd device to %s\n", mtddevice.c_str());
}

void CFlashTool::setStatusViewer( CProgress_StatusViewer* statusview )
{
	statusViewer = statusview;
}

bool CFlashTool::readFromMTD( const std::string & filename, int globalProgressEnd )
{
	int		fd1, fd2;
	long	filesize;
	int		globalProgressBegin = 0;

	if(statusViewer)
	{
		statusViewer->showLocalStatus(0);
	}

	if (mtdDevice.empty())
	{
		ErrorMessage = "mtd-device not set";
		return false;
	}

	if( (fd1 = open( mtdDevice.c_str(), O_RDONLY )) < 0 )
	{
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD);
		return false;
	}

	if( (fd2 = open( filename.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR  |  S_IRGRP | S_IWGRP  |  S_IROTH | S_IWOTH)) < 0 )
	{
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENFILE);
		close(fd1);
		return false;
	}

	if(statusViewer)
	{
		globalProgressBegin = statusViewer->getGlobalStatus();
	}
	filesize = CMTDInfo::getInstance()->getMTDSize(mtdDevice);

	char buf[1024];
	long fsize = filesize;
	while(fsize>0)
	{
		long block = fsize;
		if(block>(long)sizeof(buf))
		{
			block = sizeof(buf);
		}
		read( fd1, &buf, block);
		write( fd2, &buf, block);
		fsize -= block;
		char prog = char(100-(100./filesize*fsize));
		if(statusViewer)
		{
			statusViewer->showLocalStatus(prog);
			if(globalProgressEnd!=-1)
			{
				int globalProg = globalProgressBegin + int((globalProgressEnd-globalProgressBegin) * prog/100. );
				statusViewer->showGlobalStatus(globalProg);
			}
		}
	}

	if(statusViewer)
	{
		statusViewer->showLocalStatus(100);
	}

	close(fd1);
	close(fd2);
	return true;
}

bool CFlashTool::program( const std::string & filename, int globalProgressEndErase, int globalProgressEndFlash )
{
	int		fd1, fd2;
	long	filesize;
	int		globalProgressBegin = 0;

	if(statusViewer)
	{
		statusViewer->showLocalStatus(0);
	}

	if (mtdDevice.empty())
	{
		ErrorMessage = "mtd-device not set";
		return false;
	}

	if( (fd1 = open( filename.c_str(), O_RDONLY )) < 0 )
	{
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENFILE);
		return false;
	}

	filesize = lseek( fd1, 0, SEEK_END);
	lseek( fd1, 0, SEEK_SET);

	if(filesize==0)
	{
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_FILEIS0BYTES);
		close(fd1);
		return false;
	}

	if(statusViewer)
	{
		statusViewer->showLocalStatus(0);
		statusViewer->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_ERASING)); // UTF-8
	}
	//g_Zapit->shutdown(); sleep(2);
	if(!erase(globalProgressEndErase))
	{
		close(fd1);
		return false;
	}

	if(statusViewer)
	{
		if(globalProgressEndErase!=-1)
		{
			statusViewer->showGlobalStatus(globalProgressEndErase);
		}
		statusViewer->showLocalStatus(0);
		statusViewer->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_PROGRAMMINGFLASH)); // UTF-8
	}

	if( (fd2 = open( mtdDevice.c_str(), O_WRONLY )) < 0 )
	{
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD);
		close(fd1);
		return false;
	}

	if(statusViewer)
	{
		globalProgressBegin = statusViewer->getGlobalStatus();
	}

	char buf[1024];
	long fsize = filesize;
	while(fsize>0)
	{
		long block = fsize;
		if(block>(long)sizeof(buf))
		{
			block = sizeof(buf);
		}
		read( fd1, &buf, block);
		write( fd2, &buf, block);
		fsize -= block;
		char prog = char(100-(100./filesize*fsize));
		if(statusViewer)
		{
			statusViewer->showLocalStatus(prog);
			if(globalProgressEndFlash!=-1)
			{
				int globalProg = globalProgressBegin + int((globalProgressEndFlash-globalProgressBegin) * prog/100. );
				statusViewer->showGlobalStatus(globalProg);
			}
		}
	}

	if(statusViewer)
	{
		statusViewer->showLocalStatus(100);
	}

	close(fd1);
	close(fd2);
	return true;
}

bool CFlashTool::erase(int globalProgressEnd)
{
	int				fd;
	mtd_info_t		meminfo;
	erase_info_t	lerase;
	int				globalProgressBegin = 0;

	if( (fd = open( mtdDevice.c_str(), O_RDWR )) < 0 )
	{
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD);
		return false;
	}

	if( ioctl( fd, MEMGETINFO, &meminfo ) != 0 )
	{
	// TODO: localize error message
		ErrorMessage = "can't get mtd-info";
		close(fd);
		return false;
	}

	if(statusViewer)
	{
		globalProgressBegin = statusViewer->getGlobalStatus();
	}

	lerase.length = meminfo.erasesize;
	for (lerase.start = 0; lerase.start < meminfo.size;lerase.start += meminfo.erasesize)
	{
		printf( "Erasing %s erase size %x start %x size %x\n",
		                 mtdDevice.c_str(), meminfo.erasesize, lerase.start,
		                 meminfo.size );
		printf( "\rErasing %u Kbyte @ %x -- %2u %% complete.",
		                 meminfo.erasesize/1024, lerase.start,
		                 lerase.start*100/meminfo.size );
		if(statusViewer)
		{
			int prog = int(lerase.start*100./meminfo.size);
			statusViewer->showLocalStatus(prog);
			if(globalProgressEnd!=-1)
			{
				int globalProg = globalProgressBegin + int((globalProgressEnd-globalProgressBegin) * prog/100. );
				statusViewer->showGlobalStatus(globalProg);
			}
		}

		if(ioctl( fd, MEMERASE, &lerase) != 0)
		{
			ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_ERASEFAILED);
			close(fd);
			return false;
		}
	}

	close(fd);
	return true;
}

bool CFlashTool::check_cramfs( const std::string & /*filename*/ )
{
	int retVal =  0; //cramfs_crc( (char*) filename.c_str() );
	printf("flashcheck returned: %d\n", retVal);
	return retVal==1;
}
#define FROMHEX(c) ((c)>='a' ? (c)-'a'+10 : ((c)>='A' ? (c)-'A'+10 : (c)-'0'))
bool CFlashTool::check_md5( const std::string & filename, const std::string & smd5)
{
	unsigned char md5[16];
	unsigned char omd5[16];

	const char * ptr = smd5.c_str();

	if(strlen(ptr) < 32)
		return false;
//printf("[flashtool] check file %s md5 %s\n", filename.c_str(), ptr);

	for(int i = 0; i < 16; i++)
		omd5[i] = FROMHEX(ptr[i*2])*16 + FROMHEX(ptr[i*2+1]);

	md5_file(filename.c_str(), 1, md5);
	if(memcmp(md5, omd5, 16))
		return false;
	return true;
}

void CFlashTool::reboot()
{
	::sync();
	::reboot(RB_AUTOBOOT);
	::exit(0);
}

//-----------------------------------------------------------------------------------------------------------------
CFlashVersionInfo::CFlashVersionInfo(const std::string & versionString)
{
	//SBBBYYYYMMTTHHMM -- formatsting

	// recover type
	snapshot = versionString[0];

	// recover release cycle version
	releaseCycle[0] = versionString[1];
	releaseCycle[1] = '.';
/*
	if (versionString[2] == '0')
	{
		releaseCycle[2] = versionString[3];
		releaseCycle[3] = 0;
	}
	else
*/
	{
		releaseCycle[2] = versionString[2];
		releaseCycle[3] = versionString[3];
		releaseCycle[4] = 0;
	}

	version = atoi(&releaseCycle[0]) * 100 + atoi(&releaseCycle[2]);
	// recover date
	date[0] = versionString[10];
	date[1] = versionString[11];
	date[2] = '.';
	date[3] = versionString[8];
	date[4] = versionString[9];
	date[5] = '.';
	date[6] = versionString[4];
	date[7] = versionString[5];
	date[8] = versionString[6];
	date[9] = versionString[7];
	date[10] = 0;

	// recover time stamp
	time[0] = versionString[12];
	time[1] = versionString[13];
	time[2] = ':';
	time[3] = versionString[14];
	time[4] = versionString[15];
	time[5] = 0;
}

const char *CFlashVersionInfo::getDate(void) const
{
	return date;
}

const char *CFlashVersionInfo::getTime(void) const
{
	return time;
}

const char *CFlashVersionInfo::getReleaseCycle(void) const
{
	return releaseCycle;
}

const char *CFlashVersionInfo::getType(void) const
{
	switch (snapshot)
	{
	case '0':
		return "Release";
	case '1':
		return "Beta";
	case '2':
		return "Internal";
	case 'L':
		return "Locale";
	case 'S':
		return "Settings";
	case 'A':
		return "Addon";
	case 'T':
		return "Text";
	default:
		return "Unknown";
	}
}

int CFlashVersionInfo::getVersion(void) const
{
	return version;
}

//-----------------------------------------------------------------------------------------------------------------

CMTDInfo::CMTDInfo()
{
	getPartitionInfo();
}

CMTDInfo::~CMTDInfo()
{
	for(int x=0;x<getMTDCount();x++)
	{
		delete mtdData[x];
	}
	mtdData.clear();
}


CMTDInfo* CMTDInfo::getInstance()
{
	static CMTDInfo* MTDInfo = NULL;

	if(!MTDInfo)
	{
		MTDInfo = new CMTDInfo();
	}
	return MTDInfo;
}

void CMTDInfo::getPartitionInfo()
{
	FILE* fd = fopen("/proc/mtd", "r");
	if(!fd)
	{
		perror("cannot read /proc/mtd");
		return;
	}
	char buf[1000];
	fgets(buf,sizeof(buf),fd);
	while(!feof(fd))
	{
		if(fgets(buf,sizeof(buf),fd)!=NULL)
		{
			char mtdname[50]="";
			int mtdnr=0;
			int mtdsize=0;
			int mtderasesize=0;
			sscanf(buf, "mtd%d: %x %x \"%s\"\n", &mtdnr, &mtdsize, &mtderasesize, mtdname);
			SMTDPartition* tmp = new SMTDPartition;
				tmp->size = mtdsize;
				tmp->erasesize = mtderasesize;
				std::string tmpstr = buf;
				tmp->name = tmpstr.substr( tmpstr.find('\"')+1, tmpstr.rfind('\"')-tmpstr.find('\"')-1);
				sprintf((char*) &buf, "/dev/mtd%d", mtdnr);
				tmp->filename = buf;
			mtdData.push_back(tmp);
		}
	}
	fclose(fd);
}

int CMTDInfo::getMTDCount()
{
	return mtdData.size();
}

std::string CMTDInfo::getMTDName(const int pos)
{
	// TODO: check /proc/mtd specification to determine mtdname encoding

	return FILESYSTEM_ENCODING_TO_UTF8_STRING(mtdData[pos]->name);
}

std::string CMTDInfo::getMTDFileName(const int pos)
{
	return mtdData[pos]->filename;
}

int CMTDInfo::getMTDSize(const int pos)
{
	return mtdData[pos]->size;
}

int CMTDInfo::getMTDEraseSize(const int pos)
{
	return mtdData[pos]->erasesize;
}

int CMTDInfo::findMTDNumber(const std::string & filename)
{
	for(int x=0;x<getMTDCount();x++)
	{
		if(filename == getMTDFileName(x))
		{
			return x;
		}
	}
	return -1;
}

std::string CMTDInfo::getMTDName(const std::string & filename)
{
	return getMTDName( findMTDNumber(filename) );
}

int CMTDInfo::getMTDSize( const std::string & filename )
{
	return getMTDSize( findMTDNumber(filename) );
}

int CMTDInfo::getMTDEraseSize( const std::string & filename )
{
	return getMTDEraseSize( findMTDNumber(filename) );
}

std::string CMTDInfo::findMTDsystem()
{
	std::string sysfs = "systemFS";

	for(int i = 0; i < getMTDCount(); i++) {
		if(getMTDName(i) == sysfs) {
printf("systemFS: %d dev %s\n", i, getMTDFileName(i).c_str());
			return getMTDFileName(i);
		}
	}
	return "";
}
