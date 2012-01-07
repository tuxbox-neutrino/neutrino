/* public header file */

#ifndef _AUDIO_TD_H_
#define _AUDIO_TD_H_

#include <hardware/aud/aud_inf.h>

typedef enum
{
  AUDIO_SYNC_WITH_PTS,
  AUDIO_NO_SYNC,
  AUDIO_SYNC_AUDIO_MASTER
} AUDIO_SYNC_MODE;

typedef enum
{
   AUDIO_FMT_AUTO = 0,
   AUDIO_FMT_MPEG,
   AUDIO_FMT_MP3,
   AUDIO_FMT_DOLBY_DIGITAL,
   AUDIO_FMT_BASIC = AUDIO_FMT_DOLBY_DIGITAL,
   AUDIO_FMT_AAC,
   AUDIO_FMT_AAC_PLUS,
   AUDIO_FMT_DD_PLUS,
   AUDIO_FMT_DTS,
   AUDIO_FMT_AVS,
   AUDIO_FMT_MLP,
   AUDIO_FMT_WMA,
   AUDIO_FMT_MPG1, // TD only. For Movieplayer / cPlayback
   AUDIO_FMT_ADVANCED = AUDIO_FMT_MLP
} AUDIO_FORMAT;

class cAudio
{
	private:
		int fd;
		bool Muted;

		int clipfd; /* for pcm playback */
		int mixer_fd;  /* if we are using the OSS mixer */
		int mixer_num; /* oss mixer to use, if any */

		AUDIO_FORMAT	StreamType;
		AUDIO_SYNC_MODE    SyncMode;
		bool started;

		int volume;

		void openDevice(void);
		void closeDevice(void);

		int do_mute(bool enable, bool remember);
		void setBypassMode(bool disable);
	public:
		/* construct & destruct */
		cAudio(void *, void *, void *);
		~cAudio(void);

		void *GetHandle() { return NULL; };
		/* shut up */
		int mute(bool remember = true) { return do_mute(true, remember); };
		int unmute(bool remember = true) { return do_mute(false, remember); };

		/* volume, min = 0, max = 255 */
		int setVolume(unsigned int left, unsigned int right);
		int getVolume(void) { return volume;}
		bool getMuteStatus(void) { return Muted; };

		/* start and stop audio */
		int Start(void);
		int Stop(void);
		bool Pause(bool Pcm = true);
		void SetStreamType(AUDIO_FORMAT type);
#define AVSYNC_TYPE int
		void SetSyncMode(AVSYNC_TYPE Mode);

		/* select channels */
		int setChannel(int channel);
		int PrepareClipPlay(int uNoOfChannels, int uSampleRate, int uBitsPerSample, int bLittleEndian);
		int WriteClip(unsigned char * buffer, int size);
		int StopClip();
		void getAudioInfo(int &type, int &layer, int& freq, int &bitrate, int &mode);
		void SetSRS(int iq_enable, int nmgr_enable, int iq_mode, int iq_level);
		bool IsHdmiDDSupported() { return false; };
		void SetHdmiDD(bool) { return; };
		void SetSpdifDD(bool enable);
		void ScheduleMute(bool On);
		void EnableAnalogOut(bool enable);
};

#endif

