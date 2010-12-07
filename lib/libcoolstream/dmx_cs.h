/*******************************************************************************/
/*                                                                             */
/* libcoolstream/dmx_cs.h                                                      */
/*   Public header for demux API                                               */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
#ifndef __DEMUX_CS_H_
#define __DEMUX_CS_H_

#include <string>

#define DEMUX_POLL_TIMEOUT		0  // timeout in ms
#define MAX_FILTER_LENGTH		12    // maximum number of filters
#ifndef DMX_FILTER_SIZE
#define DMX_FILTER_SIZE			MAX_FILTER_LENGTH
#endif
#define MAX_DMX_UNITS			4 //DMX_NUM_TSS_INPUTS_REVB

#ifndef CS_DMX_PDATA
#define CS_DMX_PDATA void
#endif

#include "cs_types.h"

typedef enum {
	DMX_VIDEO_CHANNEL  = 1,
	DMX_AUDIO_CHANNEL,
	DMX_PES_CHANNEL,
	DMX_PSI_CHANNEL,
	DMX_PIP_CHANNEL,
	DMX_TP_CHANNEL,
	DMX_PCR_ONLY_CHANNEL
} DMX_CHANNEL_TYPE;

class cDemux {
private:
	int timeout;
	unsigned short pid;
	bool nb; // non block
	pthread_cond_t read_cond;
	pthread_mutex_t mutex;
	AVSYNC_TYPE syncMode;
	int uLength;
	bool enabled;
	int unit;

	DMX_CHANNEL_TYPE type;
	CS_DMX_PDATA *privateData;
public:
	cDemux(int num = 0);
	~cDemux();
	//
	bool Open(DMX_CHANNEL_TYPE pes_type, void * hVideoBuffer = NULL, int uBufferSize = 8192);
	void Close(void);
	bool Start(void);
	bool Stop(void);
	int Read(unsigned char *buff, int len, int Timeout = 0);
	void SignalRead(int len);
	unsigned short GetPID(void) { return pid; }
	const unsigned char *GetFilterTID(u8 FilterIndex = 0);
	const unsigned char *GetFilterMask(u8 FilterIndex = 0);
	const unsigned int GetFilterLength(u8 FilterIndex = 0);
	bool AddSectionFilter(unsigned short Pid, const unsigned char * const Filter, const unsigned char * const Mask, int len, const unsigned char * const nMask = NULL);
	bool sectionFilter(unsigned short Pid, const unsigned char * const Tid, const unsigned char * const Mask, int len, int Timeout = DEMUX_POLL_TIMEOUT, const unsigned char * const nMask = NULL);
	bool pesFilter(const unsigned short Pid);
	void SetSyncMode(AVSYNC_TYPE SyncMode);
	void *getBuffer(void);
	void *getChannel(void);
	DMX_CHANNEL_TYPE getChannelType(void);
	void addPid(unsigned short Pid);
	void getSTC(int64_t *STC);
	int getUnit(void);
	//
};
#endif //__DMX_CS_H_
