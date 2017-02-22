/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
                      2003 thegoodguy

	Copyright (C) 2008-2014,2016-2017 Stefan Seyfried
	Copyright (C) 2013-2014 martii

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <driver/rcinput.h>
#include <driver/abstime.h>
#include <system/helpers.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>
#include <stdlib.h>
#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
#include <termio.h>
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <dirent.h>

#include <eventserver.h>

#include <global.h>
#include <driver/shutdown_count.h>
//#include <neutrino.h>
#include <neutrinoMessages.h>
#include <timerd/timermanager.h>
#include <timerdclient/timerdclient.h>
#include <sectionsdclient/sectionsdclient.h>
#include <cs_api.h>

//#define RCDEBUG

#define ENABLE_REPEAT_CHECK

typedef struct input_event t_input_event;

#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
static struct termio orig_termio;
static bool          saved_orig_termio = false;
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */
static bool input_stopped = false;
static struct timespec devinput_mtime = { 0, 0 };

#ifdef RCDEBUG
#define d_printf printf
#else
#define d_printf(...)
#endif

/*********************************************************************************
*	Constructor - opens rc-input device, selects rc-hardware and starts threads
*
*********************************************************************************/
CRCInput::CRCInput()
{
	timerid= 1;
	repeatkeys = NULL;
	longPressAny = false;

	// pipe for internal event-queue
	// -----------------------------
	if (pipe(fd_pipe_high_priority) < 0)
	{
		perror("fd_pipe_high_priority");
		exit(-1);
	}

	fcntl(fd_pipe_high_priority[0], F_SETFL, O_NONBLOCK );
	fcntl(fd_pipe_high_priority[1], F_SETFL, O_NONBLOCK );

	if (pipe(fd_pipe_low_priority) < 0)
	{
		perror("fd_pipe_low_priority");
		exit(-1);
	}

	fcntl(fd_pipe_low_priority[0], F_SETFL, O_NONBLOCK );
	fcntl(fd_pipe_low_priority[1], F_SETFL, O_NONBLOCK );


	// open event-library
	// -----------------------------
	fd_event = 0;

	//network-setup
	struct sockaddr_un servaddr;
	int    clilen;
	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	cstrncpy(servaddr.sun_path, NEUTRINO_UDS_NAME, sizeof(servaddr.sun_path));
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);
	unlink(NEUTRINO_UDS_NAME);

	//network-setup
	if ((fd_event = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("[neutrino] socket\n");
	}

	if ( bind(fd_event, (struct sockaddr*) &servaddr, clilen) <0 )
	{
		perror("[neutrino] bind failed...\n");
		exit(-1);
	}


	if (listen(fd_event, 25) !=0)
	{
		perror("[neutrino] listen failed...\n");
		exit( -1 );
	}
	repeat_block = repeat_block_generic = 0;
	checkdev();
	open();
	rc_last_key =  KEY_MAX;
	firstKey = true;
	longPressEnd = 0;

	//select and setup remote control hardware
	set_rc_hw();
}

bool CRCInput::checkdev()
{
	/* stat()ing the directory is fast and cheap. If a device gets added or
	 * removed, the mtime of /dev/input/ will change, which in turn
	 * warrants a more thorough investigation */
	struct stat st;
	if (stat("/dev/input/", &st) == 0) {
		if (st.st_mtim.tv_sec != devinput_mtime.tv_sec ||
		    st.st_mtim.tv_nsec != devinput_mtime.tv_nsec) {
			devinput_mtime.tv_sec  = st.st_mtim.tv_sec;
			devinput_mtime.tv_nsec = st.st_mtim.tv_nsec;
			printf("[rcinput:%s] /dev/input mtime changed\n", __func__);
			return true;
		}
		return false; /* still the same... */
	}
	printf("[rcinput:%s] stat /dev/input failed: %m\n", __func__);
	return true; /* need to check anyway... */
}

bool CRCInput::checkpath(in_dev id)
{
	for (std::vector<in_dev>::iterator it = indev.begin(); it != indev.end(); ++it) {
		if ((*it).path == id.path) {
			printf("[rcinput:%s] skipping already opened %s\n", __func__, id.path.c_str());
			return true;
		}
	}
	return false;
}

/* if recheck == true, only not already opened devices are opened, if not, close then (re)open all */
void CRCInput::open(bool recheck)
{
	if (recheck == false)
		close();
	/* close() takes the lock, too... */
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	unsigned long evbit;
	struct in_dev id;
	DIR *dir;
	struct dirent *dentry;
	dir = opendir("/dev/input");
	if (! dir) {
		printf("[rcinput:%s] opendir failed: %m\n", __func__);
		return;
	}

	while ((dentry = readdir(dir)) != NULL)
	{
		id.path = "/dev/input/" + std::string(dentry->d_name);
		/* hack: on hd2, the device is called "/dev/cs_ir",
		   there are links in /dev/input: pointing to it nevis_ir and event0 (WTF???)
		   so if nevis_ir points to cs_ir, accept it, even though it is a symlink...
		   the rest of the code then uses coolstream specific parts if path == "nevis_ir"
		   a better solution would be to simply mknod /dev/input/nevis_ir c 240 0, creating
		   a second instance of /dev/cs_ir named /dv/input/nevis_ir (or to fix the driver
		   to actually create a real input device */
		if (dentry->d_type == DT_LNK &&
		    id.path == "/dev/input/nevis_ir") {
			if (readLink(id.path) != "/dev/cs_ir")
				continue;
		} else if (dentry->d_type != DT_CHR) {
			d_printf("[rcinput:%s] skipping '%s'\n", __func__, dentry->d_name);
			continue;
		}
		d_printf("[rcinput:%s] considering '%s'\n", __func__, dentry->d_name);
		if (checkpath(id))
			continue;
		id.fd = ::open(id.path.c_str(), O_RDWR|O_NONBLOCK|O_CLOEXEC);
		if (id.fd == -1) {
			printf("[rcinput:%s] open %s failed: %m\n", __func__, id.path.c_str());
			continue;
		}
		if (ioctl(id.fd, EVIOCGBIT(0, EV_MAX), &evbit) < 0) {
			::close(id.fd); /* not a proper input device, e.g. /dev/input/mice */
			continue;
		}
		if ((evbit & (1 << EV_KEY)) == 0) {
			printf("[rcinput:%s] %s is bad; no EV_KEY support (0x%lx)\n", __func__, id.path.c_str(), evbit);
			::close(id.fd);
			continue;
		}
		printf("[rcinput:%s] opened %s (fd %d) ev 0x%lx\n", __func__, id.path.c_str(), id.fd, evbit);
		indev.push_back(id);
	}
	closedir(dir);
	id.path = "/tmp/neutrino.input";
	if (! checkpath(id)) {
		id.fd = ::open(id.path.c_str(), O_RDWR|O_NONBLOCK|O_CLOEXEC);
		if (id.fd == -1) {
			/* debug, because it only matters for HAVE_GENERIC_HARDWARE */
			d_printf("[rcinput:%s] open %s failed: %m\n", __func__, id.path.c_str());
		} else {
			printf("[rcinput:%s] opened %s (fd %d)\n", __func__, id.path.c_str(), id.fd);
			indev.push_back(id);
		}
	}

	//+++++++++++++++++++++++++++++++++++++++
#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
	fd_keyb = STDIN_FILENO;
	if (fd_rc[0] < 0)
		fd_rc[0] = fd_keyb;
#else
	fd_keyb = 0;
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */
	/*
	   ::open("/dev/dbox/rc0", O_RDONLY);
	   if (fd_keyb<0)
	   {
	   perror("/dev/stdin");
	   exit(-1);
	   }
	   */
#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
	::fcntl(fd_keyb, F_SETFL, O_NONBLOCK);

	struct termio new_termio;

	::ioctl(STDIN_FILENO, TCGETA, &orig_termio);

	saved_orig_termio      = true;

	new_termio             = orig_termio;
	new_termio.c_lflag    &= ~ICANON;
	//	new_termio.c_lflag    &= ~(ICANON|ECHO);
	new_termio.c_cc[VMIN ] = 1;
	new_termio.c_cc[VTIME] = 0;

	::ioctl(STDIN_FILENO, TCSETA, &new_termio);

#else
	//fcntl(fd_keyb, F_SETFL, O_NONBLOCK );

	//+++++++++++++++++++++++++++++++++++++++
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */

	calculateMaxFd();
}

void CRCInput::close()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	for (unsigned int i = 0; i < indev.size(); i++)
		::close(indev[i].fd);
	indev.clear();
#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
	if (saved_orig_termio)
	{
		::ioctl(STDIN_FILENO, TCSETA, &orig_termio);
		printf("Original terminal settings restored.\n");
	}
#else
/*
	if(fd_keyb)
	{
		::close(fd_keyb);
	}
*/
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */
	calculateMaxFd();
}

void CRCInput::calculateMaxFd()
{
	fd_max = fd_event;

	for (unsigned int i = 0; i < indev.size(); i++)
		if (indev[i].fd > fd_max)
			fd_max = indev[i].fd;

	if(fd_pipe_high_priority[0] > fd_max)
		fd_max = fd_pipe_high_priority[0];
	if(fd_pipe_low_priority[0] > fd_max)
		fd_max = fd_pipe_low_priority[0];
}

/**************************************************************************
*	Destructor - close the input-device
*
**************************************************************************/
CRCInput::~CRCInput()
{
	close();

	if(fd_pipe_high_priority[0])
		::close(fd_pipe_high_priority[0]);
	if(fd_pipe_high_priority[1])
		::close(fd_pipe_high_priority[1]);

	if(fd_pipe_low_priority[0])
		::close(fd_pipe_low_priority[0]);
	if(fd_pipe_low_priority[1])
		::close(fd_pipe_low_priority[1]);

	if(fd_event)
		::close(fd_event);
}

/**************************************************************************
*	stopInput - stop reading rcin for plugins
*
**************************************************************************/
void CRCInput::stopInput(const bool ext)
{
	if (isLocked())
		return;

	input_stopped = true;
	close();
	if (ext)
		postMsg(NeutrinoMessages::LOCK_RC_EXTERN, 0);
}

bool CRCInput::isLocked(void)
{
	return input_stopped;
}

/**************************************************************************
*	restartInput - restart reading rcin after calling plugins
*
**************************************************************************/
void CRCInput::restartInput(const bool ext)
{
	if (!isLocked())
		return;

	close();
	open();
	input_stopped = false;
	if (ext)
		postMsg(NeutrinoMessages::UNLOCK_RC_EXTERN, 0);
}
#if 0 
//never used
int CRCInput::messageLoop( bool anyKeyCancels, int timeout )
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	bool doLoop = true;

	if ( timeout == -1 )
		timeout = g_settings.timing[SNeutrinoSettings::TIMING_MENU];

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout == 0 ? 0xFFFF : timeout);

	while (doLoop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

	if ( ( msg == CRCInput::RC_timeout ) ||
		( msg == CRCInput::RC_home ) ||
		( msg == CRCInput::RC_ok ) )
			doLoop = false;
		else if (CNeutrinoApp::getInstance()->listModeKey(msg)) {
			// do nothing
		}
		else
		{
			int mr = CNeutrinoApp::getInstance()->handleMsg( msg, data );

			if ( mr & messages_return::cancel_all )
			{
				res = menu_return::RETURN_EXIT_ALL;
				doLoop = false;
			}
			else if ( mr & messages_return::unhandled )
			{
				if ((msg <= CRCInput::RC_MaxRC) &&
				    (data == 0))                     /* <- button pressed */
				{
					if ( anyKeyCancels )
						doLoop = false;
					else
						timeoutEnd = CRCInput::calcTimeoutEnd( timeout );
				}
			}
		}


	}
	return res;
}
#endif

int CRCInput::addTimer(uint64_t Interval, bool oneshot, bool correct_time )
{
	uint64_t timeNow = time_monotonic_us();

	timer _newtimer;
	if (!oneshot)
		_newtimer.interval = Interval;
	else
		_newtimer.interval = 0;

	_newtimer.id = timerid++;
	/* in theory, this uint32_t could overflow... */
	/* ...and timerid == 0 is used as "no timer" in many places. */
	if (timerid == 0)
		timerid++;

	if ( correct_time )
		_newtimer.times_out = timeNow+ Interval;
	else
		_newtimer.times_out = Interval;

	_newtimer.correct_time = correct_time;

//printf("adding timer %d (0x%llx, 0x%llx)\n", _newtimer.id, _newtimer.times_out, Interval);

	std::vector<timer>::iterator e;
	for ( e= timers.begin(); e!= timers.end(); ++e )
		if ( e->times_out> _newtimer.times_out )
			break;

	timers.insert(e, _newtimer);
	return _newtimer.id;
}

void CRCInput::killTimer(uint32_t &id)
{
//printf("killing timer %d\n", id);
	if(id == 0)
		return;

	std::vector<timer>::iterator e;
	for ( e= timers.begin(); e!= timers.end(); ++e )
		if ( e->id == id )
		{
			timers.erase(e);
			break;
		}
	id = 0;
}

int CRCInput::checkTimers()
{
	int _id = 0;
	uint64_t timeNow = time_monotonic_us();
	std::vector<timer>::iterator e;
	for ( e= timers.begin(); e!= timers.end(); ++e )
		if ( e->times_out< timeNow+ 2000 )
		{
//printf("timeout timer %d %llx %llx\n",e->id,e->times_out,timeNow );
			_id = e->id;
			if ( e->interval != 0 )
			{
				timer _newtimer;
				_newtimer.id = e->id;
				_newtimer.interval = e->interval;
				_newtimer.correct_time = e->correct_time;
				if ( _newtimer.correct_time )
					_newtimer.times_out = timeNow + e->interval;
				else
					_newtimer.times_out = e->times_out + e->interval;

		            	timers.erase(e);
				for ( e= timers.begin(); e!= timers.end(); ++e )
					if ( e->times_out> _newtimer.times_out )
						break;

				timers.insert(e, _newtimer);
			}
			else
				timers.erase(e);

			break;
        }
//        else
//    		printf("skipped timer %d %llx %llx\n",e->id,e->times_out, timeNow );
//printf("checkTimers: return %d\n", _id);
	return _id;
}



int64_t CRCInput::calcTimeoutEnd(const int timeout_in_seconds)
{
	return time_monotonic_us() + ((uint64_t)timeout_in_seconds * (uint64_t) 1000000);
}

int64_t CRCInput::calcTimeoutEnd_MS(const int timeout_in_milliseconds)
{
	uint64_t timeNow = time_monotonic_us();
	return ( timeNow + timeout_in_milliseconds * 1000 );
}

void CRCInput::getMsgAbsoluteTimeout(neutrino_msg_t * msg, neutrino_msg_data_t * data, uint64_t *TimeoutEnd, bool bAllowRepeatLR)
{
	uint64_t timeNow = time_monotonic_us();
	uint64_t diff;

	if ( *TimeoutEnd < timeNow+ 100 )
		diff = 100;  // Minimum Differenz...
	else
		diff = ( *TimeoutEnd - timeNow );

//printf("CRCInput::getMsgAbsoluteTimeout diff %llx TimeoutEnd %llx now %llx\n", diff, *TimeoutEnd, timeNow);
	getMsg_us( msg, data, diff, bAllowRepeatLR );
}

void CRCInput::getMsg(neutrino_msg_t * msg, neutrino_msg_data_t * data, int Timeout, bool bAllowRepeatLR)
{
	getMsg_us(msg, data, (uint64_t) Timeout * 100 * 1000, bAllowRepeatLR);
}

void CRCInput::getMsg_ms(neutrino_msg_t * msg, neutrino_msg_data_t * data, int Timeout, bool bAllowRepeatLR)
{
	getMsg_us(msg, data, (uint64_t) Timeout * 1000, bAllowRepeatLR);
}

uint32_t *CRCInput::setAllowRepeat(uint32_t *rk) {
	uint32_t *r = repeatkeys;
	repeatkeys = rk;
	return r;
}

bool checkLongPress(uint32_t key); // keybind_setup.cpp

bool CRCInput::mayLongPress(uint32_t key, bool bAllowRepeatLR)
{
	if (mayRepeat(key, bAllowRepeatLR))
		return false;
	if (longPressAny)
		return true;
	return checkLongPress(key);
}

bool CRCInput::mayRepeat(uint32_t key, bool bAllowRepeatLR)
{
	if((key == RC_up) || (key == RC_down)
	|| (key == RC_plus) || (key == RC_minus)
	|| (key == RC_page_down) || (key == RC_page_up)
	|| ((bAllowRepeatLR) && ((key == RC_left ) || (key == RC_right))))
		return true;

	if (repeatkeys) {
		uint32_t *k = repeatkeys;
		while (*k != RC_nokey) {
			if (*k == key) {
				return true;
			}
			k++;
		}
	}
	return false;
}

void CRCInput::getMsg_us(neutrino_msg_t * msg, neutrino_msg_data_t * data, uint64_t Timeout, bool bAllowRepeatLR)
{
	static uint64_t last_keypress = 0ULL;
	//uint64_t getKeyBegin;

	//static __u16 rc_last_key =  KEY_MAX;
	static __u16 rc_last_repeat_key =  KEY_MAX;

	struct timeval tvselect;
	uint64_t InitialTimeout = Timeout;
	int64_t targetTimeout;

	int timer_id;
	fd_set rfds;

	*data = 0;

	/* reopen a missing input device
	 * TODO: real hot-plugging, e.g. of keyboards and triggering this loop...
	 *       right now it is only run if some event is happening "by accident" */
	if (!input_stopped) {
		if (checkdev())
			open(true);
	}

	// wiederholung reinmachen - dass wirklich die ganze zeit bis timeout gewartet wird!
	uint64_t getKeyBegin = time_monotonic_us();

	while(1) {
		timer_id = 0;
		if ( !timers.empty() )
		{
			uint64_t t_n = time_monotonic_us();
			if ( timers[0].times_out< t_n )
			{
				timer_id = checkTimers();
				*msg = NeutrinoMessages::EVT_TIMER;
				*data = timer_id;
				return;
			}
			else
			{
				targetTimeout = timers[0].times_out - t_n;
				if ( (uint64_t) targetTimeout> Timeout)
					targetTimeout= Timeout;
				else
					timer_id = timers[0].id;
			}
		}
		else
			targetTimeout= Timeout;

		tvselect.tv_sec = targetTimeout/1000000;
		tvselect.tv_usec = targetTimeout%1000000;

		FD_ZERO(&rfds);
		for (unsigned int i = 0; i < indev.size(); i++)
		{
			if (indev[i].fd != -1)
				FD_SET(indev[i].fd, &rfds);
		}
#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
		if (true)
#else
			if (fd_keyb> 0)
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */
				FD_SET(fd_keyb, &rfds);

		FD_SET(fd_event, &rfds);
		FD_SET(fd_pipe_high_priority[0], &rfds);
		FD_SET(fd_pipe_low_priority[0], &rfds);

		int status =  select(fd_max+1, &rfds, NULL, NULL, &tvselect);

		if ( status == -1 )
		{
			perror("[neutrino - getMsg_us]: select returned ");
			// in case of an error return timeout...?!
			*msg = RC_timeout;
			*data = 0;
			return;
		}
		else if ( status == 0 ) // Timeout!
		{
			if ( timer_id != 0 )
			{
				timer_id = checkTimers();
				if ( timer_id != 0 )
				{
					*msg = NeutrinoMessages::EVT_TIMER;
					*data = timer_id;
					return;
				}
				else
					continue;
			}
			else
			{
				*msg = RC_timeout;
				*data = 0;
				return;
			}
		}

		if(FD_ISSET(fd_pipe_high_priority[0], &rfds))
		{
			struct event buf;

			read(fd_pipe_high_priority[0], &buf, sizeof(buf));

			*msg  = buf.msg;
			*data = buf.data;

			// printf("got event from high-pri pipe %x %x\n", *msg, *data );

			return;
		}


#ifdef KEYBOARD_INSTEAD_OF_REMOTE_CONTROL
		if (FD_ISSET(fd_keyb, &rfds))
		{
			uint32_t trkey;
			char key = 0;
			read(fd_keyb, &key, sizeof(key));

			switch(key)
			{
				case 27: // <- Esc
					trkey = KEY_HOME;
					break;
				case 10: // <- Return
				case 'o':
					trkey = KEY_OK;
					break;
				case 'p':
					trkey = KEY_POWER;
					break;
				case 's':
					trkey = KEY_SETUP;
					break;
				case 'h':
					trkey = KEY_HELP;
					break;
				case 'i':
					trkey = KEY_UP;
					break;
				case 'm':
					trkey = KEY_DOWN;
					break;
				case 'j':
					trkey = KEY_LEFT;
					break;
				case 'k':
					trkey = KEY_RIGHT;
					break;
				case 'r':
					trkey = KEY_RED;
					break;
				case 'g':
					trkey = KEY_GREEN;
					break;
				case 'y':
					trkey = KEY_YELLOW;
					break;
				case 'b':
					trkey = KEY_BLUE;
					break;
				case '0':
					trkey = RC_0;
					break;
				case '1':
					trkey = RC_1;
					break;
				case '2':
					trkey = RC_2;
					break;
				case '3':
					trkey = RC_3;
					break;
				case '4':
					trkey = RC_4;
					break;
				case '5':
					trkey = RC_5;
					break;
				case '6':
					trkey = RC_6;
					break;
				case '7':
					trkey = RC_7;
					break;
				case '8':
					trkey = RC_8;
					break;
				case '9':
					trkey = RC_9;
					break;
				case '+':
					trkey = RC_plus;
					break;
				case '-':
					trkey = RC_minus;
					break;
				case 'a':
					trkey = KEY_A;
					break;
				case 'u':
					trkey = KEY_U;
					break;
				case '/':
					trkey = KEY_SLASH;
					break;
				case '\\':
					trkey = KEY_BACKSLASH;
					break;
				default:
					trkey = RC_nokey;
			}
			if (trkey != RC_nokey)
			{
				*msg = trkey;
				*data = 0; /* <- button pressed */
				return;
			}
		}
#else
		/*
		   if(FD_ISSET(fd_keyb, &rfds))
		   {
		   char key = 0;
		   read(fd_keyb, &key, sizeof(key));
		   printf("keyboard: %d\n", rc_key);
		   }
		   */
#endif /* KEYBOARD_INSTEAD_OF_REMOTE_CONTROL */

		if(FD_ISSET(fd_event, &rfds)) {
			//printf("[neutrino] event - accept!\n");
			socklen_t          clilen;
			struct sockaddr_in cliaddr;
			clilen = sizeof(cliaddr);
			int fd_eventclient = accept(fd_event, (struct sockaddr *) &cliaddr, &clilen);

			*msg = RC_nokey;
			//printf("[neutrino] network event - read!\n");
			CEventServer::eventHead emsg;
			int read_bytes= recv(fd_eventclient, &emsg, sizeof(emsg), MSG_WAITALL);
			//printf("[neutrino] event read %d bytes - following %d bytes\n", read_bytes, emsg.dataSize );
			if ( read_bytes == sizeof(emsg) ) {
				bool dont_delete_p = false;

				unsigned char* p;
				p= new unsigned char[ emsg.dataSize + 1 ];
				if ( p!=NULL )
				{
					read_bytes= recv(fd_eventclient, p, emsg.dataSize, MSG_WAITALL);
					//printf("[neutrino] eventbody read %d bytes - initiator %x\n", read_bytes, emsg.initiatorID );

#if 0
					if ( emsg.initiatorID == CEventServer::INITID_CONTROLD )
					{
						switch(emsg.eventID)
						{
							case CControldClient::EVT_VOLUMECHANGED :
								*msg = NeutrinoMessages::EVT_VOLCHANGED;
								*data = 0;
								break;
							case CControldClient::EVT_MUTECHANGED :
								*msg = NeutrinoMessages::EVT_MUTECHANGED;
								*data = (unsigned) p;
								dont_delete_p = true;
								break;
							case CControldClient::EVT_VCRCHANGED :
								*msg = NeutrinoMessages::EVT_VCRCHANGED;
								*data = *(int*) p;
								break;
							case CControldClient::EVT_MODECHANGED :
								*msg = NeutrinoMessages::EVT_MODECHANGED;
								*data = *(int*) p;
								break;
							default:
								printf("[neutrino] event INITID_CONTROLD - unknown eventID 0x%x\n",  emsg.eventID );
						}
					}
					else 
#endif
					if ( emsg.initiatorID == CEventServer::INITID_HTTPD )
					{
						switch(emsg.eventID)
						{
							case NeutrinoMessages::SHUTDOWN :
								*msg = NeutrinoMessages::SHUTDOWN;
								*data = 0;
								break;
							case NeutrinoMessages::REBOOT :
								*msg = NeutrinoMessages::REBOOT;
								*data = 0;
								break;
							case NeutrinoMessages::EVT_POPUP :
								*msg = NeutrinoMessages::EVT_POPUP;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case NeutrinoMessages::EVT_EXTMSG :
								*msg = NeutrinoMessages::EVT_EXTMSG;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case NeutrinoMessages::CHANGEMODE :	// Change
								*msg = NeutrinoMessages::CHANGEMODE;
								*data = *(unsigned long*) p;
								break;
							case NeutrinoMessages::STANDBY_TOGGLE :
								*msg = NeutrinoMessages::STANDBY_TOGGLE;
								*data = 0;
								break;
							case NeutrinoMessages::STANDBY_ON :
								*msg = NeutrinoMessages::STANDBY_ON;
								*data = 0;
								break;
							case NeutrinoMessages::STANDBY_OFF :
								*msg = NeutrinoMessages::STANDBY_OFF;
								*data = 0;
								break;
							case NeutrinoMessages::EVT_START_PLUGIN :
								*msg = NeutrinoMessages::EVT_START_PLUGIN;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case NeutrinoMessages::LOCK_RC :
								*msg = NeutrinoMessages::LOCK_RC;
								*data = 0;
								break;
							case NeutrinoMessages::UNLOCK_RC :
								*msg = NeutrinoMessages::UNLOCK_RC;
								*data = 0;
								break;
							case NeutrinoMessages::RELOAD_SETUP :
								*msg = NeutrinoMessages::RELOAD_SETUP;
								*data = 0;
								break;
							case NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON:
								*msg          = NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON;
								*data         = 0;
								break;
							case NeutrinoMessages::EVT_HDMI_CEC_STANDBY:
								*msg          = NeutrinoMessages::EVT_HDMI_CEC_STANDBY;
								*data         = 0;
								break;
							case NeutrinoMessages::EVT_SET_MUTE :
								*msg = NeutrinoMessages::EVT_SET_MUTE;
								*data = *(char*) p;
								break;
							case NeutrinoMessages::EVT_SET_VOLUME :
								*msg = NeutrinoMessages::EVT_SET_VOLUME;
								*data = *(char*) p;
								break;
							case NeutrinoMessages::RECORD_START :
								*msg = NeutrinoMessages::RECORD_START;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case NeutrinoMessages::RECORD_STOP :
								*msg = NeutrinoMessages::RECORD_STOP;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							default:
								printf("[neutrino] event INITID_HTTPD - unknown eventID 0x%x\n",  emsg.eventID );
						}
					}
					else if ( emsg.initiatorID == CEventServer::INITID_SECTIONSD )
					{
						//printf("[neutrino] event - from SECTIONSD %x %x\n", emsg.eventID, *(unsigned*) p);
						switch(emsg.eventID)
						{
							case CSectionsdClient::EVT_TIMESET:
							{
								printf("[neutrino] CSectionsdClient::EVT_TIMESET: timediff %" PRId64 "\n", *(int64_t*) p);
								/* compensate last_keypress for autorepeat / long press detection
								 * I doubt this works correcty, if we had kernel 3.4+, using
								 * EVIOCSCLOCKID ioctl would be better.
								 * Still guessing the logic behind the condition... */
								if ((int64_t)last_keypress > *(int64_t*)p)
									last_keypress += *(int64_t *)p;

								*msg          = NeutrinoMessages::EVT_TIMESET;
								*data         = (neutrino_msg_data_t) p;
								dont_delete_p = true;
								break;
							}
							case CSectionsdClient::EVT_GOT_CN_EPG:
								printf("[neutrino] CSectionsdClient::EVT_GOT_CN_EPG\n");
								*msg          = NeutrinoMessages::EVT_CURRENTNEXT_EPG;
								*data         = (neutrino_msg_data_t) p;
								dont_delete_p = true;
								break;
							case CSectionsdClient::EVT_WRITE_SI_FINISHED:
								*msg          = NeutrinoMessages::EVT_SI_FINISHED;
								*data         = 0;
								break;
							case CSectionsdClient::EVT_EIT_COMPLETE:
								printf("[neutrino] CSectionsdClient::EVT_EIT_COMPLETE\n");
								*msg          = NeutrinoMessages::EVT_EIT_COMPLETE;
								*data         = (neutrino_msg_data_t) p;
								dont_delete_p = true;
								break;
#if 0
							case CSectionsdClient::EVT_SERVICES_UPDATE:
								*msg          = NeutrinoMessages::EVT_SERVICES_UPD;
								*data         = 0;
								break;
							case CSectionsdClient::EVT_BOUQUETS_UPDATE:
								break;
#endif
							default:
								printf("[neutrino] event INITID_SECTIONSD - unknown eventID 0x%x\n",  emsg.eventID );
						}
					}
					else if ( emsg.initiatorID == CEventServer::INITID_ZAPIT )
					{
						//printf("[neutrino] event - from ZAPIT %x %x\n", emsg.eventID, *(unsigned*) p);
						switch(emsg.eventID)
						{
							case CZapitClient::EVT_RECORDMODE_ACTIVATED:
								*msg  = NeutrinoMessages::EVT_RECORDMODE;
								*data = true;
								break;
							case CZapitClient::EVT_RECORDMODE_DEACTIVATED:
								*msg  = NeutrinoMessages::EVT_RECORDMODE;
								*data = false;
								break;
							case CZapitClient::EVT_ZAP_COMPLETE:
								*msg = NeutrinoMessages::EVT_ZAP_COMPLETE;
								break;
							case CZapitClient::EVT_ZAP_FAILED:
								*msg = NeutrinoMessages::EVT_ZAP_FAILED;
								break;
							case CZapitClient::EVT_ZAP_SUB_FAILED:
								*msg = NeutrinoMessages::EVT_ZAP_SUB_FAILED;
								break;
							case CZapitClient::EVT_ZAP_COMPLETE_IS_NVOD:
								*msg = NeutrinoMessages::EVT_ZAP_ISNVOD;
								break;
							case CZapitClient::EVT_ZAP_SUB_COMPLETE:
								*msg = NeutrinoMessages::EVT_ZAP_SUB_COMPLETE;
								break;
							case CZapitClient::EVT_SCAN_COMPLETE:
								*msg  = NeutrinoMessages::EVT_SCAN_COMPLETE;
								*data = 0;
								break;
							case CZapitClient::EVT_SCAN_NUM_TRANSPONDERS:
								*msg  = NeutrinoMessages::EVT_SCAN_NUM_TRANSPONDERS;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS:
								*msg  = NeutrinoMessages::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_FOUND_A_CHAN:
								*msg = NeutrinoMessages::EVT_SCAN_FOUND_A_CHAN;
								break;
							case CZapitClient::EVT_SCAN_SERVICENAME:
								*msg = NeutrinoMessages::EVT_SCAN_SERVICENAME;
								break;
							case CZapitClient::EVT_SCAN_FOUND_TV_CHAN:
								*msg  = NeutrinoMessages::EVT_SCAN_FOUND_TV_CHAN;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_FOUND_RADIO_CHAN:
								*msg  = NeutrinoMessages::EVT_SCAN_FOUND_RADIO_CHAN;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_FOUND_DATA_CHAN:
								*msg  = NeutrinoMessages::EVT_SCAN_FOUND_DATA_CHAN;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_REPORT_FREQUENCYP:
								*msg  = NeutrinoMessages::EVT_SCAN_REPORT_FREQUENCYP;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_NUM_CHANNELS:
								*msg = NeutrinoMessages::EVT_SCAN_NUM_CHANNELS;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_PROVIDER:
								*msg = NeutrinoMessages::EVT_SCAN_PROVIDER;
								break;
							case CZapitClient::EVT_SCAN_SATELLITE:
								*msg = NeutrinoMessages::EVT_SCAN_SATELLITE;
								break;
							case CZapitClient::EVT_BOUQUETS_CHANGED:
								*msg  = NeutrinoMessages::EVT_BOUQUETSCHANGED;
								*data = 0;
								break;
							case CZapitClient::EVT_SERVICES_CHANGED:
								*msg  = NeutrinoMessages::EVT_SERVICESCHANGED;
								*data = 0;
								break;
							case CZapitClient::EVT_ZAP_CA_CLEAR:
								*msg  = NeutrinoMessages::EVT_ZAP_CA_CLEAR;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_ZAP_CA_LOCK:
								*msg  = NeutrinoMessages::EVT_ZAP_CA_LOCK;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_ZAP_CA_FTA:
								*msg  = NeutrinoMessages::EVT_ZAP_CA_FTA;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_ZAP_CA_ID :
								*msg = NeutrinoMessages::EVT_ZAP_CA_ID;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SCAN_FAILED:
								*msg  = NeutrinoMessages::EVT_SCAN_FAILED;
								*data = 0;
								break;
							case CZapitClient::EVT_ZAP_MOTOR:
								*msg  = NeutrinoMessages::EVT_ZAP_MOTOR;
								*data = *(unsigned*) p;
								break;
							case CZapitClient::EVT_SDT_CHANGED:
								*msg          = NeutrinoMessages::EVT_SERVICES_UPD;
								*data         = 0;
								break;
							case CZapitClient::EVT_PMT_CHANGED:
								*msg          = NeutrinoMessages::EVT_PMT_CHANGED;
								*data = (neutrino_msg_data_t) p;
								break;
							case CZapitClient::EVT_TUNE_COMPLETE:
								*msg          = NeutrinoMessages::EVT_TUNE_COMPLETE;
								*data = (neutrino_msg_data_t) p;
								break;
							case CZapitClient::EVT_BACK_ZAP_COMPLETE:
								*msg          = NeutrinoMessages::EVT_BACK_ZAP_COMPLETE;
								*data = (neutrino_msg_data_t) p;
								break;
							case CZapitClient::EVT_WEBTV_ZAP_COMPLETE:
								*msg          = NeutrinoMessages::EVT_WEBTV_ZAP_COMPLETE;
								*data = (neutrino_msg_data_t) p;
								break;
							default:
								printf("[neutrino] event INITID_ZAPIT - unknown eventID 0x%x\n",  emsg.eventID );
						}
						if (((*msg) >= CRCInput::RC_WithData) && ((*msg) < CRCInput::RC_WithData + 0x10000000))
						{
							*data         = (neutrino_msg_data_t) p;
							dont_delete_p = true;
						}
					}
					else if ( emsg.initiatorID == CEventServer::INITID_TIMERD )
					{
						/*
						   if (emsg.eventID==CTimerdClient::EVT_ANNOUNCE_NEXTPROGRAM)
						   {
						   }

						   if (emsg.eventID==CTimerdClient::EVT_NEXTPROGRAM)
						   {
						 *msg = NeutrinoMessages::EVT_NEXTPROGRAM;
						 *data = (neutrino_msg_data_t) p;
						 dont_delete_p = true;
						 }
						 */
						switch(emsg.eventID)
						{
							case CTimerdClient::EVT_ANNOUNCE_RECORD :
								*msg = NeutrinoMessages::ANNOUNCE_RECORD;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case CTimerdClient::EVT_ANNOUNCE_ZAPTO :
								*msg = NeutrinoMessages::ANNOUNCE_ZAPTO;
								*data = (neutrino_msg_data_t)p;
								dont_delete_p = true;
								break;
							case CTimerdClient::EVT_ANNOUNCE_SHUTDOWN :
								*msg = NeutrinoMessages::ANNOUNCE_SHUTDOWN;
								*data = 0;
								break;
							case CTimerdClient::EVT_ANNOUNCE_SLEEPTIMER :
								*msg = NeutrinoMessages::ANNOUNCE_SLEEPTIMER;
								*data = 0;
								break;
							case CTimerdClient::EVT_SLEEPTIMER :
								*msg = NeutrinoMessages::SLEEPTIMER;
								*data = 0;
								break;
							case CTimerdClient::EVT_RECORD_START :
								*msg = NeutrinoMessages::RECORD_START;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case CTimerdClient::EVT_RECORD_STOP :
								*msg = NeutrinoMessages::RECORD_STOP;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case CTimerdClient::EVT_ZAPTO :
								*msg = NeutrinoMessages::ZAPTO;
								*data = (unsigned long)  p;
								dont_delete_p = true;
								break;
							case CTimerdClient::EVT_SHUTDOWN :
								*msg = NeutrinoMessages::SHUTDOWN;
								*data = 0;
								break;
							case CTimerdClient::EVT_STANDBY_ON :
								*msg = NeutrinoMessages::STANDBY_ON;
								*data = 0;
								break;
							case CTimerdClient::EVT_STANDBY_OFF :
								*msg = NeutrinoMessages::STANDBY_OFF;
								*data = 0;
								break;
							case CTimerdClient::EVT_REMIND :
								*msg = NeutrinoMessages::REMIND;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							case CTimerdClient::EVT_EXEC_PLUGIN :
								*msg = NeutrinoMessages::EVT_START_PLUGIN;
								*data = (unsigned long) p;
								dont_delete_p = true;
								break;
							default :
								printf("[neutrino] event INITID_TIMERD - unknown eventID 0x%x\n",  emsg.eventID );

						}
					}
					else if (emsg.initiatorID == CEventServer::INITID_NEUTRINO)
					{
						printf("CRCInput::getMsg_us: INITID_NEUTRINO: msg %x size %d data %p\n", (int) emsg.eventID, emsg.dataSize, p);
						if (emsg.eventID == NeutrinoMessages::EVT_HOTPLUG) {
							printf("EVT_HOTPLUG: [%s]\n", (char *) p);
							*msg  = emsg.eventID;
							*data = (neutrino_msg_data_t) p;
							dont_delete_p = true;
						}
#if 0
						if ((emsg.eventID == NeutrinoMessages::EVT_RECORDING_ENDED) &&
								(read_bytes == sizeof(stream2file_status2_t)))
						{
							*msg  = NeutrinoMessages::EVT_RECORDING_ENDED;
							*data = (neutrino_msg_data_t) p;
							dont_delete_p = true;
						}
#endif
					}
					else if (emsg.initiatorID == CEventServer::INITID_GENERIC_INPUT_EVENT_PROVIDER)
					{
						if (read_bytes == sizeof(int))
						{
							*msg  = *(int *)p;
							*data = emsg.eventID;
						}
					}
					else
						printf("[neutrino] event - unknown initiatorID 0x%x\n",  emsg.initiatorID);

					switch (emsg.eventID) {
						case NeutrinoMessages::EVT_CURRENTEPG:
						case NeutrinoMessages::EVT_NEXTEPG:
							{
								CSectionsdClient::CurrentNextInfo *cn = (CSectionsdClient::CurrentNextInfo *) p;
								delete [] cn;
								p = NULL;
								break;
							}
						default:
							if (!dont_delete_p) {
								delete[] p;
								p = NULL;
							}
					}
				}
			}
			else
			{
				printf("[neutrino] event - read failed!\n");
			}

			::close(fd_eventclient);

			if ( *msg != RC_nokey )
			{
				// raus hier :)
				//printf("[neutrino] event 0x%x\n", *msg);
				return;
			}
		}

		for (std::vector<in_dev>::iterator i = indev.begin(); i != indev.end(); ++i) {
			if (((*i).fd != -1) && (FD_ISSET((*i).fd, &rfds))) {
				uint64_t now_pressed = 0;
				t_input_event ev;
				memset(&ev, 0, sizeof(ev));
				/* we later check for ev.type = EV_SYN = 0x00, so set something invalid here... */
				ev.type = EV_MAX;
				int ret = read((*i).fd, &ev, sizeof(t_input_event));
				if (ret != sizeof(t_input_event)) {
					if (errno == ENODEV) {
						/* hot-unplugged? */
						::close((*i).fd);
						indev.erase(i);
					}
					continue;
				}
				if (ev.type == EV_SYN)
					continue; /* ignore... */
				if (ev.value) {
					/* try to compensate for possible changes in wall clock
					 * kernel ev.time default uses CLOCK_REALTIME, as does gettimeofday().
					 * so subtract gettimeofday() from ev.time and then add
					 * CLOCK_MONOTONIC, which is supposed to not change with settimeofday.
					 * Everything would be much easier if we could use the post-kernel 3.4
					 * EVIOCSCLOCKID ioctl :-) */
					struct timespec t1;
					now_pressed = ev.time.tv_usec + ev.time.tv_sec * 1000000ULL;
					if (!clock_gettime(CLOCK_MONOTONIC, &t1)) {
						struct timeval t2;
						gettimeofday(&t2, NULL);
						now_pressed += t1.tv_sec * 1000000ULL + t1.tv_nsec / 1000;
						now_pressed -= (t2.tv_usec + t2.tv_sec * 1000000ULL);
					}
				}
				SHTDCNT::getInstance()->resetSleepTimer();
				if (ev.value && firstKey) {
					firstKey = false;
					CTimerManager::getInstance()->cancelShutdownOnWakeup();
				}

				uint32_t trkey = translate(ev.code);
				d_printf("key: %04x value %d, translate: %04x -%s-\n", ev.code, ev.value, trkey, getKeyName(trkey).c_str());
				if (trkey == RC_nokey)
					continue;

				if (g_settings.longkeypress_duration > LONGKEYPRESS_OFF) {
					uint64_t longPressNow = time_monotonic_us();
					if (ev.value == 0 && longPressEnd) {
						if (longPressNow < longPressEnd) {
							// Key was a potential long press, but wasn't pressed long enough
							longPressEnd = 0;
							ev.value = 1;
						} else {
							// Long-press, key released after time limit
							longPressEnd = 0;
							continue;
						}
					} else if (ev.value == 1 && mayLongPress(trkey, bAllowRepeatLR)) {
						// A long-press may start here.
						longPressEnd = longPressNow + 1000 * g_settings.longkeypress_duration;
						rc_last_key = KEY_MAX;
						continue;
					} else if (ev.value == 2 && longPressEnd) {
						if (longPressEnd < longPressNow) {
							// Key was pressed long enough.
							ev.value = 1;
							trkey |= RC_Repeat;
						} else {
							// Long-press, but key still not released. Skip.
							continue;
						}
					}
				}

				if (ev.value) {
					d_printf("rc_last_key %04x rc_last_repeat_key %04x\n\n", rc_last_key, rc_last_repeat_key);
					bool keyok = true;
#if 0
					uint64_t now_pressed;
					tv = ev.time;
					now_pressed = (uint64_t) tv.tv_usec + (uint64_t)((uint64_t) tv.tv_sec * (uint64_t) 1000000);
#endif
					if (trkey == rc_last_key) {
						/* only allow selected keys to be repeated */
						if (mayRepeat(trkey, bAllowRepeatLR) ||
						    (g_settings.shutdown_real_rcdelay && ((trkey == RC_standby) && (g_info.hw_caps->can_shutdown))))
						{
#ifdef ENABLE_REPEAT_CHECK
							if (rc_last_repeat_key != trkey) {
								if ((now_pressed > last_keypress + repeat_block) ||
										/* accept all keys after time discontinuity: */
										(now_pressed < last_keypress))
									rc_last_repeat_key = trkey;
								else
									keyok = false;
							}
#endif
						}
						else
							keyok = false;
					}
					else
						rc_last_repeat_key = KEY_MAX;

					rc_last_key = trkey;

					if (keyok) {
#ifdef ENABLE_REPEAT_CHECK
						if ((now_pressed > last_keypress + repeat_block_generic) ||
								/* accept all keys after time discontinuity: */
								(now_pressed < last_keypress))
#endif
						{
							last_keypress = now_pressed;

							*msg = trkey;
							*data = 0; /* <- button pressed */
							return;
						}
					} /*if keyok */
				} /* if (ev.value) */
				else {
					// clear rc_last_key on keyup event
					rc_last_key = KEY_MAX;
					if (trkey == RC_standby) {
						*msg = RC_standby;
						*data = 1; /* <- button released */
						return;
					}
				}
			}/* if FDSET */
		} /* for NUMBER_OF_EVENT_DEVICES */

		if(FD_ISSET(fd_pipe_low_priority[0], &rfds))
		{
			struct event buf;

			read(fd_pipe_low_priority[0], &buf, sizeof(buf));

			*msg  = buf.msg;
			*data = buf.data;

			// printf("got event from low-pri pipe %x %x\n", *msg, *data );

			return;
		}

		if ( InitialTimeout == 0 )
		{
			//nicht warten wenn kein key da ist
			*msg = RC_timeout;
			*data = 0;
			return;
		}
		else
		{
			//timeout neu kalkulieren
			int64_t getKeyNow = time_monotonic_us();
			int64_t diff = (getKeyNow - getKeyBegin);
			if( Timeout <= (uint64_t) diff )
			{
				*msg = RC_timeout;
				*data = 0;
				return;
			}
			else
				Timeout -= diff;
		}
	}
}

void CRCInput::postMsg(const neutrino_msg_t msg, const neutrino_msg_data_t data, const bool Priority)
{
//	printf("postMsg %x %x %d\n", msg, data, Priority );

	struct event buf;
	buf.msg  = msg;
	buf.data = data;

	if (Priority)
		write(fd_pipe_high_priority[1], &buf, sizeof(buf));
	else
		write(fd_pipe_low_priority[1], &buf, sizeof(buf));
}


void CRCInput::clearRCMsg()
{
	t_input_event ev;

	for (unsigned int i = 0; i < indev.size(); i++)
	{
		if (indev[i].fd != -1)
		{
			while (read(indev[i].fd, &ev, sizeof(t_input_event)) == sizeof(t_input_event))
				;
		}
	}
	rc_last_key =  KEY_MAX;
}

/**************************************************************************
*       isNumeric - test if key is 0..9
*
**************************************************************************/
bool CRCInput::isNumeric(const neutrino_msg_t key)
{
	return ((key == RC_0) || ((key >= RC_1) && (key <= RC_9)));
}

/**************************************************************************
*       getNumericValue - return numeric value of the key or -1
*
**************************************************************************/
int CRCInput::getNumericValue(const neutrino_msg_t key)
{
	return ((key == RC_0) ? (int)0 : (((key >= RC_1) && (key <= RC_9)) ? (int)(key - RC_1 + 1) : (int)-1));
}

/**************************************************************************
*       convertDigitToKey - return key representing digit or RC_nokey
*
**************************************************************************/
static const unsigned int digit_to_key[10] = {CRCInput::RC_0, CRCInput::RC_1, CRCInput::RC_2, CRCInput::RC_3, CRCInput::RC_4, CRCInput::RC_5, CRCInput::RC_6, CRCInput::RC_7, CRCInput::RC_8, CRCInput::RC_9};

unsigned int CRCInput::convertDigitToKey(const unsigned int digit)
{
	return (digit < 10) ? digit_to_key[digit] : RC_nokey;
}

/**************************************************************************
*       getUnicodeValue - return unicode value of the key or \0
*
**************************************************************************/
#define UNICODE_VALUE_SIZE 58
static const char unicode_value[UNICODE_VALUE_SIZE * 2] =
	"\0\0" "\0\0" "1\0"  "2\0" "3\0" "4\0"  "5\0"  "6\0"  "7\0"    "8\0"  "9\0"  "0\0"  "-\0"  "=\0" "\0\0" "\0\0"
	 "Q\0"  "W\0" "E\0"  "R\0" "T\0" "Y\0"  "U\0"  "I\0"  "O\0"    "P\0"  "{\0"  "}\0" "\0\0" "\0\0"  "A\0"  "S\0"
	 "D\0"  "F\0" "G\0"  "H\0" "J\0" "K\0"  "L\0"  ";\0"  "'\0" "\140\0" "\0\0" "\\\0"  "Z\0"  "X\0"  "C\0"  "V\0"
	 "B\0"  "N\0" "M\0" "\0\0" ".\0" "/\0" "\0\0" "\0\0" "\0\0"      " ";

const char *CRCInput::getUnicodeValue(const neutrino_msg_t key)
{
	if (key < UNICODE_VALUE_SIZE)
		return unicode_value + key * 2;
	return ""; 
}

/**************************************************************************
*       transforms the rc-key to const char *
*
**************************************************************************/
const char * CRCInput::getSpecialKeyName(const unsigned int key)
{
	switch(key)
	{
			case RC_standby:
				return "standby";
			case RC_home:
				return "home";
			case RC_setup:
				return "setup";
			case RC_red:
				return "red button";
			case RC_green:
				return "green button";
			case RC_yellow:
				return "yellow button";
			case RC_blue:
				return "blue button";
			case RC_page_up:
				return "page up";
			case RC_page_down:
				return "page down";
			case RC_up:
				return "cursor up";
			case RC_down:
				return "cursor down";
			case RC_left:
				return "cursor left";
			case RC_right:
				return "cursor right";
			case RC_ok:
				return "ok";
			case RC_plus:
				return "vol. inc";
			case RC_minus:
				return "vol. dec";
			case RC_spkr:
				return "mute";
			case RC_help:
				return "help";
			case RC_info:
				return "info";
			case RC_topleft:
				return "topleft";
			case RC_topright:
				return "topright";
			case RC_audio:
				return "audio";
			case RC_video:
				return "video";
			case RC_tv:
				return "tv";
			case RC_radio:
				return "radio";
			case RC_text:
				return "text";
			case RC_epg:
				return "epg";
			case RC_recall:
				return "recall";
			case RC_favorites:
				return "favorites";
			case RC_sat:
				return "sat";
			case RC_sat2:
				return "sat2";
			case RC_timeout:
				return "timeout";
			case RC_play:
				return "play";
			case RC_stop:
				return "stop";
			case RC_forward:
				return "forward";
			case RC_rewind:
				return "rewind";
			case RC_timeshift:
				return "timeshift";
			case RC_mode:
				return "mode";
			case RC_record:
				return "record";
			case RC_pause:
				return "pause";
			case RC_games:
				return "games";
			case RC_next:
				return "next";
			case RC_prev:
				return "prev";
			case RC_nokey:
				return "none";
			case RC_power_on:
				return "power on";
			case RC_power_off:
				return "power off";
			case RC_standby_on:
				return "standby on";
			case RC_standby_off:
				return "standby off";
			case RC_mute_on:
				return "mute on";
			case RC_mute_off:
				return "mute off";
			case RC_analog_on:
				return "analog on";
			case RC_analog_off:
				return "analog off";
			case RC_www:
				return "www";
			case RC_sub:
				return "sub";
			case RC_pos:
				return "pos";
			case RC_sleep:
				return "sleep";
			default:
				printf("unknown key: %d (0x%x) \n", key, key);
				return "unknown";
	}
}

std::string CRCInput::getKeyName(const unsigned int key)
{
	std::string res(getKeyNameC(key & ~RC_Repeat));
	if ((key & RC_Repeat) && res != "unknown")
		res += " (long)";
	return res;
}

const char *CRCInput::getKeyNameC(const unsigned int key)
{
	const char *lunicode_value = getUnicodeValue(key);
	if (*lunicode_value)
		return lunicode_value;
	return getSpecialKeyName(key);
}

/**************************************************************************
*	transforms the rc-key to generic - internal use only!
*
**************************************************************************/
int CRCInput::translate(int code)
{
	switch(code)
	{
		case 0x100: // FIXME -- needed?
			return RC_up;
		case 0x101: // FIXME -- needed?
			return RC_down;
#ifdef HAVE_AZBOX_HARDWARE
		case KEY_HOME:
			return RC_favorites;
		case KEY_TV:
			return RC_stop;
		case KEY_RADIO:
			return RC_record;
		case KEY_PLAY:
			return RC_pause;
#endif
		default:
			break;
	}
	if ((code >= 0) && (code <= KEY_MAX))
		return code;

	return (int)RC_nokey;
}

void CRCInput::setKeyRepeatDelay(unsigned int start_ms, unsigned int repeat_ms)
{
	for (std::vector<in_dev>::iterator it = indev.begin(); it != indev.end(); ++it) {
		int fd = (*it).fd;
		std::string path = (*it).path;
		if (path == "/tmp/neutrino.input")
			continue; /* setting repeat rate does not work here */
#ifdef BOXMODEL_CS_HD1
		/* this is ugly, but the driver does not support anything advanced... */
		if (path == "/dev/input/nevis_ir") {
			d_printf("[rcinput:%s] %s(fd %d) using proprietary ioctl\n", __func__, path.c_str(), fd);
			ioctl(fd, IOC_IR_SET_F_DELAY, start_ms);
			ioctl(fd, IOC_IR_SET_X_DELAY, repeat_ms);
			continue;
		}
#endif
		d_printf("[rcinput:%s] %s(fd %d) writing EV_REP (%d->%d)\n",
				__func__, path.c_str(), fd, start_ms, repeat_ms);
		/* if we have a good input device, we don't need the private ioctl above */
		struct input_event ie;
		memset(&ie, 0, sizeof(ie));
		ie.type = EV_REP;
		/* increase by 10 ms to trick the repeat checker code in the
		 * rcinput loop into accepting the key event... */
		ie.value = start_ms + 10;
		ie.code = REP_DELAY;
		if (write(fd, &ie, sizeof(ie)) == -1)
			printf("[rcinput:%s] %s(fd %d) write %s: %m\n", __func__, path.c_str(), fd, "REP_DELAY");

		ie.value = repeat_ms + 10;
		ie.code = REP_PERIOD;
		if (write(fd, &ie, sizeof(ie)) == -1)
			printf("[rcinput:%s] %s(fd %d) write %s: %m\n", __func__, path.c_str(), fd, "REP_PERIOD");
	}
}

#ifdef IOC_IR_SET_PRI_PROTOCOL
// hint: ir_protocol_t and other useful things are defined in cs_ir_generic.h
void CRCInput::set_rc_hw(ir_protocol_t ir_protocol, unsigned int ir_address)
{
	int ioctl_ret = -1;
	if (indev.empty()) {
		printf("[rcinput:%s] indev is empty!\n", __func__);
		return;
	}
	int fd = -1;
	for (std::vector<in_dev>::iterator it = indev.begin(); it != indev.end(); ++it) {
		if ((*it).path == "/dev/input/nevis_ir") {
			fd = (*it).fd;
			break;
		}
	}
	if (fd == -1) {
		printf("[rcinput:%s] no nevis_ir input device found??\n", __func__);
		return;
	}
	ioctl_ret = ::ioctl(fd, IOC_IR_SET_PRI_PROTOCOL, ir_protocol);
	if(ioctl_ret < 0)
		perror("IOC_IR_SET_PRI_PROTOCOL");
	else
		printf("CRCInput::set_rc_hw: Set IOCTRL : IOC_IR_SET_PRI_PROTOCOL, %05X\n", ir_protocol);

	//bypass setting of IR Address with ir_address=0
	if(ir_address > 0)
	{
		//fixme?: for now fd_rc[] is hardcoded to 0 since only fd_rc[0] is used at the moment
		ioctl_ret = ::ioctl(fd, IOC_IR_SET_PRI_ADDRESS, ir_address);
		if(ioctl_ret < 0)
			perror("IOC_IR_SET_PRI_ADDRESS");
		else
			printf("CRCInput::set_rc_hw: Set IOCTRL : IOC_IR_SET_PRI_ADDRESS,  %05X\n", ir_address);
	}
}

// hint: ir_protocol_t and other useful things are defined in cs_ir_generic.h
void CRCInput::set_rc_hw(void)
{
	ir_protocol_t ir_protocol = IR_PROTOCOL_UNKNOWN;
	unsigned int ir_address = 0x00;

	switch(g_settings.remote_control_hardware)
	{
		case RC_HW_COOLSTREAM:
			ir_protocol = IR_PROTOCOL_NECE;
			ir_address  = 0xFF80;
			break;
		case RC_HW_DBOX:
			ir_protocol = IR_PROTOCOL_NRC17;
			ir_address  = 0x00C5;
			break;
		case RC_HW_PHILIPS:
			ir_protocol = IR_PROTOCOL_RC5;
			ir_address  = 0x000A;
			break;
		case RC_HW_TRIPLEDRAGON:
			ir_protocol = IR_PROTOCOL_RMAP_E;
			ir_address  = 0x000A; // with device id 0
//			ir_address  = 0x100A; // with device id 1
//			ir_address  = 0x200A; // with device id 2
//			ir_address  = 0x300A; // with device id 3
//			ir_address  = 0x400A; // with device id 4
//			ir_address  = 0x500A; // with device id 5
//			ir_address  = 0x600A; // with device id 6
//			ir_address  = 0x700A; // with device id 7
			break;
		default:
			ir_protocol = IR_PROTOCOL_NECE;
			ir_address  = 0xFF80;
	}
	
	set_rc_hw(ir_protocol, ir_address);
}
#else
void CRCInput::set_rc_hw(void)
{
}
#endif
