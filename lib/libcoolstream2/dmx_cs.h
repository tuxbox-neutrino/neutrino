/*******************************************************************************/
/*                                                                             */
/* libcoolstream/dmx_cs.h                                                      */
/*   Public header for demux API                                               */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __DEMUX_CS_H_
#define __DEMUX_CS_H_

#include <string>

#include <video_cs.h>
#include "cs_types.h"

#define DEMUX_POLL_TIMEOUT		0  // timeout in ms
#define MAX_FILTER_LENGTH		12    // maximum number of filters

#ifdef DMX_FILTER_SIZE
#error
#endif
#define DMX_FILTER_SIZE			MAX_FILTER_LENGTH

#define MAX_DMX_UNITS			5

typedef enum {
	DMX_VIDEO_CHANNEL  = 1,
	DMX_AUDIO_CHANNEL,
	DMX_PES_CHANNEL,
	DMX_PSI_CHANNEL,
	DMX_PIP_CHANNEL,
	DMX_TP_CHANNEL,
	DMX_PCR_ONLY_CHANNEL
} DMX_CHANNEL_TYPE;

class cDemuxData;
class cVideo;
class cAudio;

class cDemux {
friend class cVideo;
friend class cAudio;
private:
	DMX_CHANNEL_TYPE	type;
	int			timeout;
	unsigned short		pid;
	AVSYNC_TYPE		syncMode;
	bool			enabled;
	int			unit;

	cDemuxData * dd;
public:
	cDemux(int num = 0);
	~cDemux();
	//
	bool		Open(DMX_CHANNEL_TYPE pes_type, void * hVideoBuffer = NULL, int uBufferSize = 8192);
	void		Close(void);
	bool		Start(bool record = false);
	bool		Stop(void);
	int		Read(unsigned char *buff, int len, int Timeout = 0);

	bool		SetVideoFormat(VIDEO_FORMAT VideoFormat);
	bool		SetSource(int source);

	bool		sectionFilter(unsigned short Pid, const unsigned char * const Tid, const unsigned char * const Mask, int len, int Timeout = DEMUX_POLL_TIMEOUT, const unsigned char * const nMask = NULL);
	bool		AddSectionFilter(unsigned short Pid, const unsigned char * const Filter, const unsigned char * const Mask, int len, const unsigned char * const nMask = NULL);

	bool		pesFilter(const unsigned short Pid);
	bool		addPid(unsigned short Pid);
	void		SetSyncMode(AVSYNC_TYPE SyncMode);
	void		*getBuffer(void);
	void		*getChannel(void);
	void		getSTC(s64 *STC);

	DMX_CHANNEL_TYPE getChannelType(void) { return type; };
	int		getUnit(void) { return unit; };
	unsigned short	GetPID(void) { return pid; }

	int		GetSource();
	static bool	SetSource(int unit, int source);
	static int	GetSource(int unit);
};
#endif //__DMX_CS_H_
