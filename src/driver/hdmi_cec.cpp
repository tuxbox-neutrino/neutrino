/*
	Copyright (C) 2018-2022 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>
#include <ctype.h>

#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <linux/input.h>

#include "hdmi_cec.h"
#include "hdmi_cec_types.h"

#include <neutrino.h>
#include <system/debug.h>
#include <global.h>

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"
#define NORMAL "\x1B[0m"

#define EPOLL_WAIT_TIMEOUT (1000)
#define EPOLL_MAX_EVENTS (1)

#define CEC_HDMIDEV "/dev/hdmi_cec"
#if BOXMODEL_H7 || BOXMODEL_H9COMBO || BOXMODEL_H9
#define RC_DEVICE  "/dev/input/event2"
#elif BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS
#define RC_DEVICE  "/dev/input/event0"
#else
#define RC_DEVICE  "/dev/input/event1"
#endif

hdmi_cec::hdmi_cec()
{
	standby_cec_activ = autoview_cec_activ = muted = false;
	hdmiFd = -1;
	volume = 0;
	tv_off = true;
	deviceType = CECDEVICE_UNREGISTERED;
	audio_destination = CECDEVICE_AUDIOSYSTEM;
	strcpy(osdname, "neutrino");
	running = false;
	active_source = false;
}

hdmi_cec::~hdmi_cec()
{
	if (standby_cec_activ && hdmiFd >= 0)
		SetCECState(true);

	Stop();

	if (hdmiFd >= 0)
	{
		close(hdmiFd);
		hdmiFd = -1;
	}
}

bool hdmi_cec::SetCECMode(VIDEO_HDMI_CEC_MODE cecOnOff)
{
	physicalAddress[0] = 0x10;
	physicalAddress[1] = 0x00;
	logicalAddress = 1;

	if (cecOnOff == VIDEO_HDMI_CEC_MODE_OFF)
	{
		Stop();
		dprintf(DEBUG_NORMAL, GREEN" [CEC] switch off %s\n" NORMAL, __func__);
		return false;
	}

	dprintf(DEBUG_NORMAL, GREEN" [CEC] switch on %s\n" NORMAL, __func__);

	if (hdmiFd == -1)
	{
		hdmiFd = ::open(CEC_HDMIDEV, O_RDWR | O_NONBLOCK | O_CLOEXEC);

		if (hdmiFd >= 0)
		{
			::ioctl(hdmiFd, 0); /* flush old messages */
		}
	}

	if (hdmiFd >= 0)
	{
		GetCECAddressInfo();

		if (autoview_cec_activ)
			SetCECState(false);

		return Start();
	}

	return false;
}

void hdmi_cec::GetCECAddressInfo()
{
	if (hdmiFd >= 0)
	{
		struct addressinfo addressinfo;

		if (::ioctl(hdmiFd, 1, &addressinfo) >= 0)
		{
			deviceType = addressinfo.type;
			logicalAddress = addressinfo.logical;
			dprintf(DEBUG_NORMAL, GREEN" [CEC] %s: detected physical address: 0x%02X:0x%02X (Type: 0x%02X/Logical: 0x%02X)\n" NORMAL, __func__, physicalAddress[0], physicalAddress[1], deviceType, logicalAddress);

			if (memcmp(physicalAddress, addressinfo.physical, sizeof(physicalAddress)))
			{
				dprintf(DEBUG_NORMAL, YELLOW" [CEC] %s: detected physical address change: 0x%02X:0x%02X --> 0x%02X:0x%02X\n" NORMAL, __func__, physicalAddress[0], physicalAddress[1], addressinfo.physical[0], addressinfo.physical[1]);
				memcpy(physicalAddress, addressinfo.physical, sizeof(physicalAddress));
				ReportPhysicalAddress();
			}
		}
	}
}

void hdmi_cec::ReportPhysicalAddress()
{
	struct cec_message txmessage;
	txmessage.initiator = logicalAddress;
	txmessage.destination = CECDEVICE_BROADCAST;
	txmessage.data[0] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
	txmessage.data[1] = physicalAddress[0];
	txmessage.data[2] = physicalAddress[1];
	txmessage.data[3] = deviceType;
	txmessage.length = 4;
	SendCECMessage(txmessage);
}

void hdmi_cec::SendCECMessage(struct cec_message &txmessage, int sleeptime)
{
	(void) sleeptime; // avoid compiler warning

	char str[txmessage.length * 6];

	for (int i = 0; i < txmessage.length; i++)
	{
		sprintf(str + (i * 6), "[0x%02X]", txmessage.data[i]);
	}

	dprintf(DEBUG_NORMAL, YELLOW" [CEC] send message %s to %s (0x%02X>>0x%02X) '%s' (%s)\n" NORMAL, ToString((cec_logical_address)txmessage.initiator), txmessage.destination == 0xf ? "all" : ToString((cec_logical_address)txmessage.destination), txmessage.initiator, txmessage.destination, ToString((cec_opcode)txmessage.data[0]), str);
	struct cec_message_fb message;
	message.address = txmessage.destination;
	message.length = txmessage.length;
	memcpy(&message.data, txmessage.data, txmessage.length);
	mutex.lock();
	msg_que.push_back(message);
	mutex.unlock();
}

void hdmi_cec::SetCECAutoStandby(bool state)
{
	standby_cec_activ = state;
}

void hdmi_cec::SetCECAutoView(bool state)
{
	autoview_cec_activ = state;
}

void hdmi_cec::SetCECState(bool state)
{
	if ((standby_cec_activ) && state)
	{
		SendStandBy();
	}

	if ((autoview_cec_activ) && !state)
	{
		RequestTVPowerStatus();
		SendViewOn();
		active_source = true;
		SendAnnounce();
	}
}

void hdmi_cec::SendStandBy()
{
	struct cec_message message;
	message.initiator = logicalAddress;
	message.destination = CECDEVICE_BROADCAST;
	message.data[0] = CEC_OPCODE_STANDBY;
	message.length = 1;
	SendCECMessage(message);
	message.initiator = logicalAddress;
	message.destination = CECDEVICE_TV;
	message.data[0] = CEC_OPCODE_GIVE_DEVICE_POWER_STATUS;
	message.length = 1;
	SendCECMessage(message);
}

void hdmi_cec::SendViewOn()
{
	struct cec_message message;
	message.initiator = logicalAddress;
	message.destination = CECDEVICE_TV;
	message.data[0] = CEC_OPCODE_IMAGE_VIEW_ON;
	message.length = 1;
	SendCECMessage(message);
	message.initiator = logicalAddress;
	message.destination = CECDEVICE_TV;
	message.data[0] = CEC_OPCODE_GIVE_DEVICE_POWER_STATUS;
	message.length = 1;
	SendCECMessage(message);
}

void hdmi_cec::SendAnnounce()
{
	GetCECAddressInfo();
	struct cec_message message;
	message.initiator = logicalAddress;
	message.destination = CECDEVICE_TV;
	message.data[0] = CEC_OPCODE_GET_CEC_VERSION;
	message.length = 1;
	SendCECMessage(message);

	SendActiveSource(true);

	message.initiator = logicalAddress;
	message.destination = CECDEVICE_TV;
	message.data[0] = CEC_OPCODE_GIVE_OSD_NAME;
	message.length = 1;
	SendCECMessage(message);
	request_audio_status();
}

void hdmi_cec::SendActiveSource(bool force)
{
	struct cec_message message;
	message.destination = CECDEVICE_BROADCAST;
	message.initiator = logicalAddress;
	message.data[0] = CEC_OPCODE_ACTIVE_SOURCE;
	message.data[1] = physicalAddress[0];
	message.data[2] = physicalAddress[1];
	message.length = 3;

	if (active_source || force)
		SendCECMessage(message);
}

void hdmi_cec::RequestTVPowerStatus()
{
	struct cec_message message;
	message.initiator = logicalAddress;
	message.destination = CECDEVICE_TV;
	message.data[0] = CEC_OPCODE_GIVE_DEVICE_POWER_STATUS;
	message.length = 1;
	SendCECMessage(message);
}

long hdmi_cec::translateKey(unsigned char code)
{
	long key = 0;

	switch (code)
	{
		case CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL:
			key = KEY_MENU;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER0:
			key = KEY_0;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER1:
			key = KEY_1;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER2:
			key = KEY_2;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER3:
			key = KEY_3;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER4:
			key = KEY_4;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER5:
			key = KEY_5;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER6:
			key = KEY_6;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER7:
			key = KEY_7;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER8:
			key = KEY_8;
			break;

		case CEC_USER_CONTROL_CODE_NUMBER9:
			key = KEY_9;
			break;

		case CEC_USER_CONTROL_CODE_CHANNEL_UP:
			key = KEY_CHANNELUP;
			break;

		case CEC_USER_CONTROL_CODE_CHANNEL_DOWN:
			key = KEY_CHANNELDOWN;
			break;
#if BOXMODEL_HD51 || BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE || BOXMODEL_HD60 || BOXMODEL_HD61 || BOXMODEL_BRE2ZE4K || BOXMODEL_H7 || BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS || BOXMODEL_SF8008 || BOXMODEL_SF8008M || BOXMODEL_USTYM4KPRO || BOXMODEL_H9COMBO || BOXMODEL_H9

		case CEC_USER_CONTROL_CODE_PLAY:
		case CEC_USER_CONTROL_CODE_PAUSE:
			key = KEY_PLAYPAUSE;
			break;
#elif BOXMODEL_VUPLUS_ALL

		case CEC_USER_CONTROL_CODE_PLAY:
			key = KEY_PLAY;
			break;

		case CEC_USER_CONTROL_CODE_PAUSE:
			key = KEY_PLAYPAUSE;
			break;
#else

		case CEC_USER_CONTROL_CODE_PLAY:
			key = KEY_PLAY;
			break;

		case CEC_USER_CONTROL_CODE_PAUSE:
			key = KEY_PAUSE;
			break;
#endif

		case CEC_USER_CONTROL_CODE_STOP:
			key = KEY_STOP;
			break;

		case CEC_USER_CONTROL_CODE_RECORD:
			key = KEY_RECORD;
			break;

		case CEC_USER_CONTROL_CODE_REWIND:
			key = KEY_REWIND;
			break;

		case CEC_USER_CONTROL_CODE_FAST_FORWARD:
			key = KEY_FASTFORWARD;
			break;

		case CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE:
			key = KEY_INFO;
			break;

		case CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING:
			key = KEY_PROGRAM;
			break;

		case CEC_USER_CONTROL_CODE_PLAY_FUNCTION:
			key = KEY_PLAY;
			break;

		case CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION:
			key = KEY_PLAYPAUSE;
			break;

		case CEC_USER_CONTROL_CODE_RECORD_FUNCTION:
			key = KEY_RECORD;
			break;

		case CEC_USER_CONTROL_CODE_STOP_FUNCTION:
			key = KEY_STOP;
			break;

		case CEC_USER_CONTROL_CODE_SELECT:
			key = KEY_OK;
			break;

		case CEC_USER_CONTROL_CODE_LEFT:
			key = KEY_LEFT;
			break;

		case CEC_USER_CONTROL_CODE_RIGHT:
			key = KEY_RIGHT;
			break;

		case CEC_USER_CONTROL_CODE_UP:
			key = KEY_UP;
			break;

		case CEC_USER_CONTROL_CODE_DOWN:
			key = KEY_DOWN;
			break;

		case CEC_USER_CONTROL_CODE_EXIT:
			key = KEY_EXIT;
			break;

		case CEC_USER_CONTROL_CODE_F2_RED:
			key = KEY_RED;
			break;

		case CEC_USER_CONTROL_CODE_F3_GREEN:
			key = KEY_GREEN;
			break;

		case CEC_USER_CONTROL_CODE_F4_YELLOW:
			key = KEY_YELLOW;
			break;

		case CEC_USER_CONTROL_CODE_F1_BLUE:
			key = KEY_BLUE;
			break;

		case CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION:
			key = KEY_POWER;
			break;

		default:
			key = KEY_MENU;
			break;
	}

	return key;
}

bool hdmi_cec::Start()
{
	if (running)
		return false;

	if (hdmiFd == -1)
		return false;

	dprintf(DEBUG_NORMAL, GREEN" [CEC] thread starting...\n" NORMAL);
	running = true;
	OpenThreads::Thread::setSchedulePriority(THREAD_PRIORITY_MIN);
	return (OpenThreads::Thread::start() == 0);
}

bool hdmi_cec::Stop()
{
	if (!running)
		return false;

	running = false;
	OpenThreads::Thread::cancel();

	if (hdmiFd >= 0)
	{
		close(hdmiFd);
		hdmiFd = -1;
	}

	return (OpenThreads::Thread::join() == 0);
}

void hdmi_cec::run()
{
	OpenThreads::Thread::setCancelModeAsynchronous();
	int n;
	int epollfd = epoll_create1(0);
	struct epoll_event event;
	event.data.fd = hdmiFd;
	event.events = EPOLLPRI | EPOLLIN | EPOLLOUT;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, hdmiFd, &event);
	std::array<struct epoll_event, EPOLL_MAX_EVENTS> events;
	dprintf(DEBUG_NORMAL, GREEN" [CEC] thread started...\n" NORMAL);

	while (running)
	{
		n = epoll_wait(epollfd, events.data(), EPOLL_MAX_EVENTS, EPOLL_WAIT_TIMEOUT);

		for (int i = 0; i < n; ++i)
		{
			if (events[i].events & EPOLLOUT)
			{
				mutex.lock();

				for (std::vector<cec_message_fb>::iterator it = msg_que.begin(); it != msg_que.end(); ++it)
					::write(events[i].data.fd, &(*it), 2 + (*it).length);

				msg_que.clear();
				mutex.unlock();
			}

			if (events[i].events & EPOLLPRI)
			{
				GetCECAddressInfo();
			}

			if (events[i].events & EPOLLIN)
			{
				bool hasdata = false;
				struct cec_message rxmessage;
				struct cec_message txmessage;
				struct cec_message_fb rx_message;

				if (::read(events[i].data.fd, &rx_message, 2) == 2)
				{
					if (::read(events[i].data.fd, &rx_message.data, rx_message.length) == rx_message.length)
					{
						rxmessage.length = rx_message.length;
						rxmessage.initiator = rx_message.address;
						rxmessage.destination = logicalAddress;
						rxmessage.opcode = rx_message.data[0];
						memcpy(&rxmessage.data, rx_message.data, rx_message.length);
						hasdata = true;
					}
				}

				if (hasdata)
				{
					bool keypressed = false;
					static unsigned char pressedkey = 0;
					char str[rxmessage.length * 6];

					for (int j = 0; j < rxmessage.length; j++)
					{
						sprintf(str + (j * 6), "[0x%02X]", rxmessage.data[j]);
					}

					dprintf(DEBUG_NORMAL, GREEN" [CEC] received message %s to %s (0x%02X>>0x%02X) '%s' (%s)\n" NORMAL, ToString((cec_logical_address)rxmessage.initiator), rxmessage.destination == 0xf ? "all" : ToString((cec_logical_address)rxmessage.destination), rxmessage.initiator, rxmessage.destination, ToString((cec_opcode)rxmessage.opcode), str);

					switch (rxmessage.opcode)
					{
						case CEC_OPCODE_GET_CEC_VERSION:
						{
							txmessage.initiator = logicalAddress;
							txmessage.destination = rxmessage.initiator;
							txmessage.data[0] = CEC_OPCODE_CEC_VERSION;
							txmessage.data[1] = CEC_OP_CEC_VERSION_1_4;
							txmessage.length = 2;
							SendCECMessage(txmessage);
							break;
						}

						case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
						{
							ReportPhysicalAddress();
							break;
						}

						case CEC_OPCODE_GIVE_OSD_NAME:
						{
							txmessage.initiator = logicalAddress;
							txmessage.destination = rxmessage.initiator;
							txmessage.data[0] = CEC_OPCODE_SET_OSD_NAME;
							memcpy(txmessage.data + 1, osdname, strlen(osdname));
							txmessage.length = strlen(osdname) + 1;
							SendCECMessage(txmessage);
							break;
						}

						case CEC_OPCODE_FEATURE_ABORT:
						{
							dprintf(DEBUG_NORMAL, GREEN" [CEC] decoded message feature '%s' not supported (%s)\n" NORMAL, ToString((cec_opcode)rxmessage.data[1]), ToString((cec_error_id)rxmessage.data[2]));
							break;
						}

						case CEC_OPCODE_SET_OSD_NAME:
						{
							dprintf(DEBUG_NORMAL, GREEN" [CEC] decoded message '%s' \n" NORMAL, (char *)rxmessage.data + 1);
							break;
						}

						case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
						{
							dprintf(DEBUG_NORMAL, CYAN" [CEC] active_source=%s neutrino standby=%s\n" NORMAL, active_source ? "yes" : "no", CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby ? "yes" : "no");
							if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby && active_source)
									g_RCInput->postMsg(NeutrinoMessages::STANDBY_OFF, (neutrino_msg_data_t)"cec");
							SendActiveSource();
							break;
						}

						case CEC_OPCODE_REPORT_AUDIO_STATUS:
						{
							muted = ((rxmessage.data[1] & 0x80) == 0x80);
							volume = ((rxmessage.data[1] & 0x7F) / 127.0) * 100.0;

							if (muted)
								dprintf(DEBUG_NORMAL, GREEN" [CEC] %s volume muted\n" NORMAL, ToString((cec_logical_address)rxmessage.initiator));
							else
								dprintf(DEBUG_NORMAL, GREEN" [CEC] %s volume %d \n" NORMAL, ToString((cec_logical_address)rxmessage.initiator), volume);

							break;
						}

						case CEC_OPCODE_DEVICE_VENDOR_ID:
						case CEC_OPCODE_VENDOR_COMMAND_WITH_ID:
						{
							uint64_t iVendorId =	((uint64_t)rxmessage.data[1] << 16) +
										((uint64_t)rxmessage.data[2] << 8) +
										(uint64_t)rxmessage.data[3];
							dprintf(DEBUG_NORMAL, GREEN" [CEC] decoded message '%s' (%s)\n" NORMAL, ToString((cec_opcode)rxmessage.opcode), ToString((cec_vendor_id)iVendorId));
							break;
						}

						case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
						{
							txmessage.destination = rxmessage.initiator;
							txmessage.initiator = rxmessage.destination;
							txmessage.data[0] = GetResponseOpcode((cec_opcode)rxmessage.opcode);
							txmessage.data[1] = (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby) ? CEC_POWER_STATUS_STANDBY : CEC_POWER_STATUS_ON;
							txmessage.length = 2;
							SendCECMessage(txmessage);
							break;
						}

						case CEC_OPCODE_REPORT_POWER_STATUS:
						{
							if ((rxmessage.data[1] == CEC_POWER_STATUS_ON) || (rxmessage.data[1] == CEC_POWER_STATUS_IN_TRANSITION_STANDBY_TO_ON))
							{
								dprintf(DEBUG_NORMAL, GREEN" [CEC] %s reporting state on (%d)\n" NORMAL, ToString((cec_logical_address)rxmessage.initiator), rxmessage.data[1]);

								if (rxmessage.initiator == CECDEVICE_TV)
									tv_off = false;
							}
							else
							{
								dprintf(DEBUG_NORMAL, GREEN" [CEC] %s reporting state off (%d)\n" NORMAL, ToString((cec_logical_address)rxmessage.initiator), rxmessage.data[1]);

								if (rxmessage.initiator == CECDEVICE_TV)
									tv_off = true;
							}

							break;
						}

						case CEC_OPCODE_STANDBY:
						{
							if (rxmessage.initiator == CECDEVICE_TV)
							{
								RequestTVPowerStatus();

								if (CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_standby)
									g_RCInput->postMsg(NeutrinoMessages::STANDBY_ON, (neutrino_msg_data_t)"cec");
							}

							break;
						}

						case CEC_OPCODE_SET_STREAM_PATH:
						{
							char msgpath[8];
							char phypath[8];
							sprintf(msgpath, "%02X:%02X", rxmessage.data[1], rxmessage.data[2]);
							sprintf(phypath, "%02X:%02X", physicalAddress[0], physicalAddress[1]);

							if (strcmp(msgpath, phypath) == 0)
							{
								dprintf(DEBUG_NORMAL, CYAN" [CEC] received streampath (%s) change to me (%s) -> wake up \n" NORMAL, msgpath, phypath);

								if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby && g_settings.hdmi_cec_wakeup)
									g_RCInput->postMsg(NeutrinoMessages::STANDBY_OFF, (neutrino_msg_data_t)"cec");

								active_source = true;
								SendActiveSource();
							}
							else
							{
								dprintf(DEBUG_NORMAL, CYAN" [CEC] received streampath (%s) change away from me (%s) -> go to sleep\n" NORMAL, msgpath, phypath);

								if (CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_standby && g_settings.hdmi_cec_sleep)
									g_RCInput->postMsg(NeutrinoMessages::STANDBY_ON, (neutrino_msg_data_t)"cec");

								active_source = false;
							}

							break;
						}

						case CEC_OPCODE_USER_CONTROL_PRESSED: /* key pressed */
						{
							keypressed = true;
							pressedkey = rxmessage.data[1];
						} // fall through

						case CEC_OPCODE_USER_CONTROL_RELEASE: /* key released */
						{
							long code = translateKey(pressedkey);
							dprintf(DEBUG_NORMAL, GREEN" [CEC] decoded key %s (%ld)\n" NORMAL, ToString((cec_user_control_code)pressedkey), code);

							if (code == KEY_POWER || code == KEY_POWERON)
							{
								if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby)
									g_RCInput->postMsg(NeutrinoMessages::STANDBY_OFF, (neutrino_msg_data_t)"cec");
							}
							else
								handleCode(code, keypressed);

							break;
						}
					}
				}
			}
		}
	}
}

void hdmi_cec::handleCode(long code, bool keypressed)
{
	int evd = open(RC_DEVICE, O_RDWR);

	if (evd < 0)
	{
		dprintf(DEBUG_NORMAL, RED" [CEC] opening " RC_DEVICE " failed" NORMAL);
		return;
	}

	if (keypressed)
	{
		if (rc_send(evd, code, CEC_KEY_PRESSED) < 0)
		{
			dprintf(DEBUG_NORMAL, RED" [CEC] writing 'KEY_PRESSED' event failed" NORMAL);
			close(evd);
			return;
		}

		rc_sync(evd);
	}
	else
	{
		if (rc_send(evd, code, CEC_KEY_RELEASED) < 0)
		{
			dprintf(DEBUG_NORMAL, RED" [CEC] writing 'KEY_RELEASED' event failed" NORMAL);
			close(evd);
			return;
		}

		rc_sync(evd);
	}

	close(evd);
}

int hdmi_cec::rc_send(int fd, unsigned int code, unsigned int value)
{
	struct input_event ev;
	ev.type = EV_KEY;
	ev.code = code;
	ev.value = value;
	return write(fd, &ev, sizeof(ev));
}

void hdmi_cec::rc_sync(int fd)
{
	struct input_event ev;
	gettimeofday(&ev.time, NULL);
	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 0;
	write(fd, &ev, sizeof(ev));
}

void hdmi_cec::send_key(unsigned char key, unsigned char destination)
{
	struct cec_message txmessage;
	txmessage.destination = destination;
	txmessage.initiator = logicalAddress;
	txmessage.data[0] = CEC_OPCODE_USER_CONTROL_PRESSED;
	txmessage.data[1] = key;
	txmessage.length = 2;
	SendCECMessage(txmessage);
	txmessage.destination = destination;
	txmessage.initiator = logicalAddress;
	txmessage.data[0] = CEC_OPCODE_USER_CONTROL_RELEASE;
	txmessage.length = 1;
	SendCECMessage(txmessage);
}

void hdmi_cec::request_audio_status()
{
	struct cec_message txmessage;
	txmessage.destination = audio_destination;
	txmessage.initiator = logicalAddress;
	txmessage.data[0] = CEC_OPCODE_GIVE_AUDIO_STATUS;
	txmessage.length = 1;
	SendCECMessage(txmessage);
}

void hdmi_cec::vol_up()
{
	send_key(CEC_USER_CONTROL_CODE_VOLUME_UP, audio_destination);
}
void hdmi_cec::vol_down()
{
	send_key(CEC_USER_CONTROL_CODE_VOLUME_DOWN, audio_destination);
}
void hdmi_cec::toggle_mute()
{
	send_key(CEC_USER_CONTROL_CODE_MUTE, audio_destination);
	request_audio_status();
}

void hdmi_cec::SetAudioDestination(int audio_dest)
{
	switch (audio_dest)
	{
		case 2:
			audio_destination = CECDEVICE_TV;
			break;

		case 1:
		default:
			audio_destination = CECDEVICE_AUDIOSYSTEM;
			break;
	}
}

