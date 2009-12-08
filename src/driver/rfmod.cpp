#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <global.h>
#include <neutrino.h>
#include <system/settings.h>
#include <driver/rfmod.h>

#define RFMOD_DEV "/dev/rfmod0"
#define IOCTL_SET_CHANNEL           0
#define IOCTL_SET_TESTMODE          1
#define IOCTL_SET_SOUNDENABLE       2
#define IOCTL_SET_SOUNDSUBCARRIER   3
#define IOCTL_SET_FINETUNE          4
#define IOCTL_SET_STANDBY           5

#define C0	3
#define C1	2
#define FL	1
#define FM	0

RFmod::RFmod()
{
	rfmodfd=open(RFMOD_DEV, O_RDWR);
}

void RFmod::init()
{
	soundsubcarrier=g_settings.rf_subcarrier;
	soundenable=g_settings.rf_soundenable;
	channel = g_settings.rf_channel;
	finetune = g_settings.rf_finetune;
	standby = g_settings.rf_standby;

	setSoundSubCarrier(soundsubcarrier);
	setSoundEnable(soundenable);
	setChannel(channel);
	setFinetune(finetune);
	setStandby(standby);
	setTestPattern(0);
}

RFmod::~RFmod()
{
	if (rfmodfd>=0)
		close(rfmodfd);
}	

int	RFmod::setSoundEnable(int val)
{
	soundenable = val;
//printf("RF sound: %d\n", val);
	if(rfmodfd > 0)
		ioctl(rfmodfd,IOCTL_SET_SOUNDENABLE,&soundenable);
		
	return 0;	
}

int	RFmod::setStandby(int val)
{
	standby = val;
//printf("RF standby: %d\n", val);

	if(rfmodfd > 0)
		ioctl(rfmodfd,IOCTL_SET_STANDBY,&standby);

	return 0;
}

int RFmod::setChannel(int val)
{
	channel = val;
//printf("RF channel: %d\n", val);

	if(rfmodfd > 0)
		ioctl(rfmodfd,IOCTL_SET_CHANNEL,&channel);
		
	return 0;	
}

int	RFmod::setFinetune(int val)
{
	finetune = val;
//printf("RF finetune: %d\n", val);

	if(rfmodfd > 0)
		ioctl(rfmodfd,IOCTL_SET_FINETUNE,&finetune);
		
	return 0;	
}		

int	RFmod::setTestPattern(int val)
{
//printf("RF test: %d\n", val);
	if(rfmodfd > 0)
		ioctl(rfmodfd,IOCTL_SET_TESTMODE,&val);
		
	return 0;	
}		


int RFmod::setSoundSubCarrier(int freq)			//freq in KHz
{
//printf("RF carrier: %d\n", freq);
	soundsubcarrier=freq;
/*	
	switch(freq)
	{
		case 4500:
			sfd=0;
			break;
		case 5500:
			sfd=1;
			break;
		case 6000:
			sfd=2;
			break;
		case 6500:
			sfd=3;
			break;
		default:
			return -1;
	}		
*/
	if(rfmodfd > 0)
		ioctl(rfmodfd,IOCTL_SET_SOUNDSUBCARRIER,&soundsubcarrier);

	return 0;
}
