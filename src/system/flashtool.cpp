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
#include <system/fsmounter.h>
#include <system/helpers.h>
#include <eitd/sectionsd.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>

#include <linux/version.h>

#include <global.h>
#include <neutrino.h>
#include <driver/display.h>

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

void CFlashTool::setStatusViewer( CProgressWindow* statusview )
{
	statusViewer = statusview;
}

bool CFlashTool::readFromMTD( const std::string & filename, int globalProgressEnd )
{
	int fd1;
	long filesize;
	int globalProgressBegin = 0;

	if(statusViewer)
		statusViewer->showLocalStatus(0);

	if (mtdDevice.empty()) {
		ErrorMessage = "mtd-device not set";
		return false;
	}

	if( (fd = open( mtdDevice.c_str(), O_RDONLY )) < 0 ) {
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD);
		return false;
	}
	if (!getInfo()) {
		close(fd);
		return false;
	}

	if( (fd1 = open( filename.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR  |  S_IRGRP | S_IWGRP  |  S_IROTH | S_IWOTH)) < 0 ) {
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENFILE);
		close(fd);
		return false;
	}

	if(statusViewer)
		globalProgressBegin = statusViewer->getGlobalStatus();

	filesize = CMTDInfo::getInstance()->getMTDSize(mtdDevice);

	unsigned char buf[meminfo.writesize];
	unsigned mtdoffset = 0;
	long fsize = filesize;
	while(fsize > 0) {
		unsigned block = meminfo.writesize;
		if (isnand) {
			unsigned blockstart = mtdoffset & ~(meminfo.erasesize - 1);
			if (blockstart == mtdoffset) {
				while (mtdoffset < meminfo.size) {
					printf("CFlashTool::readFromMTD: read block at %x\n", mtdoffset);
					loff_t offset = mtdoffset;
					int ret = ioctl(fd, MEMGETBADBLOCK, &offset);
					if (ret == 0)
						break;
					printf("CFlashTool::readFromMTD: bad block at %x, skipping..\n", mtdoffset);
					mtdoffset += meminfo.erasesize;
					fsize -= meminfo.erasesize;
					lseek(fd, mtdoffset, SEEK_SET);
					continue;
				}
				if (mtdoffset >= meminfo.size) {
					printf("CFlashTool::readFromMTD: end of device...\n");
					break;
				}
			}
		}
		read(fd, buf, block);
		write(fd1, buf, block);
		fsize -= block;
		mtdoffset += meminfo.writesize;
		char prog = char(100-(100./filesize*fsize));
		if(statusViewer) {
			statusViewer->showLocalStatus(prog);
			if(globalProgressEnd!=-1) {
				int globalProg = globalProgressBegin + int((globalProgressEnd-globalProgressBegin) * prog/100. );
				statusViewer->showGlobalStatus(globalProg);
			}
		}
	}

	if(statusViewer)
		statusViewer->showLocalStatus(100);

	close(fd);
	close(fd1);
	return true;
}

bool CFlashTool::program( const std::string & filename, int globalProgressEndErase, int globalProgressEndFlash )
{
	int fd1;
	ssize_t filesize;
	int globalProgressBegin = 0;

	if(g_settings.epg_save)
		CNeutrinoApp::getInstance()->saveEpg(true);

	if(statusViewer)
		statusViewer->showLocalStatus(0);

	if (mtdDevice.empty()) {
		ErrorMessage = "mtd-device not set";
		return false;
	}

	char buf1[1024];
	memset(buf1, 0, sizeof(buf1));
	strncpy(buf1, filename.c_str(), sizeof(buf1)-1);
	char* dn = dirname(buf1);
	std::string flashfile;

	bool skipCopy = false;
#ifdef BOXMODEL_CS_HD2
	if (strcmp(dn, "/tmp") != 0) {
		uint64_t btotal = 0, bused = 0;
		long bsize = 0;
		if (get_fs_usage("/tmp", btotal, bused, &bsize)) {
			uint64_t fileSize = (uint64_t)file_size(filename.c_str()) / 1024ULL;
			uint64_t backupMaxSize = (int)((btotal - bused) * bsize);
			uint64_t res = 10; // Reserved 10% of available space
			backupMaxSize = (backupMaxSize - ((backupMaxSize * res) / 100ULL)) / 1024ULL;
			if (backupMaxSize < fileSize)
				skipCopy = true;
		}
		else
			skipCopy = true;
	}
#endif

	if ((strcmp(dn, "/tmp") != 0) && !skipCopy) {
		memset(buf1, 0, sizeof(buf1));
		strncpy(buf1, filename.c_str(), sizeof(buf1)-1);
		flashfile = (std::string)"/tmp/" + basename(buf1);
		CFileHelpers fh;
		printf("##### [CFlashTool::program] copy flashfile to %s\n", flashfile.c_str());
		if(statusViewer)
			statusViewer->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_COPY_IMAGE));
		fh.copyFile(filename.c_str(), flashfile.c_str(), 0644);
		sync();
		if(statusViewer)
			statusViewer->showGlobalStatus(statusViewer->getGlobalStatus()+5);
	}
	else
		flashfile = filename;

	// Unmount all NFS & CIFS volumes
	if (!skipCopy) {
		nfs_mounted_once = false;
		CFSMounter::umount();
	}

	if( (fd1 = open( flashfile.c_str(), O_RDONLY )) < 0 ) {
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENFILE);
		return false;
	}

	filesize = (ssize_t)lseek( fd1, 0, SEEK_END);
	lseek( fd1, 0, SEEK_SET);

	if(filesize==0) {
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_FILEIS0BYTES);
		close(fd1);
		return false;
	}

	if(statusViewer) {
		statusViewer->showLocalStatus(0);
		statusViewer->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_ERASING)); // UTF-8
	}

	if(!erase(globalProgressEndErase)) {
		close(fd1);
		return false;
	}

	if(statusViewer) {
		if(globalProgressEndErase!=-1)
			statusViewer->showGlobalStatus(globalProgressEndErase);
		statusViewer->showLocalStatus(0);
		statusViewer->showStatusMessageUTF(g_Locale->getText(LOCALE_FLASHUPDATE_PROGRAMMINGFLASH)); // UTF-8
	}
#ifndef VFD_UPDATE
	CVFD::getInstance()->ShowText("Write Flash");
#endif

	if( (fd = open( mtdDevice.c_str(), O_WRONLY )) < 0 ) {
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD);
		close(fd1);
		return false;
	}

	if(statusViewer)
		globalProgressBegin = statusViewer->getGlobalStatus();

	unsigned char buf[meminfo.writesize];
	unsigned mtdoffset = 0;
	unsigned fsize = filesize;
	printf("CFlashTool::program: file %s write size %d, erase size %d\n", flashfile.c_str(), meminfo.writesize, meminfo.erasesize);
	while(fsize > 0) {
		unsigned block = meminfo.writesize;
		if (block > fsize)
			block = fsize;

		unsigned res = read(fd1, buf, block);
		if (res != block) {
			printf("CFlashTool::program: read from %s failed: %d from %d\n", flashfile.c_str(), res, block);
		}
		if (isnand) {
			if (block < (unsigned) meminfo.writesize) {
				printf("CFlashTool::program: padding at %x\n", mtdoffset);
				memset(buf + res, 0, meminfo.writesize - res);
			}
			unsigned blockstart = mtdoffset & ~(meminfo.erasesize - 1);
			if (blockstart == mtdoffset) {
				while (mtdoffset < meminfo.size) {
					printf("CFlashTool::program: write block at %x\n", mtdoffset);
					loff_t offset = mtdoffset;
					int ret = ioctl(fd, MEMGETBADBLOCK, &offset);
					if (ret == 0)
						break;
					printf("CFlashTool::program: bad block at %x, skipping..\n", mtdoffset);
					mtdoffset += meminfo.erasesize;
					lseek(fd, mtdoffset, SEEK_SET);
					continue;
				}
				if (mtdoffset >= meminfo.size) {
					printf("CFlashTool::program: not enough space to write, left: %d\n", fsize);
					break;
				}
			}
		}
		write(fd, buf, meminfo.writesize);
		fsize -= block;
		mtdoffset += meminfo.writesize;
		if(statusViewer) {
			char prog = char(100-(100./filesize*fsize));
			statusViewer->showLocalStatus(prog);
			if(globalProgressEndFlash!=-1) {
				int globalProg = globalProgressBegin + int((globalProgressEndFlash-globalProgressBegin) * prog/100. );
				statusViewer->showGlobalStatus(globalProg);
			}
		}
	}

	if(statusViewer)
		statusViewer->showLocalStatus(100);

	close(fd1);
	close(fd);
	// FIXME error message
	if (fsize)
		return false;
	CVFD::getInstance()->ShowText("Flash OK.");
	return true;
}

bool CFlashTool::getInfo()
{
	if (ioctl( fd, MEMGETINFO, &meminfo ) != 0 ) {
		// TODO: localize error message
		ErrorMessage = "can't get mtd-info";
		return false;
	}
	if (meminfo.writesize < 1024)
		meminfo.writesize = 1024;

	isnand = (meminfo.type == MTD_NANDFLASH);
	printf("CFlashTool::getInfo: NAND: %s writesize %d\n", isnand ? "yes" : "no", meminfo.writesize);
	return true;
}

bool CFlashTool::erase(int globalProgressEnd)
{
	erase_info_t lerase;
	int globalProgressBegin = 0;

	if( (fd = open( mtdDevice.c_str(), O_RDWR )) < 0 )
	{
		printf("CFlashTool::erase: cant open %s\n", mtdDevice.c_str());
		ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_CANTOPENMTD);
		return false;
	}

	if (!getInfo()) {
		close(fd);
		return false;
	}

	CNeutrinoApp::getInstance()->stopDaemonsForFlash();

#ifndef VFD_UPDATE
	CVFD::getInstance()->ShowText("Erase Flash");
#endif

	if(statusViewer) {
		globalProgressBegin = statusViewer->getGlobalStatus();
		statusViewer->paint();
		statusViewer->showLocalStatus(0);
		statusViewer->showGlobalStatus(globalProgressBegin);
	}

	lerase.length = meminfo.erasesize;

	for (lerase.start = 0; lerase.start < meminfo.size; lerase.start += meminfo.erasesize)
	{
		/* printf( "Erasing %s erase size %x start %x size %x\n",
		                 mtdDevice.c_str(), meminfo.erasesize, lerase.start,
		                 meminfo.size ); */
		int prog = int(lerase.start*100./meminfo.size);
		if(statusViewer)
		{
			statusViewer->showLocalStatus(prog);
			if(globalProgressEnd!=-1)
			{
				int globalProg = globalProgressBegin + int((globalProgressEnd-globalProgressBegin) * prog/100. );
				statusViewer->showGlobalStatus(globalProg);
			}
		}
		if (isnand) {
			loff_t offset = lerase.start;
			int ret = ioctl(fd, MEMGETBADBLOCK, &offset);
			if (ret > 0) {
				printf("Erasing: bad block at %x, skipping..\n", lerase.start);
				continue;
			}
		}
		if(ioctl( fd, MEMERASE, &lerase) != 0)
		{
			ErrorMessage = g_Locale->getText(LOCALE_FLASHUPDATE_ERASEFAILED);
			close(fd);
			return false;
		}
		printf( "Erasing %u Kbyte @ 0x%08X -- %2u %% complete.\n", meminfo.erasesize/1024, lerase.start, prog);
	}
	printf("\n");

	close(fd);
	return true;
}
#if 0 
//never used
bool CFlashTool::check_cramfs( const std::string & /*filename*/ )
{
	int retVal =  0; //cramfs_crc( (char*) filename.c_str() );
	printf("flashcheck returned: %d\n", retVal);
	return retVal==1;
}
#endif
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
		omd5[i] = (unsigned char)(FROMHEX(ptr[i*2])*16 + FROMHEX(ptr[i*2+1]));

	md5_file(filename.c_str(), 1, md5);
	if(memcmp(md5, omd5, 16))
		return false;
	return true;
}

void CFlashTool::reboot()
{
	::reboot(RB_AUTOBOOT);
	::exit(0);
}

//-----------------------------------------------------------------------------------------------------------------
CFlashVersionInfo::CFlashVersionInfo(const std::string & _versionString)
{
	//SBBBYYYYMMTTHHMM -- formatsting
	std::string versionString = _versionString;
	/* just to make sure the string is long enough for the following code
	 * trailing chars don't matter -- will just be ignored */
	if (versionString.size() < 16)
		versionString.append(16, '0');
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
	struct tm tt;
	memset(&tt, 0, sizeof(tt));
	date[0] = versionString[10];
	date[1] = versionString[11];
	date[2] = '.';
	tt.tm_mday = atoi(&date[0]);

	date[3] = versionString[8];
	date[4] = versionString[9];
	date[5] = '.';
	tt.tm_mon = atoi(&date[3]) - 1;

	date[6] = versionString[4];
	date[7] = versionString[5];
	date[8] = versionString[6];
	date[9] = versionString[7];
	date[10] = 0;
	tt.tm_year = atoi(&date[6]) - 1900;

	// recover time stamp
	time[0] = versionString[12];
	time[1] = versionString[13];
	time[2] = ':';
	tt.tm_hour = atoi(&time[0]);

	time[3] = versionString[14];
	time[4] = versionString[15];
	time[5] = 0;
	tt.tm_min = atoi(&time[3]);

	datetime = mktime(&tt);
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
	// TODO: localize it

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
	case 'U':
		return "Update";
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

	if (pos >= 0)
		return FILESYSTEM_ENCODING_TO_UTF8_STRING(mtdData[pos]->name);
	return "";
}

std::string CMTDInfo::getMTDFileName(const int pos)
{
	if (pos >= 0)
		return mtdData[pos]->filename;
	return "";
}

int CMTDInfo::getMTDSize(const int pos)
{
	if (pos >= 0)
		return mtdData[pos]->size;
	return 0;
}

int CMTDInfo::getMTDEraseSize(const int pos)
{
	if (pos >= 0)
		return mtdData[pos]->erasesize;
	return 0;
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

int CMTDInfo::findMTDNumberFromName(const char* name)
{
	for (int i = 0; i < getMTDCount(); i++) {
		if ((std::string)name == getMTDName(i))
			return i;
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
#ifdef BOXMODEL_CS_HD2
	std::string sysfs = "root0";
#else
	std::string sysfs = "systemFS";
#endif

	for(int i = 0; i < getMTDCount(); i++) {
		if(getMTDName(i) == sysfs) {
printf("systemFS: %d dev %s\n", i, getMTDFileName(i).c_str());
			return getMTDFileName(i);
		}
	}
	return "";
}
