/*******************************************************************************/
/*                                                                             */
/* libcoolstream/pwrmngr.h                                                     */
/*   Private header of the Powermanager API                                    */
/*                                                                             */
/* (C) 2010 CoolStream International                                           */
/*                                                                             */
/* $Id::                                                                     $ */
/*******************************************************************************/
#ifndef __PWRMNGR_H_
#define __PWRMNGR_H_

// -- cCpuFreqManager ----------------------------------------------------------

class cCpuFreqManager {
private:
	unsigned long			startCpuFreq;
	unsigned long			delta;
public:
	void Up(void);
	void Down(void);
	void Reset(void);
	//
	bool SetCpuFreq(unsigned long CpuFreq);
	bool SetDelta(unsigned long Delta);
	unsigned long GetCpuFreq(void);
	unsigned long GetDelta(void);
	//
	cCpuFreqManager(void);
	~cCpuFreqManager();
};

// -- cPowerManageger ----------------------------------------------------------

typedef enum {
	PWR_INIT = 1,
	PWR_FULL_ACTIVE, /* all devices/clocks up */
	PWR_ACTIVE_STANDBY,
	PWR_PASSIVE_STANDBY,
	PWR_INVALID
} PWR_STATE;

class cPowerManager {
private:
	bool			init;
	bool			opened;
	PWR_STATE		powerState;
	//
	bool SetState(PWR_STATE PowerState);
public:
	bool Open(void);
	void Close(void);
	//
	bool SetStandby(bool Active, bool Passive);
	//
	cPowerManager(void);
	virtual ~cPowerManager();
};

#endif // __PWRMNGR_H__
