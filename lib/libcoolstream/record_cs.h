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

class cRecordData;

class cRecord {
private:
	cRecordData * rd;
	bool enabled;
	int num_apids;
	int unit;
	int nRecordFD;

public:
	cRecord(int num = 0);
	~cRecord();

	bool Open();
	void Close(void);
	bool Start(int fd, unsigned short vpid, unsigned short * apids, int numapids);
	bool Stop(void);
	bool AddPid(unsigned short pid);
	/* not tested */
	bool ChangePids(unsigned short vpid, unsigned short * apids, int numapids);
};

#endif // __RECORD_CS_H_
