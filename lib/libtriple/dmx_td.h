#ifndef __DEMUX_TD_H
#define __DEMUX_TD_H

#include <cstdlib>
#include <vector>
extern "C" {
#include <inttypes.h>
#include <sys/ioctl.h>
#include <hardware/xp/xp_osd_user.h>
}
#if defined DMX_FILTER_SIZE
#undef DMX_FILTER_SIZE
#endif
#define DMX_FILTER_SIZE FILTER_LENGTH

typedef enum
{
	DMX_INVALID = 0,
	DMX_VIDEO_CHANNEL = 1,
	DMX_AUDIO_CHANNEL,
	DMX_PES_CHANNEL,
	DMX_PSI_CHANNEL,
	DMX_PIP_CHANNEL,
	DMX_TP_CHANNEL,
	DMX_PCR_ONLY_CHANNEL
} DMX_CHANNEL_TYPE;

typedef struct
{
	int fd;
	unsigned short pid;
} pes_pids;

class cDemux
{
	private:
		int num;
		int fd;
		int buffersize;
		bool measure;
		uint64_t last_measure, last_data;
		DMX_CHANNEL_TYPE dmx_type;
		std::vector<pes_pids> pesfds;
		struct demux_filter_para s_flt;
		demux_pes_para p_flt;
	public:

		bool Open(DMX_CHANNEL_TYPE pes_type, void * x = NULL, int y = 0);
		void Close(void);
		bool Start(bool record = false);
		bool Stop(void);
		int Read(unsigned char *buff, int len, int Timeout = 0);
		bool sectionFilter(unsigned short pid, const unsigned char * const filter, const unsigned char * const mask, int len, int Timeout = 0, const unsigned char * const negmask = NULL);
		bool pesFilter(const unsigned short pid);
#define AVSYNC_TYPE int
		void SetSyncMode(AVSYNC_TYPE mode);
		void * getBuffer();
		void * getChannel();
		DMX_CHANNEL_TYPE getChannelType(void) { return dmx_type; };
		bool addPid(unsigned short pid);
		void getSTC(int64_t * STC);
		int getUnit(void);
		// TD only functions
		int getFD(void) { return fd; };		/* needed by cPlayback class */
		void removePid(unsigned short Pid);	/* needed by cRecord class */
		std::vector<pes_pids> getPesPids(void) { return pesfds; };
		//
		cDemux(int num = 0);
		~cDemux();
};

#endif //__DEMUX_H
