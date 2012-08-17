/*******************************************************************************/
/*                                                                             */
/* libcoolstream/record_cs.h                                                   */
/*   Public header file for record API                                         */
/*                                                                             */
/* (C) 2008 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __RECORD_CS_H_
#define __RECORD_CS_H_

#include <string>

class cRecordData;

class cRecord {
private:
	cRecordData * rd;
	bool enabled;
	int unit;

public:
	cRecord(int num = 0);
	~cRecord();

	bool Open();
	void Close(void);
	bool Start(int fd, unsigned short vpid, unsigned short * apids, int numapids, uint64_t chid);
	bool Stop(void);
	bool AddPid(unsigned short pid);
};

#endif // __RECORD_CS_H_
