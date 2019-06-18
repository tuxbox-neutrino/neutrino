/*
        Copyright (C) 2013 CoolStream International Ltd

	Based on iwlist.c, Copyright (c) 1997-2007 Jean Tourrilhes <jt@hpl.hp.com>

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

/* something is fishy with ppc compilers: if vector gets included later from
 * iwscan.h it blows up in the c++config.h libstdc++ header...
 * probably caused by the redefinition of the "inline" keyword in iwlib.h. */
#include <vector>

#include <sys/time.h>
extern "C" {
#include "iwlib.h"              /* Header */
}

#include <config.h>
#include "iwscan.h"

static void device_up(std::string dev)
{
	int sockfd;
	struct ifreq ifr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0) {
		perror("socket");
		return;
	}

	memset(&ifr, 0, sizeof ifr);

	strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0)
		perror("SIOCGIFFLAGS");

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0)
		perror("SIOCSIFFLAGS");

	close(sockfd);
}

bool get_wlan_list(std::string dev, std::vector<wlan_network> &networks)
{
	int skfd;
	struct iwreq          wrq;
	unsigned char *       buffer = NULL;          /* Results */
	int                   buflen = IW_SCAN_MAX_DATA; /* Min for compat WE<17 */
	struct iw_range       range;
	int                   has_range;
	struct timeval        tv;                             /* Select timeout */
	int                   timeout = 15000000;             /* 15s */
	const char * ifname = dev.c_str();

	networks.clear();
	device_up(dev);

	if((skfd = iw_sockets_open()) < 0)
	{
		perror("socket");
		return false;
	}

	/* Get range stuff */
	has_range = (iw_get_range_info(skfd, ifname, &range) >= 0);

	/* Check if the interface could support scanning. */
	if((!has_range) || (range.we_version_compiled < 14))
	{
		fprintf(stderr, "%-8.16s  Interface doesn't support scanning.\n\n",
				ifname);
		goto _return;
	}

	/* Init timeout value -> 250ms between set and first get */
	tv.tv_sec = 0;
	tv.tv_usec = 250000;

	wrq.u.data.pointer = NULL;
	wrq.u.data.flags = 0;
	wrq.u.data.length = 0;

	/* Initiate Scanning */
	if(iw_set_ext(skfd, ifname, SIOCSIWSCAN, &wrq) < 0)
	{
		if(errno != EPERM)
		{
			fprintf(stderr, "%-8.16s  Interface doesn't support scanning : %s\n\n",
					ifname, strerror(errno));
			goto _return;
		}
		/* If we don't have the permission to initiate the scan, we may
		 * still have permission to read left-over results.
		 * But, don't wait !!! */
		tv.tv_usec = 0;
	}
	timeout -= tv.tv_usec;

	/* Forever */
	while(1)
	{
		fd_set            rfds;           /* File descriptors for select */
		int               last_fd;        /* Last fd */
		int               ret;

		/* Guess what ? We must re-generate rfds each time */
		FD_ZERO(&rfds);
		last_fd = -1;

		/* In here, add the rtnetlink fd in the list */

		/* Wait until something happens */
		ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

		/* Check if there was an error */
		if(ret < 0)
		{
			if(errno == EAGAIN || errno == EINTR)
				continue;
			fprintf(stderr, "Unhandled signal - exiting...\n");
			goto _return;
		}

		/* Check if there was a timeout */
		if(ret == 0)
		{
			unsigned char *       newbuf;

realloc:
			/* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
			newbuf = (unsigned char *) realloc(buffer, buflen);
			if(newbuf == NULL)
			{
				fprintf(stderr, "%s: Allocation failed\n", __FUNCTION__);
				goto _return;
			}
			buffer = newbuf;

			/* Try to read the results */
			wrq.u.data.pointer = buffer;
			wrq.u.data.flags = 0;
			wrq.u.data.length = buflen;
			if(iw_get_ext(skfd, ifname, SIOCGIWSCAN, &wrq) < 0)
			{
				/* Check if buffer was too small (WE-17 only) */
				if((errno == E2BIG) && (range.we_version_compiled > 16))
				{
					/* Some driver may return very large scan results, either
					 * because there are many cells, or because they have many
					 * large elements in cells (like IWEVCUSTOM). Most will
					 * only need the regular sized buffer. We now use a dynamic
					 * allocation of the buffer to satisfy everybody. Of course,
					 * as we don't know in advance the size of the array, we try
					 * various increasing sizes. Jean II */

					/* Check if the driver gave us any hints. */
					if(wrq.u.data.length > buflen)
						buflen = wrq.u.data.length;
					else
						buflen *= 2;

					/* Try again */
					goto realloc;
				}

				/* Check if results not available yet */
				if(errno == EAGAIN)
				{
					/* Restart timer for only 100ms*/
					tv.tv_sec = 0;
					tv.tv_usec = 100000;
					timeout -= tv.tv_usec;
					if(timeout > 0)
						continue;   /* Try again later */
				}

				/* Bad error */
				fprintf(stderr, "%-8.16s  Failed to read scan data : %s\n\n",
						ifname, strerror(errno));
				goto _return;
			}
			else
				/* We have the results, go to process them */
				break;
		}

		/* In here, check if event and event type
		 * if scan event, read results. All errors bad & no reset timeout */
	}

	if(wrq.u.data.length)
	{
		struct iw_event           iwe;
		struct iw_event * event = &iwe;
		struct stream_descr       stream;
		int                       ret;

		printf("%-8.16s  Scan completed :\n", ifname);
		iw_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
		int count = -1;
		while(1)
		{
			wlan_network network;
			/* Extract an event and print it */
			ret = iw_extract_event_stream(&stream, &iwe,
					range.we_version_compiled);
			if (ret <= 0)
				break;
			switch(event->cmd) {
				case SIOCGIWAP:
					count++;
					networks.push_back(network);
					printf("          Network %d:\n", count+1);
					break;
				case SIOCGIWESSID:
					{
						char essid[IW_ESSID_MAX_SIZE+1];
						memset(essid, '\0', sizeof(essid));
						if((event->u.essid.pointer) && (event->u.essid.length))
							memcpy(essid, event->u.essid.pointer, event->u.essid.length);
						if(event->u.essid.flags)
						{
							printf("                    ESSID:\"%s\"\n", essid);
							networks[count].ssid = essid;
						}
						else {
							printf("                    ESSID:off/any/hidden\n");
							networks[count].ssid = "(hidden)";
						}
					}

					break;
				case SIOCGIWFREQ:
					{
						double          freq;                   /* Frequency/channel */
						int             channel = -1;           /* Converted to channel */
						char          buf[128];
						freq = iw_freq2float(&(event->u.freq));
						/* Convert to channel if possible */
						if(has_range)
							channel = iw_freq_to_channel(freq, &range);
						iw_print_freq(buf, sizeof(buf),
								freq, channel, event->u.freq.flags);
#if 0
						if ((channel < 0) && (freq < KILO))
							channel = freq;
#endif
						printf("                    %s\n", buf);
						networks[count].channel = buf;
					}
					break;
				case IWEVQUAL:
					{
						char          buf[128];
						iw_print_stats(buf, sizeof(buf),
								&event->u.qual, &range, has_range);
						printf("                    %s\n", buf);
						networks[count].qual = buf;
						std::size_t found = networks[count].qual.find_first_of(' ');
						if (found != std::string::npos)
							networks[count].qual = networks[count].qual.substr(0, found);
					}
					break;
				case SIOCGIWENCODE:
					if(event->u.data.flags & IW_ENCODE_DISABLED)
						networks[count].encrypted = 0;
					else
						networks[count].encrypted = 1;

					printf("                    Encryption key:%s\n", networks[count].encrypted ? "on" : "off");
					break;
			}
		}
		printf("\n");
	}
	else
		printf("%-8.16s  No scan results\n\n", ifname);

_return:
	if (buffer)
		free(buffer);
	iw_sockets_close(skfd);

	return !networks.empty();
}
