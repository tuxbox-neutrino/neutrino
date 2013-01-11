/*******************************************************************************/
/*                                                                             */
/* libcoolstream/audio_cs.h                                                    */
/*   Public header file for audio API                                          */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __AUDIO_CS_H_
#define __AUDIO_CS_H_

#ifndef CS_AUDIO_PDATA
#define CS_AUDIO_PDATA void
#endif

#include "cs_types.h"

typedef enum {
	AUDIO_SYNC_WITH_PTS,
	AUDIO_NO_SYNC,
	AUDIO_SYNC_AUDIO_MASTER
} AUDIO_SYNC_MODE;

typedef enum {
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
	AUDIO_FMT_ADVANCED = AUDIO_FMT_MLP
} AUDIO_FORMAT;

typedef enum {
	HDMI_ENCODED_OFF,
	HDMI_ENCODED_AUTO,
	HDMI_ENCODED_FORCED
} HDMI_ENCODED_MODE;

typedef enum
{
	HDMI_AUDIO_FMT_LPCM  = 0x1,
	HDMI_AUDIO_FMT_AC3        ,
	HDMI_AUDIO_FMT_MPEG1      ,
	HDMI_AUDIO_FMT_MP3        ,
	HDMI_AUDIO_FMT_MPEG2      ,
	HDMI_AUDIO_FMT_AAC        ,
	HDMI_AUDIO_FMT_DTS        ,
	HDMI_AUDIO_FMT_ATRAC
} HDMI_AUDIO_FORMAT;

#define CS_MAX_AUDIO_DECODERS	1
#define CS_MAX_AUDIO_FORMATS 10

typedef struct cs_audio_format {
	HDMI_AUDIO_FORMAT format;
	unsigned int max_channels;
} cs_audio_format_t;

typedef struct cs_audio_caps {
	unsigned char count;
	cs_audio_format_t formats[CS_MAX_AUDIO_FORMATS];
} cs_audio_caps_t;

class cDemux;
class cVideo;

class cAudio {
private:
	static cAudio		*instance[CS_MAX_AUDIO_DECODERS];
	unsigned int		unit;
	cDemux			*demux;
	cVideo			*video;
	CS_AUDIO_PDATA	*privateData;
	//unsigned int		cEncodedDataOnSPDIF, cEncodedDataOnHDMI;
	bool			muted;

	AUDIO_FORMAT		streamType;
	AUDIO_SYNC_MODE		syncMode;
	bool			started;
	unsigned int		uAudioPTSDelay;
	unsigned int		uAudioDolbyPTSDelay, uAudioMpegPTSDelay;
	bool			receivedDelay;

	/* internal methods */
	int setBypassMode(bool Disable);
	int LipsyncAdjust(void);
	int atten;
	int volume;

	bool clip_started;
	HDMI_ENCODED_MODE hdmiDD;
	bool spdifDD;
	bool hasMuteScheduled;
	bool analogOut;
	//
	cAudio(unsigned int Unit);
public:
	/* construct & destruct */
	cAudio(void *hBuffer, void *encHD, void *encSD);
	~cAudio(void);

	void *GetHandle(void);
	void *GetDSP(void);
	void HandleAudioMessage(int Event, void *pData);
	void HandlePcmMessage(int Event, void *pData);
	/* shut up */
	int mute(void);
	int unmute(void);
	int SetMute(bool Enable);

	/* bypass audio to external decoder */
	int enableBypass(void);
	int disableBypass(void);

	/* volume, min = 0, max = 255 */
	int setVolume(unsigned int Left, unsigned int Right);
	int getVolume(void) { return volume;}
	bool getMuteStatus(void) { return muted; }

	/* start and stop audio */
	int Start(void);
	int Stop(void);
	bool Pause(bool Pcm = true);
	bool Resume(bool Pcm = true);
	void SetStreamType(AUDIO_FORMAT StreamType) { streamType = StreamType; };
	AUDIO_FORMAT GetStreamType(void) { return streamType; }
	bool ReceivedAudioDelay(void) { return receivedDelay; }
	void SetReceivedAudioDelay(bool Set = false) { receivedDelay = Set; }
	unsigned int GetAudioDelay(void) { return (streamType == AUDIO_FMT_DOLBY_DIGITAL) ? uAudioDolbyPTSDelay : uAudioMpegPTSDelay; }
	void SetSyncMode(AVSYNC_TYPE SyncMode);

	/* stream source */
	int getSource(void);
	int setSource(int Source);
	int Flush(void);

	/* select channels */
	int setChannel(int Channel);
	int getChannel(void);
	int PrepareClipPlay(int uNoOfChannels, int uSampleRate, int uBitsPerSample, int bLittleEndian);
	int WriteClip(unsigned char *Buffer, int Size);
	int StopClip(void);
	void getAudioInfo(int &Type, int &Layer, int &Freq, int &Bitrate, int &Mode);
	void SetSRS(int iq_enable, int nmgr_enable, int iq_mode, int iq_level);
	bool IsHdmiDDSupported(void);
	void SetHdmiDD(bool On);
	void SetSpdifDD(bool Enable);
	void ScheduleMute(bool On);
	void EnableAnalogOut(bool Enable);
	bool GetHdmiAudioCaps(cs_audio_caps_t &caps);
	bool IsHdmiAudioFormatSupported(HDMI_AUDIO_FORMAT format);
	void SetHdmiDD(HDMI_ENCODED_MODE type);
	bool IsHdmiDTSSupported(void);
	void SetDemux(cDemux *Demux);
	void SetVideo(cVideo *Video);
	static cAudio *GetDecoder(unsigned int Unit);
};

#endif //__AUDIO_CS_H_
