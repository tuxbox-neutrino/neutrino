#ifndef __erfmod_h
#define __erfmod_h


class RFmod
{
	int channel,soundsubcarrier,soundenable,finetune,standby;

public:
	RFmod();
	~RFmod();

	int rfmodfd;
	void init();

	int setChannel(int channel);
	int setSoundSubCarrier(int val);
	int setSoundEnable(int val);
	int setStandby(int val);
	int setFinetune(int val);
	int setTestPattern(int val);
};
#endif
