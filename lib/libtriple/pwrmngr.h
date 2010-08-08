#ifndef __PWRMNGR_H__
#define __PWRMNGR_H__

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

};

// -- cPowerManageger ----------------------------------------------------------

typedef enum
{
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
	static void ApplicationCallback(void *, void *, signed long, void *, void *) {}
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
