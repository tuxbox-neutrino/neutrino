/*
 * (C) Copyright 2011 Stefan Seyfried, B1 Systems GmbH, Vohburg, Germany
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * drivertool.c
 * Small command line tool for testing the private ioctl()s of
 * the Coolstream HD1 drivers.
 *
 * right now the frontpanel and the IR remote drivers are implemented
 */

#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cs_frontpanel.h>

#ifdef BOXMODEL_CST_HD2
#ifdef HAVE_COOLSTREAM_CS_IR_GENERIC_H
#include <cs_ir_generic.h>
#endif
#else
#ifdef HAVE_COOLSTREAM_NEVIS_IR_H
#include <nevis_ir.h>
#endif
#endif


#ifndef IOC_IR_SET_PRI_PROTOCOL
/* unfortunately, the shipped headers seem to be still incomplete...
 * documented here:
 * http://www.dbox2world.net/board293-coolstream-hd1/board314-coolstream-development/p120342-alternative-ir-codes/#post120342
 */
typedef enum
{
	IR_PROTOCOL_UNKNOWN    = 0x00000,
	IR_PROTOCOL_RMAP       = 0x00001, /* Ruwido rMAP */
	IR_PROTOCOL_RMAP_E     = 0x00002, /* Ruwido rMAP with extension for MNC Ltd sp. z o.o. */
	IR_PROTOCOL_NRC17      = 0x00004, /* Nokia NRC17 */
	IR_PROTOCOL_JVC        = 0x00008, /* JVC */
	IR_PROTOCOL_RCA        = 0x00010, /* RCA */
	IR_PROTOCOL_PSD        = 0x00020, /* Precision Squared (not yet supported) */
	IR_PROTOCOL_RC5        = 0x00040, /* Philips RC5 */
	IR_PROTOCOL_RCMM       = 0x00080, /* Philips RCMM */
	IR_PROTOCOL_RECS80     = 0x00100, /* Philips RECS80 */
	IR_PROTOCOL_NEC        = 0x00200, /* NEC */
	IR_PROTOCOL_NECE       = 0x00400, /* NEC with 16 bit address capability */
	IR_PROTOCOL_SCA        = 0x00800, /* Scientific Atlanta */
	IR_PROTOCOL_MATSUSHITA = 0x01000, /* Matsushita (Technics/Panasonics) */
	IR_PROTOCOL_SONY       = 0x02000, /* Sony SIRC 12 bit */
	IR_PROTOCOL_SONY15     = 0x04000, /* Sony SIRC 15 bit */
	IR_PROTOCOL_SONY20     = 0x08000, /* Sony SIRC 20 bit */
	IR_PROTOCOL_SONY24     = 0x10000, /* Sony SIRC 24 bit */
	IR_PROTOCOL_ALL        = 0x1FFFF
} ir_protocol_t;

typedef enum
{
	FP_MODE_KEYS_DISABLED = 0,
	FP_MODE_KEYS_ENABLED  = 1
} fp_mode_t;

/* set the IR-protocols to listen for. */
#define IOC_IR_SET_PRI_PROTOCOL _IOW(0xDD,  1, ir_protocol_t) /* set the primary IR-protocol */
#define IOC_IR_SET_SEC_PROTOCOL _IOW(0xDD,  2, ir_protocol_t) /* set the secondary IR-protocol */

/* some IR-protocols can handle different device addresses. */
#define IOC_IR_SET_PRI_ADDRESS  _IOW(0xDD,  3, unsigned int)  /* set the primary IR-address */
#define IOC_IR_SET_SEC_ADDRESS  _IOW(0xDD,  4, unsigned int)  /* set the secondary IR-address */

/* defines the delay time between two pulses in milliseconds */
#define IOC_IR_SET_F_DELAY      _IOW(0xDD,  5, unsigned int)  /* set the delay time before the first repetition */
#define IOC_IR_SET_X_DELAY      _IOW(0xDD,  6, unsigned int)  /* set the delay time between all other repetitions */

#define IOC_IR_SET_FP_MODE      _IOW(0xDD,  9, fp_mode_t)     /* switch the front panel buttons on or off */
#define IOC_IR_GET_PROTOCOLS    _IOR(0xDD, 10, ir_protocol_t) /* should report a bitmask of supported protocols */
#endif

typedef enum
{
	TYPE_CHAR,
	TYPE_CHARP,
	TYPE_UINT,
	TYPE_UINT_GET,
	TYPE_UNSUPP
} ioc_type;

struct ioctl_list
{
	int device;
	const char *text;
	long ioctl;
	ioc_type type;
};

static struct ioctl_list n[] = {
	{ 0, "FP_SET_BRIGHT",		IOC_FP_SET_BRIGHT,		TYPE_CHAR },
	{ 0, "FP_CLEAR_ALL",		IOC_FP_CLEAR_ALL,		TYPE_UINT },
	{ 0, "FP_SET_TEXT",		IOC_FP_SET_TEXT,		TYPE_CHARP },
	{ 0, "FP_SET_ICON",		IOC_FP_SET_ICON,		TYPE_UINT },
	{ 0, "FP_CLEAR_ICON",		IOC_FP_CLEAR_ICON,		TYPE_UINT },
	{ 0, "FP_SET_OUTPUT",		IOC_FP_SET_OUTPUT,		TYPE_CHAR },
	{ 0, "FP_CLEAR_OUTPUT",		IOC_FP_CLEAR_OUTPUT,		TYPE_CHAR },
	{ 0, "FP_STANDBY",		IOC_FP_STANDBY,			TYPE_UNSUPP },
	{ 0, "FP_LED_CTRL",		IOC_FP_LED_CTRL,		TYPE_CHAR },
	{ 0, "FP_GET_WAKEUP",		IOC_FP_GET_WAKEUP,		TYPE_UNSUPP },
	{ 1, "IR_SET_PRI_PROTOCOL",	IOC_IR_SET_PRI_PROTOCOL,	TYPE_UINT },
	{ 1, "IR_SET_SEC_PROTOCOL",	IOC_IR_SET_SEC_PROTOCOL,	TYPE_UINT },
	{ 1, "IR_SET_PRI_ADDRESS",	IOC_IR_SET_PRI_ADDRESS,		TYPE_UINT },
	{ 1, "IR_SET_SEC_ADDRESS",	IOC_IR_SET_SEC_ADDRESS,		TYPE_UINT },
	{ 1, "IR_SET_F_DELAY",		IOC_IR_SET_F_DELAY,		TYPE_UINT },
	{ 1, "IR_SET_X_DELAY",		IOC_IR_SET_X_DELAY,		TYPE_UINT },
	{ 1, "IR_SET_FP_MODE",		IOC_IR_SET_FP_MODE,		TYPE_UINT },
	{ 1, "IR_GET_PROTOCOLS",	IOC_IR_GET_PROTOCOLS,		TYPE_UINT_GET },
#ifdef HAVE_COOLSTREAM_CS_IR_GENERIC_H
	{ 1, "IOC_IR_SET_PRI_KEYMAP",	IOC_IR_SET_PRI_KEYMAP,		TYPE_UNSUPP },
	{ 1, "IOC_IR_SET_SEC_KEYMAP",	IOC_IR_SET_SEC_KEYMAP,		TYPE_UNSUPP },
#endif
	{ -1, NULL, 0, TYPE_UNSUPP }
};

static const char *devices[2] =
{
	"/dev/display",
	"/dev/input/nevis_ir"
};

void usage(void)
{
	int i = 0;
	printf("need arguments: <cmd> <value>\n");
	printf("possible commands and their arguments:\n");
	while (n[i].text != NULL)
	{
		const char *arg;
		switch (n[i].type)
		{
			case TYPE_UNSUPP:
				arg = "[not (yet) supported]";
				break;
			case TYPE_CHARP:
				arg = "<string>";
				break;
			case TYPE_UINT:
			case TYPE_CHAR:
				arg = "<number>";
				break;
			case TYPE_UINT_GET:
				arg = "";
				break;
			default:
				arg = "[INTERNAL ERROR]";
				break;
		}
		printf("\t%-20s %s\n", n[i++].text, arg);
	}
	printf("\n");
	printf("shortcuts:\n\t-c\tswitch primary remote to coolstream\n"
		"\t-t <n>\tswitch primary remote to Tripledragon addr <n>\n\n");
	exit(0);
}

int main(int argc, char **argv)
{
	int fd, i, ret;
	unsigned int a;

	i = 0;
	a = 0;

	if (argc < 2 || (argc > 1 && strcmp(argv[1], "-h") == 0))
		usage();

	if (argv[1][0] == '-')
	{
		ir_protocol_t p = IR_PROTOCOL_UNKNOWN;

		switch (argv[1][1])
		{
			case 't':
				if (argc < 3)
					usage(); /* does not return */
				p = IR_PROTOCOL_RMAP_E;
				a = (unsigned int) strtoll(argv[2], NULL, 0);
				a <<= 16;
				a |= 0x0A;
				break;
			case 'c':
				p = IR_PROTOCOL_NECE;
				a = 0xFF80;
				break;
			default:
				usage();
		}

		fd = open(devices[1], O_RDONLY);
		if (fd < 0)
		{
			perror(devices[1]);
			return 1;
		}
		ret = ioctl(fd, IOC_IR_SET_PRI_PROTOCOL, p);
		if (ret)
		{
			perror("IOC_IR_SET_PRI_PROTOCOL");
			ret = errno;
			close(fd);
			return ret;
		}
		ret = ioctl(fd, IOC_IR_SET_PRI_ADDRESS, a);
		if (ret)
		{
			perror("IOC_IR_SET_PRI_ADDRESS");
			ret = errno;
		}
		close(fd);
		return ret;
	}

	ret = 0;
	/* allow the old VFD_ names for backwards compatibility */
	while (n[i].text != NULL && strcmp(n[i].text, argv[1]) != 0 &&
	       !(strncmp(argv[1], "VFD_", 4) == 0 && strcmp(n[i].text + 2, argv[1] + 3) == 0))
		i++;

	if (!n[i].text || (n[i].type != TYPE_UINT_GET && argc < 3))
		usage();

	fd = open(devices[n[i].device], O_RDONLY);
	if (fd < 0)
	{
		perror(devices[n[i].device]);
		return 1;
	}

	switch (n[i].type)
	{
		case TYPE_CHARP:
			ret = ioctl(fd, n[i].ioctl, argv[2]);
			break;
		case TYPE_CHAR:
			a = (unsigned int) strtoll(argv[2], NULL, 0);
			ret = ioctl(fd, n[i].ioctl, (unsigned char)a);
			break;
		case TYPE_UINT:
			a = (unsigned int) strtoll(argv[2], NULL, 0);
			ret = ioctl(fd, n[i].ioctl, a);
			break;
		case TYPE_UINT_GET:
			/* at least IOC_IR_GET_PROTOCOLS does not seem to work:
			 * it returns always 0 */
			ret = ioctl(fd, n[i].ioctl, &a);
			if (! ret)
				printf("returns: 0x%08x\n", a);
			break;
		case TYPE_UNSUPP:
			printf("sorry, %s is not (yet) supported\n", n[i].text);
			break;
	}

	// printf("issued ioctl(fd, %s, 0x%x\n", n[i].text, a);
	// printf("      (ioctl(fd, 0x%lx, 0x%x\n", n[i].ioctl, a);

	if (ret)
	{
		perror("ioctl");
		ret = errno;
	}
	close(fd);
	return ret;
}
