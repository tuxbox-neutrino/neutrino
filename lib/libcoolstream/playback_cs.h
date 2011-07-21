/*******************************************************************************/
/*                                                                             */
/* libcoolstream/playback_cs.h                                                 */
/*   Public header file for playback API                                       */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
#ifndef __PLAYBACK_CS_H_
#define __PLAYBACK_CS_H_

#include <string>

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE
} playmode_t;

class cPlaybackData;

class cPlayback {
private:
	cPlaybackData * pd;

	bool enabled;
	bool paused;
	bool playing;
	int unit;
	int nPlaybackFD;
	int video_type;
	int mSpeed;
	playmode_t playMode;
	//
	void Attach(void);
	void Detach(void);
	bool SetAVDemuxChannel(bool On, bool Video = true, bool Audio = true);
public:
	cPlayback(int num = 0);
	~cPlayback();

	bool Open(playmode_t PlayMode);
	void Close(void);
	bool Start(char * filename, unsigned short vpid, int vtype, unsigned short apid, int audio_flag, unsigned int duration = 0);
	bool Stop(void);
	bool SetAPid(unsigned short pid, int audio_flag);
	bool SetSpeed(int speed);
	bool GetSpeed(int &speed) const;
	bool GetPosition(int &position, int &duration);
	bool GetOffset(off64_t &offset);
	bool SetPosition(int position, bool absolute = false);
	bool IsPlaying(void) const { return playing; }
	bool IsEnabled(void) const { return enabled; }
	void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);

};

#endif // __PLAYBACK_CS_H_
