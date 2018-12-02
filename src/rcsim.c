/******************************************************************************
 *	rcsim - rcsim.c
 *
 *	simulates the remote control, sends the requested key
 *
 *	(c) 2003 Carsten Juttner (carjay@gmx.net)
 *	(c) 2009 Stefan Seyfried, add code to use the neutrino socket instead
 *			of the input subsystem for dreambox / tripledragon
 *	(c) 2011 Stefan Seyfried, convert driver/rcinput.h via script to
 *			rcsim.h for automated import of new keys
 *
 * 	This program is free software; you can redistribute it and/or modify
 * 	it under the terms of the GNU General Public License as published by
 * 	The Free Software Foundation; either version 2 of the License, or
 * 	(at your option) any later version.
 *
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 *
 * 	You should have received a copy of the GNU General Public License
 * 	along with this program; if not, write to the Free Software
 * 	Foundation, 51 Franklin Street, Fifth Floor Boston, MA 02110-1301, USA.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <error.h>

#if 0
/* if you want use HAVE_XX_HARDWARE, better include config.h :-) */
#include "config.h"

#if defined(HAVE_DBOX_HARDWARE)
#define EVENTDEV "/dev/input/event0"
#elif defined (HAVE_COOL_HARDWARE)
#define EVENTDEV "/dev/input/input0"
#else
#endif
#else
/* dreambox and tripledragon do not use a "normal" input device, so we cannot
   (ab-)use the event repeating function of it. use the neutrino socket instead. */
#include <sys/socket.h>
#include <sys/un.h>
#define NEUTRINO_SOCKET "/tmp/neutrino.sock"

/* those structs / values are stolen from libeventserver */
struct eventHead
{
	unsigned int eventID;
	unsigned int initiatorID;
	unsigned int dataSize;
};

enum initiators
{
	INITID_CONTROLD,
	INITID_SECTIONSD,
	INITID_ZAPIT,
	INITID_TIMERD,
	INITID_HTTPD,
	INITID_NEUTRINO,
	INITID_GENERIC_INPUT_EVENT_PROVIDER
};
#endif

#include "rcsim.h"

void usage(char *n){
	unsigned int keynum = sizeof(keyname)/sizeof(struct key);
	unsigned int i;
#if defined (EVENTDEV)
	printf ("rcsim v1.2\nUsage: %s <keyname> [<time>] [<repeat>]\n"
		"    <keyname> is an excerpt of the 'KEY_FOO'-names in <driver/rcinput.h>,\n"
		"    <time>    is how long a code is repeatedly sent,\n"
		"              unit is seconds, default is 0 = sent only once\n"
		"    <repeat>  what time is waited until a new code is sent\n"
		"              (if <time> is greater than 0), unit is milliseconds,\n"
		"              default is 500\n\n"
		"    Example:\n"
		"        %s KEY_1\n"
		"             ; KEY_1 sent once\n"
		"        %s KEY_OK 2 250\n"
		"             ; KEY_OK sent every 250ms for 2 seconds\n\n"
		"    Keys:",n,n,n);
#else
	printf ("rcsim v1.2\nUsage: %s <keyname>\n\n"
		"    Keys:",n);
#endif
	for (i=0;i<keynum;){
		if ((i % 4) == 0)
			printf ("\n    %-20s",keyname[i++].name);
		else
			printf ("%-20s",keyname[i++].name);
	}
	printf ("\n\n");
}

/* we could also use the neutrino socket on the dbox, but this needs more testing.
   so leave it as is for now */
#if defined (EVENTDEV)
int push(int ev, unsigned int code, unsigned int value)
{
	struct input_event iev;
	iev.type=EV_KEY;
	iev.code=code;
	iev.value=value;
	return write (ev,&iev,sizeof(iev));
}
#else
int push(int ev, unsigned int code, unsigned int value)
{
	struct eventHead eh;
	struct sockaddr_un servaddr;
	int clilen, fd;
	const char *errmsg;

	/* ev is unused - stupid compiler... */
	fd = ev;
	/* ignore for now, neutrino does not handle this anyway */
	if (value == KEY_RELEASED)
		return 0;

	memset(&servaddr, 0, sizeof(struct sockaddr_un));
	servaddr.sun_family = AF_UNIX;
	strncpy(servaddr.sun_path, NEUTRINO_SOCKET, sizeof(servaddr.sun_path) - 1);
	clilen = sizeof(servaddr.sun_family) + strlen(servaddr.sun_path);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		perror("cannot open socket " NEUTRINO_SOCKET);
		return fd;
	}

	if (connect(fd, (struct sockaddr*) &servaddr, clilen) < 0)
	{
		errmsg = "connect " NEUTRINO_SOCKET;
		goto error;
	}

	eh.initiatorID = INITID_GENERIC_INPUT_EVENT_PROVIDER;
	eh.eventID = 0; // data field
	eh.dataSize = sizeof(int);
#if 0
	/* neutrino-hd does not yet do REPEAT or RELEASE AFAICT. */
	if (value == KEY_AUTOREPEAT)
		code |= 0x0400; // neutrino:CRCInput::RC_repeat
	if (value == KEY_RELEASED)
		code |= 0x0800; // neutrino:CRCInput::RC_release
#endif

	if (write(fd, &eh, sizeof(eh)) < 0)
	{
		errmsg = "write() eventHead";
		goto error;
	}

	if (write(fd, &code, sizeof(code)) < 0)
	{
		errmsg = "write() event";
		goto error;
	}

	close(fd);
	return 0;

 error:
	perror(errmsg);
	close(fd);
	return -1;
}
#endif

int main (int argc, char **argv){
	int evd;
	unsigned long sendcode=KEY_0;
	unsigned int keys = sizeof(keyname)/sizeof(struct key);
	unsigned long rctime=0;
	unsigned long reptime=500;
	unsigned int offset;

	if (argc<2||argc>4){
		usage(argv[0]);
		return 1;
	}

	if (argc==2)
		if (!strncmp(argv[1],"--help",6)||!strncmp(argv[1],"-h",2)){
			usage(argv[0]);
			return 1;
		}

	for (offset=0;offset<keys;offset++){
		if (!strcmp(argv[1],keyname[offset].name)){
			sendcode = keyname[offset].code;
			break;
		}
	}

	if (offset==keys){
		printf ("keyname '%s' not found in list\n",argv[1]);
		return 1;
	}

	if (argc==4){
		reptime=atol (argv[3]);
	}

	if (argc>=3){
		rctime=(atol (argv[2])*1000)/reptime;
	}

#if defined (EVENTDEV)
	evd=open (EVENTDEV,O_RDWR);
	if (evd<0){
		perror ("opening " EVENTDEV " failed");
		return 1;
	}
#else
	evd = -1; // close(-1) does not harm... ;)
#endif
	printf ("sending key %s for %ld seconds\n",keyname[offset].name,(reptime*rctime)/1000);
	if (push (evd,sendcode,KEY_PRESSED)<0){
		perror ("writing 'key_pressed' event failed");
		close (evd);
		return 1;
	}
	while (rctime--){
		usleep(reptime*1000);
		if (push (evd,sendcode,KEY_AUTOREPEAT)<0){
			perror ("writing 'key_autorepeat' event failed");
			close (evd);
			return 1;
		}
	}
	if (push (evd,sendcode,KEY_RELEASED)<0){
		perror ("writing 'key_released' event failed");
		close (evd);
		return 1;
	}
	close (evd);
	return 0;
}

