/*******************************************************************************/
/*                                                                             */
/* libcoolstream/playback_cs.h                                                 */
/*   Public header file for playback API                                       */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __PLAYBACK_CS_H_
#define __PLAYBACK_CS_H_

#include <string>
#include <stdint.h>
#include <vector>

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE
} playmode_t;

class cPlaybackData;

typedef struct {
	bool enabled;
	uint16_t pid;
	uint16_t ac3flags;
	std::string lang;
	std::string codec_name;
} playback_audio_pid_info_t;

#define MAX_PLAYBACK_PIDS 40

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
	bool Start(std::string filename, std::string headers = "");
	bool Stop(void);
	bool SetAPid(unsigned short pid, int audio_flag);
	bool SetSubtitlePid(int /*pid*/){return true;}
	bool SetSpeed(int speed);
	bool GetSpeed(int &speed) const;
	bool GetPosition(int &position, int &duration);
	bool GetOffset(off64_t &offset);
	bool SetPosition(int position, bool absolute = false);
	bool IsPlaying(void) const { return playing; }
	bool IsEnabled(void) const { return enabled; }
	void FindAllPids(playback_audio_pid_info_t *audiopids, uint16_t size, uint16_t *numpida);
	void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
	void FindAllSubs(uint16_t *pids, unsigned short *supported, uint16_t *numpida, std::string *language);
	bool SelectSubtitles(int pid, std::string charset = "");
	void GetChapters(std::vector<int> &positions, std::vector<std::string> &titles);
	void RequestAbort();
	void GetTitles(std::vector<int> &playlists, std::vector<std::string> &titles, int &current);
	void SetTitle(int title);
	uint64_t GetReadCount(void);
	void GetMetadata(std::vector<std::string> &keys, std::vector<std::string> &values);
};

#endif // __PLAYBACK_CS_H_
