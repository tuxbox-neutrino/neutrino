/*******************************************************************************/
/*                                                                             */
/* libcoolstream/cszapper/demux.h                                              */
/*   ZAP interface for neutrino frontend                                       */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
#ifndef __PLAYBACK_CS_H
#define __PLAYBACK_CS_H

#include <string>

#ifndef CS_PLAYBACK_PDATA
#define CS_PLAYBACK_PDATA void
#endif

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t; 

class cPlayback
{
	private:
		int timeout;
		pthread_cond_t read_cond;
		pthread_mutex_t mutex;
		CS_PLAYBACK_PDATA * privateData;
		bool enabled;
		bool paused;
		bool playing;
		int unit;
		int nPlaybackFD;
		int video_type;
		int nPlaybackSpeed;
		int mSpeed;
		playmode_t playMode;
		//
		void Attach(void);
		void Detach(void);
		bool SetAVDemuxChannel(bool On, bool Video = true, bool Audio = true);
	public:
		void PlaybackNotify (int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, bool ac3);
		bool Stop(void);
		bool SetAPid(unsigned short pid, bool ac3);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);
		bool GetOffset(off64_t &offset);
		bool SetPosition(int position, bool absolute = false);
		bool IsPlaying(void) const { return playing; }
		bool IsEnabled(void) const { return enabled; }
		void * GetHandle(void);
		void * GetDmHandle(void);
		int GetCurrPlaybackSpeed(void) const { return nPlaybackSpeed; }
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
		//
		cPlayback(int num = 0);
		~cPlayback();

};

#endif
