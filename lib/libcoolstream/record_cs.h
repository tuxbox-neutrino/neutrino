/*******************************************************************************/
/*                                                                             */
/* libcoolstream/record_cs.h                                                   */
/*   Public header file for record API                                         */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/*******************************************************************************/
#ifndef __RECORD_CS_H_
#define __RECORD_CS_H_

#include <string>

#ifndef CS_RECORD_PDATA
#define CS_RECORD_PDATA void
#endif

class cRecord {
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
	bool Start(int fd, unsigned short vpid, unsigned short * apids, int numapids);
	bool Stop(void);
	void RecordNotify(int Event, void *pData);
	/* not tested */
	bool ChangePids(unsigned short vpid, unsigned short * apids, int numapids);
};

#endif // __RECORD_CS_H_
