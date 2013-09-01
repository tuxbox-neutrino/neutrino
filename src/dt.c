/*
        Copyright (C) 2013 CoolStream International Ltd

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <time.h>

#include "cs_frontpanel.h"

int main(int argc, char **argv)
{
    int x, b, f;
    int fd;
    char s[1024];
    time_t t;
    struct tm *tmp;
    unsigned int timer;

    if (argc > 1)
    {
	fd = open("/dev/display", O_RDONLY);
	if (fd < 0)
	{
	    perror("/dev/display");
	    return -fd;
	}

	memset(s, 0, 1024);

	for (x = 1; x < argc; x++)
	{
	    if (argv[x][0] == '-')
	    {
		if (sscanf(argv[x], "-b%d", &b) == 1)
		{
		    if ((b < 16) && (b >= 0))
		    {
			if (ioctl(fd, IOC_FP_SET_BRIGHT, (unsigned char)  b))
			    perror("IOC_FP_SET_BRIGHT");
		    }
		    else
		        printf("Error: brightness is out of range (0 ... 16)\n\n");
		}
		else if (sscanf(argv[x], "-s%X:%X:%X", &b, &f, &timer) == 3)
		{
		    if ((b < 16) && (b >= 0))
		    {
			fp_standby_data_t standby;
			char h[3], m[3];

			t = time(NULL);
			tmp = localtime(&t);
			if (tmp == NULL) {
			    perror("localtime");
			}
			if (strftime(h, sizeof(h), "%_H", tmp) == 0) {
			    fprintf(stderr, "strftime returned 0\n");
			}
			if (strftime(m, sizeof(m), "%_M", tmp) == 0) {
			    fprintf(stderr, "strftime returned 0\n");
			}

			standby.brightness          = b;
			standby.flags               = f & 0xFF;
			standby.current_hour        = atoi(h);
			standby.current_minute      = atoi(m);
			standby.timer_minutes_hi    = timer >> 8;
			standby.timer_minutes_lo    = timer & 0xFF;
			printf("brightness       %d\n", standby.brightness);
			printf("flags            %02X\n", standby.flags);
			printf("current_hour     %d\n", standby.current_hour);
			printf("current_minute   %d\n", standby.current_minute);
			printf("timer_minutes_hi %d\n", standby.timer_minutes_hi);
			printf("timer_minutes_lo %d\n", standby.timer_minutes_lo);

			if (ioctl(fd, IOC_FP_STANDBY, (fp_standby_data_t *)  &standby))
			    perror("IOC_FP_STANDBY");
		    }
		    else
		        printf("Error: brightness is out of range (0 ... 16)\n\n");
		}
		else if (sscanf(argv[x], "-is%X", &b) == 1)
		{
		    if (ioctl(fd, IOC_FP_SET_ICON, (unsigned int)  b))
		        perror("IOC_FP_SET_ICON");
		}
		else if (sscanf(argv[x], "-ic%X", &b) == 1)
		{
		    if (ioctl(fd, IOC_FP_CLEAR_ICON, (unsigned int)  b))
		        perror("IOC_FP_CLEAR_ICON");
		}

		else if (sscanf(argv[x], "-ps%X", &b) == 1)
		{
		    if (ioctl(fd, IOC_FP_SET_OUTPUT, (unsigned char)  b))
		        perror("IOC_FP_SET_OUTPUT");
		}
		else if (sscanf(argv[x], "-pc%X", &b) == 1)
		{
		    if (ioctl(fd, IOC_FP_CLEAR_OUTPUT, (unsigned char)  b))
		        perror("IOC_FP_CLEAR_OUTPUT");
		}

		else if (sscanf(argv[x], "-t%[^\n]", s) == 1)
		{
		    if (ioctl(fd, IOC_FP_SET_TEXT, s))
		        perror("IOC_FP_SET_TEXT");
		}
		else if (sscanf(argv[x], "-ls%X", &b) == 1)
		{
		    if ((b > 0) || (b <= 8)) {
			    if (ioctl(fd, IOC_FP_LED_CTRL, b | 0x80))
			        perror("IOC_FP_LED_CTRL");
		    }
		}
		else if (sscanf(argv[x], "-lc%X", &b) == 1)
		{
		    if ((b > 0) || (b <= 8)) {
			    if (ioctl(fd, IOC_FP_LED_CTRL, b))
			        perror("IOC_FP_LED_CTRL");
		    }
		}
		else if (sscanf(argv[x], "-st%X", &b) == 1)
		{
		    fp_standby_cmd_data_t pwr;
		    pwr.addr = 0;
		    pwr.cmd = b;
		    printf("standby command: %x\n", pwr.cmd);fflush(stdout);

		    if (ioctl(fd, IOC_FP_STANDBY_CMD, &pwr))
			    perror("IOC_FP_STANDBY_CMD");
		}

		else if (argv[x][1] == 'c')
		{
		    if (strlen(argv[x]) == 2)
		    {
			if (ioctl(fd, IOC_FP_CLEAR_ALL, NULL))
			    perror("IOC_FP_CLEAR_ALL");
		    }
		    else
		    {
			if (ioctl(fd, IOC_FP_SET_TEXT, NULL))
		    	    perror("IOC_FP_SET_TEXT");
		    }
		}
	    }
	}
	close(fd);
    }
    else
    {
	printf("%s\n", argv[0]);
	printf("dt - displaytest usage:\n"
	       "  -b<n>  set display brightness between 0 and 15\n"
	       "  -t<s>  write a UTF-8 text to the display\n"
	       "  -c     clear entire display\n"
	       "  -ct    clear text on the display\n"
	       "  -is<n> set icon\n"
	       "  -ic<n> clear icon\n"
	       "  -ps<n> set port\n"
	       "  -pc<n> clear port\n"
	       "  -ls<n> led n on\n"
	       "  -lc<n> led n off\n"
	       "  -s<n>:<n>:<n> sets the box into standby (brightness, flags, timer)\n"
	       "\n");
    }
    return 0;
}
