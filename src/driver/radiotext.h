/*
	$Id: radiotext.h,v 1.4 2009/10/31 10:11:02 seife Exp $
	
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

	ripped from:
*/

/*
 * radioaudio.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * This is a "plugin" for the Video Disk Recorder (VDR).
 *
 * Written by:                  Lars Tegeler <email@host.dom>
 *
 * Project's homepage:          www.math.uni-paderborn.de/~tegeler/vdr
 *
 * Latest version available at: URL
 *
 * See the file COPYING for license information.
 *
 * Description:
 *
 * This Plugin display an background image while the vdr is switcht to radio channels.
 *
*/

#ifndef __RADIO_AUDIO_H
#define __RADIO_AUDIO_H

#include <hardware/dmx.h>
#include <OpenThreads/Thread>
#include <OpenThreads/Condition>
#include <sigc++/signal.h>

typedef unsigned char uchar;
typedef unsigned int uint;

extern const char *ConfigDir;
extern const char *DataDir;
extern char *ReplayFile;

#define RT_MEL 65
#define tr(a) a

class CRadioText : public OpenThreads::Thread, public sigc::trackable
{

private:
	bool enabled;
	bool have_radiotext;
	char *imagepath;
	bool imageShown;
	int imagedelay;
	void send_pes_packet(unsigned char *data, int len, int timestamp);
	void ShowImage (const char *file);
	int first_packets;

	//Radiotext
	void RadioStatusMsg(void);
	bool DividePes(unsigned char *data, int length, int *substart, int *subend);

	uint pid;

	OpenThreads::Mutex mutex;
	OpenThreads::Mutex pidmutex;
	OpenThreads::Condition cond;
	bool running;

	void run();
	void init();
public:
	CRadioText(void);
	~CRadioText(void);
	int  PES_Receive(unsigned char *data, int len);
	void EnableRadioTextProcessing(const char *Titel, bool replay = false);
	void DisableRadioTextProcessing();
	void RadiotextDecode(uchar *Data, int Length);
	void RDS_PsPtynDecode(bool PTYN, uchar *Data, int Length);
	void ShowText(void);
	char* ptynr2string(int nr);
	char *rds_entitychar(char *text);

	void setPid(uint inPid);
	uint getPid(){ return pid; }

	void radiotext_stop(void);
	bool haveRadiotext(void) {return have_radiotext; }
	sigc::signal<void> OnAfterDecodeLine;

	cDemux *audioDemux;

	//Setup-Params
	int S_RtFunc;
	int S_RtOsd;
	int S_RtOsdTitle;
	int S_RtOsdTags;
	int S_RtOsdPos;
	int S_RtOsdRows;
	int S_RtOsdLoop;
	int S_RtOsdTO;
	int S_RtSkinColor;
	int S_RtBgCol;
	int S_RtBgTra;
	int S_RtFgCol;
	int S_RtDispl;
	int S_RtMsgItems;
	int S_Verbose;

	// Radiotext
	int RTP_ItemToggle, RTP_TToggle;
	bool RT_MsgShow, RT_PlusShow;
	bool RT_Replay, RT_ReOpen;
	char RT_Text[5][RT_MEL];
	char RTP_Artist[RT_MEL], RTP_Title[RT_MEL];
	int RT_Info, RT_Index, RT_PTY;
	time_t RTP_Starttime;
	bool RT_OsdTO, RTplus_Osd;
	int RT_OsdTOTemp;
	char RDS_PTYN[9];
	char *RT_Titel, *RTp_Titel;
};

// Radiotext-Memory
#define MAX_RTPC 50
struct rtp_classes {
    time_t start;
    char temptext[RT_MEL];
    char *radiotext[2*MAX_RTPC];
    int rt_Index;
    // Item
    bool item_New;
    char *item_Title[MAX_RTPC];		// 1
    char *item_Artist[MAX_RTPC];	// 4	
    time_t item_Start[MAX_RTPC];
    int item_Index;
    // Info
    char *info_News;			// 12
    char *info_NewsLocal;		// 13
    char *info_Stock[MAX_RTPC];		// 14
    int info_StockIndex;
    char *info_Sport[MAX_RTPC];		// 15
    int info_SportIndex;
    char *info_Lottery[MAX_RTPC];	// 16
    int info_LotteryIndex;
    char *info_DateTime;		// 24
    char *info_Weather[MAX_RTPC];	// 25
    int info_WeatherIndex;
    char *info_Traffic;			// 26
    char *info_Alarm;			// 27
    char *info_Advert;			// 28
    char *info_Url;			// 29
    char *info_Other[MAX_RTPC];		// 30
    int info_OtherIndex;
    // Programme
    char *prog_Station;			// 31
    char *prog_Now;			// 32
    char *prog_Next;			// 33
    char *prog_Part;			// 34
    char *prog_Host;			// 35
    char *prog_EditStaff;		// 36
    char *prog_Homepage;		// 38
    // Interactivity
    char *phone_Hotline;		// 39
    char *phone_Studio;			// 40
    char *email_Hotline;		// 44
    char *email_Studio;			// 45
// to be continue...
};

#endif //__RADIO_AUDIO_H
