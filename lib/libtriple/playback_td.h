#ifndef __PLAYBACK_TD_H
#define __PLAYBACK_TD_H

#include <inttypes.h>
#include <string>
#include <map>
#include <vector>

/* almost 256kB */
#define INBUF_SIZE (1394 * 188)
#define PESBUF_SIZE (128 * 1024)

typedef enum {
	PLAYMODE_TS = 0,
	PLAYMODE_FILE,
} playmode_t;

typedef enum {
	FILETYPE_UNKNOWN,
	FILETYPE_TS,
	FILETYPE_MPG,
	FILETYPE_VDR
} filetype_t;

typedef enum {
	STATE_STOP,
	STATE_PLAY,
	STATE_PAUSE,
	STATE_FF,
	STATE_REW,
	STATE_INIT
} playstate_t;

typedef struct {
	std::string Name;
	off_t Size;
} filelist_t;

class cPlayback
{
	private:
		uint8_t *inbuf;
		ssize_t inbuf_pos;
		ssize_t inbuf_sync;
		uint8_t *pesbuf;
		ssize_t pesbuf_pos;
		ssize_t inbuf_read(void);
		ssize_t read_ts(void);
		ssize_t read_mpeg(void);

		uint8_t cc[256];

		int in_fd;

		int video_type;
		int playback_speed;
		int mSpeed;
		playmode_t playMode;
		std::vector<filelist_t> filelist; /* for multi-file playback */

		bool filelist_auto_add(void);
		int mf_open(int fileno);
		int mf_close(void);
		off_t mf_lseek(off_t pos);
		off_t mf_getsize(void);
		int curr_fileno;
		off_t curr_pos;
		off_t last_size;
		off_t bytes_per_second;

		uint16_t vpid;
		uint16_t apid;
		bool ac3;
		struct AStream {
			// uint16_t pid;
			bool ac3;
			std::string lang; /* not yet really used */
		};
		std::map<uint16_t, AStream> astreams; /* stores AStream sorted by pid */

		int64_t pts_start;
		int64_t pts_end;
		int64_t _pts_end; /* last good endpts */
		int64_t pts_curr;
		int64_t get_pts(uint8_t *p, bool pes, int bufsize);

		filetype_t filetype;
		playstate_t playstate;

		off_t seek_to_pts(int64_t pts);
		off_t mp_seekSync(off_t pos);
		int64_t get_PES_PTS(uint8_t *buf, int len, bool until_eof);

		pthread_t thread;
		bool thread_started;
	public:
		cPlayback(int num = 0);
		~cPlayback();

		void playthread();

		bool Open(playmode_t PlayMode);
		void Close(void);
		bool Start(char *filename, unsigned short vpid, int vtype, unsigned short apid,
			   int ac3, unsigned int duration);
		bool SetAPid(unsigned short pid, int ac3);
		bool SetSpeed(int speed);
		bool GetSpeed(int &speed) const;
		bool GetPosition(int &position, int &duration);	/* pos: current time in ms, dur: file length in ms */
		bool SetPosition(int position, bool absolute = false);	/* position: jump in ms */
		void FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language);
#if 0
		// Functions that are not used by movieplayer.cpp:
		bool Stop(void);
		bool GetOffset(off64_t &offset);
		bool IsPlaying(void) const { return playing; }
		bool IsEnabled(void) const { return enabled; }
		void * GetHandle(void);
		void * GetDmHandle(void);
		int GetCurrPlaybackSpeed(void) const { return nPlaybackSpeed; }
		void PlaybackNotify (int  Event, void *pData, void *pTag);
		void DMNotify(int Event, void *pTsBuf, void *Tag);
#endif
};
#endif
