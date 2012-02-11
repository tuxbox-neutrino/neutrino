/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2011 CoolStream International Ltd

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

#ifndef __record_h__
#define __record_h__

#include <string>
#include <map>
#include <list>

#include <sectionsdclient/sectionsdclient.h>
#include <timerdclient/timerdclient.h>

#include <neutrinoMessages.h>
#include <gui/movieinfo.h>
#include <zapit/channel.h>
#include <zapit/client/zapittools.h>

#if HAVE_COOL_HARDWARE
#include <record_cs.h>
#include <driver/vfd.h>
#endif
#if HAVE_TRIPLEDRAGON || USE_STB_HAL
#include <record_td.h>
#include <driver/lcdd.h>
#endif

#include <OpenThreads/Mutex>

#define REC_MAX_APIDS 10
#define FILENAMEBUFFERSIZE 1024
#if HAVE_TRIPLEDRAGON
/* I'm not able to get it to work with more than 1 recording at a time :-( */
#define RECORD_MAX_COUNT 1
#else
#define RECORD_MAX_COUNT 8
#endif

#define TSHIFT_MODE_OFF 0
#define TSHIFT_MODE_TEMPORAER 1
#define TSHIFT_MODE_PERMANET 2
#define TSHIFT_MODE_PAUSE 3

//FIXME
enum record_error_msg_t
{
        RECORD_OK			=  0,
        RECORD_BUSY			= -1,
        RECORD_INVALID_DIRECTORY	= -2,
        RECORD_INVALID_CHANNEL		= -3,
        RECORD_FAILURE			= -4,
};

class CRecordInstance
{
	private:
		typedef struct {
			unsigned short apid;
			unsigned int index;//FIXME not used ?
			bool ac3;
		} APIDDesc;
		typedef std::list<APIDDesc> APIDList;

		t_channel_id	channel_id;
		event_id_t	epgid;
		std::string	epgTitle;
		unsigned char	apidmode;
		time_t		epg_time;
		time_t		start_time;
		bool		StreamVTxtPid;
		bool		StreamPmtPid;
		unsigned short	apids[REC_MAX_APIDS];
		unsigned int	numpids;
		CZapitClient::responseGetPIDs allpids;
		int		recording_id;
		bool		autoshift;

		std::string	Directory;
		char		filename[FILENAMEBUFFERSIZE];
		std::string	rec_stop_msg;

		CMovieInfo *	cMovieInfo;
		MI_MOVIE_INFO *	recMovieInfo;
		cRecord *	record;

		void GetPids(CZapitChannel * channel);
		void ProcessAPIDnames();
		void FilterPids(APIDList & apid_list);
		void FillMovieInfo(CZapitChannel * channel, APIDList & apid_lis);
		record_error_msg_t MakeFileName(CZapitChannel * channel);
		bool SaveXml();
		record_error_msg_t Start(CZapitChannel * channel /*, APIDList &apid_list*/);
		void WaitRecMsg(time_t StartTime, time_t WaitTime);
	public:		
		CRecordInstance(const CTimerd::RecordingInfo * const eventinfo, std::string &dir, bool timeshift = false, bool stream_vtxt_pid = false, bool stream_pmt_pid = false);
		~CRecordInstance();

		record_error_msg_t Record();
		bool Stop(bool remove_event = true);
		bool Update();

		void SetRecordingId(int id) { recording_id = id; };
		int GetRecordingId(void) { return recording_id; };
		std::string GetEpgTitle(void) { return epgTitle; };
		MI_MOVIE_INFO * GetMovieInfo(void) { return recMovieInfo; };
		void GetRecordString(std::string& str);
		const char * GetFileName() { return filename; };
		bool Timeshift() { return autoshift; };
		int tshift_mode;
		void SetStopMessage(const char* text) {rec_stop_msg = text;} ;
};

typedef std::map<t_channel_id, CRecordInstance*> recmap_t;
typedef recmap_t::iterator recmap_iterator_t;

typedef std::list<CTimerd::RecordingInfo *> nextmap_t;
typedef nextmap_t::iterator nextmap_iterator_t;

class CRecordManager : public CMenuTarget, public CChangeObserver
{
	private:
		static CRecordManager *  manager;

		recmap_t	recmap;
		nextmap_t	nextmap;
		std::string	Directory;
		std::string	TimeshiftDirectory;
		bool		StreamVTxtPid;
		bool		StreamPmtPid;
		bool		StopSectionsd;
		int		last_mode;
		bool		autoshift;
		uint32_t	shift_timer;

		OpenThreads::Mutex mutex;
		static OpenThreads::Mutex sm;

		bool CutBackNeutrino(const t_channel_id channel_id, const int mode);
		void RestoreNeutrino(void);
		bool CheckRecording(const CTimerd::RecordingInfo * const eventinfo);
		void StartNextRecording();
		void StopPostProcess();
		CRecordInstance * FindInstance(t_channel_id);
		void SetTimeshiftMode(CRecordInstance * inst=NULL, int mode=TSHIFT_MODE_OFF);

	public:
		enum record_modes_t
		{
			RECMODE_OFF = 0,
			RECMODE_REC = 1,
			RECMODE_TSHIFT = 2,
			RECMODE_REC_TSHIFT = 3,
		};
		
		CRecordManager();
		~CRecordManager();

		static CRecordManager * getInstance();

		bool Record(const CTimerd::RecordingInfo * const eventinfo, const char * dir = NULL, bool timeshift = false);
		bool Record(const t_channel_id channel_id, const char * dir = NULL, bool timeshift = false);
		bool Stop(const t_channel_id channel_id); 
		bool Stop(const CTimerd::RecordingStopInfo * recinfo); 
		bool Update(const t_channel_id channel_id);
		bool ShowMenu(void);
		bool AskToStop(const t_channel_id channel_id);
		int  exec(CMenuTarget* parent, const std::string & actionKey);
		bool StartAutoRecord();
		bool StopAutoRecord();

		MI_MOVIE_INFO * GetMovieInfo(const t_channel_id channel_id);
		const std::string GetFileName(const t_channel_id channel_id);

		bool RunStartScript(void);
		bool RunStopScript(void);

		void Config(const bool stopsectionsd, const bool stream_vtxt_pid, const bool stream_pmt_pid)
		{
			StopSectionsd	= stopsectionsd;
			StreamVTxtPid	= stream_vtxt_pid;
			StreamPmtPid	= stream_pmt_pid;
		};
		void SetDirectory(const char * const directory) { Directory	= directory; };
		void SetTimeshiftDirectory(const char * const directory) { TimeshiftDirectory	= directory; };
		bool RecordingStatus(const t_channel_id channel_id = 0);
		bool TimeshiftOnly();
		bool Timeshift() { return (autoshift || shift_timer); };
		bool SameTransponder(const t_channel_id channel_id);
		int  handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data);
		// old code
		bool ChooseRecDir(std::string &dir);
		bool MountDirectory(const char *recordingDir);
		// mimic old behavior for start/stop menu option chooser, still actual ?
		int recordingstatus;
		bool doGuiRecord();
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
		int GetRecordCount() { return recmap.size(); };
		bool IsTimeshift(t_channel_id channel_id=0);
		void StartTimeshift();
		int GetRecordMode(const t_channel_id channel_id=0);
		bool IsFileRecord(std::string file);
};
#endif
