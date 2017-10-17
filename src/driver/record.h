/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2011 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

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

#include <timerdclient/timerdtypes.h>

#include <neutrinoMessages.h>
#include <driver/movieinfo.h>

#if HAVE_COOL_HARDWARE
#include <record_cs.h>
#endif
#if USE_STB_HAL
#include <record_hal.h>
#endif

#include <OpenThreads/Mutex>
#include <OpenThreads/Thread>

#if 0
extern "C" {
#include <libavformat/avformat.h>
}
#endif

#define REC_MAX_APIDS 10
#define FILENAMEBUFFERSIZE 1024
#if HAVE_TRIPLEDRAGON
/* I'm not able to get it to work with more than 1 recording at a time :-( */
#define RECORD_MAX_COUNT 1
#else
#define RECORD_MAX_COUNT 8
#endif

#define TSHIFT_MODE_OFF		0
#define TSHIFT_MODE_ON		1
#define TSHIFT_MODE_PAUSE	2
#define TSHIFT_MODE_REWIND	3

class CFrontend;
class CZapitChannel;

//FIXME
enum record_error_msg_t
{
        RECORD_OK			=  0,
        RECORD_BUSY			= -1,
        RECORD_INVALID_DIRECTORY	= -2,
        RECORD_INVALID_CHANNEL		= -3,
        RECORD_FAILURE			= -4
};

class CRecordInstance
{
	protected:
		typedef struct {
			uint32_t apid;
			unsigned int index;//FIXME not used ?
			bool ac3;
		} APIDDesc;
		typedef std::list<APIDDesc> APIDList;

		t_channel_id	channel_id;
		event_id_t	epgid;
		std::string	epgTitle;
		std::string	epgInfo1;
		unsigned char	apidmode;
		time_t		epg_time;
		time_t		start_time;
		bool		StreamVTxtPid;
		bool		StreamSubtitlePids;
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

		virtual void GetPids(CZapitChannel * channel);
		virtual void FillMovieInfo(CZapitChannel * channel, APIDList & apid_list);
		record_error_msg_t Start(CZapitChannel * channel);
		void ProcessAPIDnames();
		void FilterPids(APIDList & apid_list);
		record_error_msg_t MakeFileName(CZapitChannel * channel);
		bool SaveXml();
		void WaitRecMsg(time_t StartTime, time_t WaitTime);
		void MakeExtFileName(CZapitChannel * channel, std::string &FilenameTemplate);
		void StringReplace(std::string &str, const std::string search, const std::string rstr);
	public:
		CRecordInstance(const CTimerd::RecordingInfo * const eventinfo, std::string &dir, bool timeshift = false, bool stream_vtxt_pid = false, bool stream_pmt_pid = false, bool stream_subtitle_pids = false);
		virtual ~CRecordInstance();

		virtual record_error_msg_t Record();
		virtual bool Stop(bool remove_event = true);
		bool Update();

		void SetRecordingId(int id) { recording_id = id; };
		int GetRecordingId(void) { return recording_id; };
		t_channel_id GetChannelId(void) { return channel_id; };
		std::string GetEpgTitle(void) { return epgTitle; };
		MI_MOVIE_INFO * GetMovieInfo(void) { return recMovieInfo; };
		void GetRecordString(std::string& str, std::string &dur);
		const char * GetFileName() { return filename; };
		bool Timeshift() { return autoshift; };
		int tshift_mode;
		void SetStopMessage(const char* text) {rec_stop_msg = text;} ;
		int  GetStatus();

		CFrontend *	frontend;
};

typedef std::pair<int, CRecordInstance*> recmap_pair_t;
typedef std::map<int, CRecordInstance*> recmap_t;
typedef recmap_t::iterator recmap_iterator_t;

typedef std::list<CTimerd::RecordingInfo *> nextmap_t;
typedef nextmap_t::iterator nextmap_iterator_t;

class CRecordManager : public CMenuTarget /*, public CChangeObserver*/
{
	private:
		static CRecordManager *  manager;

		recmap_t	recmap;
		nextmap_t	nextmap;
		std::string	Directory;
		std::string	TimeshiftDirectory;
		std::vector<std::string> durations;
		bool		StreamVTxtPid;
		bool		StreamSubtitlePids;
		bool		StreamPmtPid;
		bool		StopSectionsd;
		int		last_mode;
		bool		autoshift;
		uint32_t	shift_timer;
		uint32_t	check_timer;
		bool		error_display;
		bool		warn_display;

		OpenThreads::Mutex mutex;
		static OpenThreads::Mutex sm;

		bool CutBackNeutrino(const t_channel_id channel_id, CFrontend * &frontend);
		void RestoreNeutrino(void);
		void StartNextRecording();
		void StopPostProcess();
		void StopInstance(CRecordInstance * inst, bool remove_event = true);
		CRecordInstance * FindInstance(t_channel_id);
		CRecordInstance * FindInstanceID(int recid);
		CRecordInstance * FindTimeshift();

	public:
		enum record_modes_t
		{
			RECMODE_OFF = 0,
			RECMODE_REC = 1,
			RECMODE_TSHIFT = 2,
			RECMODE_REC_TSHIFT = 3
		};
		
		CRecordManager();
		~CRecordManager();

		static CRecordManager * getInstance();

		bool Record(const CTimerd::RecordingInfo * const eventinfo, const char * dir = NULL, bool timeshift = false);
		bool Record(const t_channel_id channel_id, const char * dir = NULL, bool timeshift = false);
		bool Stop(const t_channel_id channel_id); 
		bool Stop(const CTimerd::RecordingStopInfo * recinfo); 
		bool IsRecording(const CTimerd::RecordingStopInfo * recinfo);
		bool Update(const t_channel_id channel_id);
		bool ShowMenu(void);
		bool AskToStop(const t_channel_id channel_id, const int recid = 0);
		int  exec(CMenuTarget* parent, const std::string & actionKey);
		bool StartAutoRecord();
		bool StopAutoRecord(bool lock = true);
		void StopAutoTimer();
		bool CheckRecordingId_if_Timeshift(int recid);
		recmap_t GetRecordMap()const{return recmap;}

		MI_MOVIE_INFO * GetMovieInfo(const t_channel_id channel_id, bool timeshift = true);
		const std::string GetFileName(const t_channel_id channel_id, bool timeshift = true);

		bool RunStartScript(void);
		bool RunStopScript(void);

		void Config(const bool stopsectionsd, const bool stream_vtxt_pid, const bool stream_pmt_pid, bool stream_subtitle_pids )
		{
			StopSectionsd		= stopsectionsd;
			StreamVTxtPid		= stream_vtxt_pid;
			StreamSubtitlePids	= stream_subtitle_pids;
			StreamPmtPid		= stream_pmt_pid;
		};
		void SetDirectory(std::string directory) { Directory	= directory; };
		void SetTimeshiftDirectory(std::string directory) { TimeshiftDirectory	= directory; };
		bool RecordingStatus(const t_channel_id channel_id = 0);
		bool TimeshiftOnly();
		bool Timeshift() { return (autoshift || shift_timer); };
		bool SameTransponder(const t_channel_id channel_id);
		int  handleMsg(const neutrino_msg_t _msg, neutrino_msg_data_t data);
		// mimic old behavior for start/stop menu option chooser, still actual ?
		int GetRecordCount() { return recmap.size(); };
		void StartTimeshift();
		int GetRecordMode(const t_channel_id channel_id=0);
		CRecordInstance* getRecordInstance(std::string file);
		// old code
#if 0
		bool MountDirectory(const char *recordingDir);
		bool ChooseRecDir(std::string &dir);
		int recordingstatus;
		bool doGuiRecord();
		bool changeNotify(const neutrino_locale_t OptionName, void * /*data*/);
#endif
};

#if 0
class CStreamRec : public CRecordInstance, OpenThreads::Thread
{
	private:
		AVFormatContext *ifcx;
		AVFormatContext *ofcx;
		AVBitStreamFilterContext *bsfc;
		bool stopped;
		bool interrupt;
		time_t time_started;
		int  stream_index;

		void GetPids(CZapitChannel * channel);
		void FillMovieInfo(CZapitChannel * channel, APIDList & apid_list);
		bool Start();

		void Close();
		bool Open(CZapitChannel * channel);
		void run();
		void WriteHeader(uint32_t duration);
	public:
		CStreamRec(const CTimerd::RecordingInfo * const eventinfo, std::string &dir, bool timeshift = false, bool stream_vtxt_pid = false, bool stream_pmt_pid = false, bool stream_subtitle_pids = false);
		~CStreamRec();
		record_error_msg_t Record();
		bool Stop(bool remove_event = true);
		static int Interrupt(void * data);
};
#endif

#endif
