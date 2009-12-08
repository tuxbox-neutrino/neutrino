/*******************************************************************************/
/*                                                                             */
/* libcoolstream/cszapper/demux.h                                              */
/*   ZAP interface for neutrino frontend                                       */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
#ifndef __RECORD_CS_H
#define __RECORD_CS_H

#include <string>

#ifndef CS_RECORD_PDATA
#define CS_RECORD_PDATA void
#endif

class cRecord
{
	private:
		CS_RECORD_PDATA * privateData;
		bool enabled;
		int num_apids;
		int unit;
		int nRecordFD;

	public:
		cRecord(int num = 0);
		~cRecord();

		bool Open(int numpids);
		void Close(void);
		bool Start(int fd, unsigned short vpid, unsigned short * apids, int numpids);
		bool Stop(void);
		void RecordNotify(int Event, void *pData);
};

#endif
