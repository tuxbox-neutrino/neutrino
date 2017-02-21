/******************************************************************************
 * definitions for plugin and lib                                             *
 ******************************************************************************/
#define TUXTXT_COMPRESS 2 // compress page data: 0 no compression, 1 with zlib, 2 with own algorithm

#include <config.h>

#define FLOFSIZE 4

#define PAGESIZE (40*25)


enum /* page function */
{
	FUNC_LOP = 0, /* Basic Level 1 Teletext page (LOP) */
	FUNC_DATA, /* Data broadcasting page coded according to EN 300 708 [2] clause 4 */
	FUNC_GPOP, /* Global Object definition page (GPOP) - (see clause 10.5.1) */
	FUNC_POP, /* Normal Object definition page (POP) - (see clause 10.5.1) */
	FUNC_GDRCS, /* Global DRCS downloading page (GDRCS) - (see clause 10.5.2) */
	FUNC_DRCS, /* Normal DRCS downloading page (DRCS) - (see clause 10.5.2) */
	FUNC_MOT, /* Magazine Organization table (MOT) - (see clause 10.6) */
	FUNC_MIP, /* Magazine Inventory page (MIP) - (see clause 11.3) */
	FUNC_BTT, /* Basic TOP table (BTT) } */
	FUNC_AIT, /* Additional Information Table (AIT) } (see clause 11.2) */
	FUNC_MPT, /* Multi-page table (MPT) } */
	FUNC_MPTEX, /* Multi-page extension table (MPT-EX) } */
	FUNC_TRIGGER /* Page contain trigger messages defined according to [8] */
};


/* struct for (G)POP/(G)DRCS links for level 2.5, allocated at reception of p27/4 or /5, initialized with 0 after allocation */
typedef struct
{
	short page; /* linked page number */
	unsigned short subpage; /* 1 bit for each needed (1) subpage */
	unsigned char l25:1; /* 1: page required at level 2.5 */
	unsigned char l35:1; /* 1: page required at level 3.5 */
	unsigned char drcs:1; /* 1: link to (G)DRCS, 0: (G)POP */
	unsigned char local:1; /* 1: global (G*), 0: local */
} tstp27;

/* struct for extension data for level 2.5, allocated at reception, initialized with 0 after allocation */
typedef struct
{
	unsigned char *p26[16]; /* array of pointers to max. 16 designation codes of packet 26 */
	tstp27 *p27; /* array of 4 structs for (G)POP/(G)DRCS links for level 2.5 */
	unsigned short bgr[16]; /* CLUT 2+3, 2*8 colors, 0x0bgr */
	unsigned char DefaultCharset:7; /* default G0/G2 charset + national option */
	unsigned char LSP:1; /* 1: left side panel to be displayed */
	unsigned char SecondCharset:7; /* second G0 charset */
	unsigned char RSP:1; /* 1: right side panel to be displayed */
	unsigned char DefScreenColor:5; /* default screen color (above and below lines 0..24) */
	unsigned char ColorTableRemapping:3; /* 1: index in table of CLUTs to use */
	unsigned char DefRowColor:5; /* default row color (left and right to lines 0..24) */
	unsigned char BlackBgSubst:1; /* 1: substitute black background (as result of start-of-line or 1c, not 00/10+1d) */
	unsigned char SPL25:1; /* 1: side panel required at level 2.5 */
	unsigned char p28Received:1; /* 1: extension data valid (p28/0 received) */
	unsigned char LSPColumns:4; /* number of columns in left side panel, 0->16, rsp=16-lsp */
} tstExtData;


/* struct for pageinfo, max. 16 Bytes, at beginning of each cached page buffer, initialized with 0 after allocation */
typedef struct
{
	unsigned char *p24; /* pointer to lines 25+26 (packets 24+25) (2*40 bytes) for FLOF or level 2.5 data */
	tstExtData *ext; /* pointer to array[16] of data for level 2.5 */
	unsigned char boxed         :1; /* p0: boxed (newsflash or subtitle) */
	unsigned char nationalvalid :1; /* p0: national option character subset is valid (no biterror detected) */
	unsigned char national      :3; /* p0: national option character subset */
	unsigned char function      :3; /* p28/0: page function */
} tstPageinfo;

/* one cached page: struct for pageinfo, 24 lines page data */
typedef struct
{
	tstPageinfo pageinfo;
	unsigned char p0[24]; /* packet 0: center of headline */
#if TUXTXT_COMPRESS == 1
	unsigned char * pData;/* packet 1-23 */
	unsigned short ziplen;
#elif TUXTXT_COMPRESS == 2
	unsigned char * pData;/* packet 1-23 */
	unsigned char bitmask[23*5];
#else
	unsigned char data[23*40];	/* packet 1-23 */
#endif
} tstCachedPage;

/* main data structure */
typedef struct
{
	short flofpages[0x900][FLOFSIZE];
	unsigned char adip[0x900][13];
	unsigned char subpagetable[0x900];
	int vtxtpid;
	int cached_pages, page, subpage, pageupdate,page_receiving, current_page[9], current_subpage[9];
	int receiving, thread_starting, zap_subpage_manual;
	char bttok;
	int adippg[10];
	int maxadippg;
	unsigned char basictop[0x900];

	unsigned char  timestring[8];
	/* cachetable for packets 29 (one for each magazine) */
	tstExtData *astP29[9];
	/* cachetable */
	tstCachedPage *astCachetable[0x900][0x80];

	pthread_t thread_id;
	void *thread_result;
} tuxtxt_cache_struct;

/* hamming table */
const unsigned char dehamming[] =
{
	0x01, 0xFF, 0x01, 0x01, 0xFF, 0x00, 0x01, 0xFF, 0xFF, 0x02, 0x01, 0xFF, 0x0A, 0xFF, 0xFF, 0x07,
	0xFF, 0x00, 0x01, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x06, 0xFF, 0xFF, 0x0B, 0xFF, 0x00, 0x03, 0xFF,
	0xFF, 0x0C, 0x01, 0xFF, 0x04, 0xFF, 0xFF, 0x07, 0x06, 0xFF, 0xFF, 0x07, 0xFF, 0x07, 0x07, 0x07,
	0x06, 0xFF, 0xFF, 0x05, 0xFF, 0x00, 0x0D, 0xFF, 0x06, 0x06, 0x06, 0xFF, 0x06, 0xFF, 0xFF, 0x07,
	0xFF, 0x02, 0x01, 0xFF, 0x04, 0xFF, 0xFF, 0x09, 0x02, 0x02, 0xFF, 0x02, 0xFF, 0x02, 0x03, 0xFF,
	0x08, 0xFF, 0xFF, 0x05, 0xFF, 0x00, 0x03, 0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x03, 0xFF, 0x03, 0x03,
	0x04, 0xFF, 0xFF, 0x05, 0x04, 0x04, 0x04, 0xFF, 0xFF, 0x02, 0x0F, 0xFF, 0x04, 0xFF, 0xFF, 0x07,
	0xFF, 0x05, 0x05, 0x05, 0x04, 0xFF, 0xFF, 0x05, 0x06, 0xFF, 0xFF, 0x05, 0xFF, 0x0E, 0x03, 0xFF,
	0xFF, 0x0C, 0x01, 0xFF, 0x0A, 0xFF, 0xFF, 0x09, 0x0A, 0xFF, 0xFF, 0x0B, 0x0A, 0x0A, 0x0A, 0xFF,
	0x08, 0xFF, 0xFF, 0x0B, 0xFF, 0x00, 0x0D, 0xFF, 0xFF, 0x0B, 0x0B, 0x0B, 0x0A, 0xFF, 0xFF, 0x0B,
	0x0C, 0x0C, 0xFF, 0x0C, 0xFF, 0x0C, 0x0D, 0xFF, 0xFF, 0x0C, 0x0F, 0xFF, 0x0A, 0xFF, 0xFF, 0x07,
	0xFF, 0x0C, 0x0D, 0xFF, 0x0D, 0xFF, 0x0D, 0x0D, 0x06, 0xFF, 0xFF, 0x0B, 0xFF, 0x0E, 0x0D, 0xFF,
	0x08, 0xFF, 0xFF, 0x09, 0xFF, 0x09, 0x09, 0x09, 0xFF, 0x02, 0x0F, 0xFF, 0x0A, 0xFF, 0xFF, 0x09,
	0x08, 0x08, 0x08, 0xFF, 0x08, 0xFF, 0xFF, 0x09, 0x08, 0xFF, 0xFF, 0x0B, 0xFF, 0x0E, 0x03, 0xFF,
	0xFF, 0x0C, 0x0F, 0xFF, 0x04, 0xFF, 0xFF, 0x09, 0x0F, 0xFF, 0x0F, 0x0F, 0xFF, 0x0E, 0x0F, 0xFF,
	0x08, 0xFF, 0xFF, 0x05, 0xFF, 0x0E, 0x0D, 0xFF, 0xFF, 0x0E, 0x0F, 0xFF, 0x0E, 0x0E, 0xFF, 0x0E
};

/* odd parity table, error=0x20 (space) */
const unsigned char deparity[] =
{
	' ' , 0x01, 0x02, ' ' , 0x04, ' ' , ' ' , 0x07, 0x08, ' ' , ' ' , 0x0b, ' ' , 0x0d, 0x0e, ' ' ,
	0x10, ' ' , ' ' , 0x13, ' ' , 0x15, 0x16, ' ' , ' ' , 0x19, 0x1a, ' ' , 0x1c, ' ' , ' ' , 0x1f,
	0x20, ' ' , ' ' , 0x23, ' ' , 0x25, 0x26, ' ' , ' ' , 0x29, 0x2a, ' ' , 0x2c, ' ' , ' ' , 0x2f,
	' ' , 0x31, 0x32, ' ' , 0x34, ' ' , ' ' , 0x37, 0x38, ' ' , ' ' , 0x3b, ' ' , 0x3d, 0x3e, ' ' ,
	0x40, ' ' , ' ' , 0x43, ' ' , 0x45, 0x46, ' ' , ' ' , 0x49, 0x4a, ' ' , 0x4c, ' ' , ' ' , 0x4f,
	' ' , 0x51, 0x52, ' ' , 0x54, ' ' , ' ' , 0x57, 0x58, ' ' , ' ' , 0x5b, ' ' , 0x5d, 0x5e, ' ' ,
	' ' , 0x61, 0x62, ' ' , 0x64, ' ' , ' ' , 0x67, 0x68, ' ' , ' ' , 0x6b, ' ' , 0x6d, 0x6e, ' ' ,
	0x70, ' ' , ' ' , 0x73, ' ' , 0x75, 0x76, ' ' , ' ' , 0x79, 0x7a, ' ' , 0x7c, ' ' , ' ' , 0x7f,
	0x00, ' ' , ' ' , 0x03, ' ' , 0x05, 0x06, ' ' , ' ' , 0x09, 0x0a, ' ' , 0x0c, ' ' , ' ' , 0x0f,
	' ' , 0x11, 0x12, ' ' , 0x14, ' ' , ' ' , 0x17, 0x18, ' ' , ' ' , 0x1b, ' ' , 0x1d, 0x1e, ' ' ,
	' ' , 0x21, 0x22, ' ' , 0x24, ' ' , ' ' , 0x27, 0x28, ' ' , ' ' , 0x2b, ' ' , 0x2d, 0x2e, ' ' ,
	0x30, ' ' , ' ' , 0x33, ' ' , 0x35, 0x36, ' ' , ' ' , 0x39, 0x3a, ' ' , 0x3c, ' ' , ' ' , 0x3f,
	' ' , 0x41, 0x42, ' ' , 0x44, ' ' , ' ' , 0x47, 0x48, ' ' , ' ' , 0x4b, ' ' , 0x4d, 0x4e, ' ' ,
	0x50, ' ' , ' ' , 0x53, ' ' , 0x55, 0x56, ' ' , ' ' , 0x59, 0x5a, ' ' , 0x5c, ' ' , ' ' , 0x5f,
	0x60, ' ' , ' ' , 0x63, ' ' , 0x65, 0x66, ' ' , ' ' , 0x69, 0x6a, ' ' , 0x6c, ' ' , ' ' , 0x6f,
	' ' , 0x71, 0x72, ' ' , 0x74, ' ' , ' ' , 0x77, 0x78, ' ' , ' ' , 0x7b, ' ' , 0x7d, 0x7e, ' ' ,
};

#if 1	/* lookup-table algorithm for decoding Hamming 24/18, credits to: */
/*
 *  libzvbi - Error correction functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 *  [AleVT]
 *
 *  This table generates the parity checks for hamm24/18 decoding.
 *  Bit 0 is for test A, 1 for B, ...
 *
 *  Thanks to R. Gancarz for this fine table *g*
 */
const unsigned char hamm24par[3][256] = {
    {
        /* Parities of first byte */
	 0, 33, 34,  3, 35,  2,  1, 32, 36,  5,  6, 39,  7, 38, 37,  4,
	37,  4,  7, 38,  6, 39, 36,  5,  1, 32, 35,  2, 34,  3,  0, 33,
	38,  7,  4, 37,  5, 36, 39,  6,  2, 35, 32,  1, 33,  0,  3, 34,
	 3, 34, 33,  0, 32,  1,  2, 35, 39,  6,  5, 36,  4, 37, 38,  7,
	39,  6,  5, 36,  4, 37, 38,  7,  3, 34, 33,  0, 32,  1,  2, 35,
	 2, 35, 32,  1, 33,  0,  3, 34, 38,  7,  4, 37,  5, 36, 39,  6,
	 1, 32, 35,  2, 34,  3,  0, 33, 37,  4,  7, 38,  6, 39, 36,  5,
	36,  5,  6, 39,  7, 38, 37,  4,  0, 33, 34,  3, 35,  2,  1, 32,
	40,  9, 10, 43, 11, 42, 41,  8, 12, 45, 46, 15, 47, 14, 13, 44,
	13, 44, 47, 14, 46, 15, 12, 45, 41,  8, 11, 42, 10, 43, 40,  9,
	14, 47, 44, 13, 45, 12, 15, 46, 42, 11,  8, 41,  9, 40, 43, 10,
	43, 10,  9, 40,  8, 41, 42, 11, 15, 46, 45, 12, 44, 13, 14, 47,
	15, 46, 45, 12, 44, 13, 14, 47, 43, 10,  9, 40,  8, 41, 42, 11,
	42, 11,  8, 41,  9, 40, 43, 10, 14, 47, 44, 13, 45, 12, 15, 46,
	41,  8, 11, 42, 10, 43, 40,  9, 13, 44, 47, 14, 46, 15, 12, 45,
	12, 45, 46, 15, 47, 14, 13, 44, 40,  9, 10, 43, 11, 42, 41,  8
    }, {
        /* Parities of second byte */
	 0, 41, 42,  3, 43,  2,  1, 40, 44,  5,  6, 47,  7, 46, 45,  4,
	45,  4,  7, 46,  6, 47, 44,  5,  1, 40, 43,  2, 42,  3,  0, 41,
	46,  7,  4, 45,  5, 44, 47,  6,  2, 43, 40,  1, 41,  0,  3, 42,
	 3, 42, 41,  0, 40,  1,  2, 43, 47,  6,  5, 44,  4, 45, 46,  7,
	47,  6,  5, 44,  4, 45, 46,  7,  3, 42, 41,  0, 40,  1,  2, 43,
	 2, 43, 40,  1, 41,  0,  3, 42, 46,  7,  4, 45,  5, 44, 47,  6,
	 1, 40, 43,  2, 42,  3,  0, 41, 45,  4,  7, 46,  6, 47, 44,  5,
	44,  5,  6, 47,  7, 46, 45,  4,  0, 41, 42,  3, 43,  2,  1, 40,
	48, 25, 26, 51, 27, 50, 49, 24, 28, 53, 54, 31, 55, 30, 29, 52,
	29, 52, 55, 30, 54, 31, 28, 53, 49, 24, 27, 50, 26, 51, 48, 25,
	30, 55, 52, 29, 53, 28, 31, 54, 50, 27, 24, 49, 25, 48, 51, 26,
	51, 26, 25, 48, 24, 49, 50, 27, 31, 54, 53, 28, 52, 29, 30, 55,
	31, 54, 53, 28, 52, 29, 30, 55, 51, 26, 25, 48, 24, 49, 50, 27,
	50, 27, 24, 49, 25, 48, 51, 26, 30, 55, 52, 29, 53, 28, 31, 54,
	49, 24, 27, 50, 26, 51, 48, 25, 29, 52, 55, 30, 54, 31, 28, 53,
	28, 53, 54, 31, 55, 30, 29, 52, 48, 25, 26, 51, 27, 50, 49, 24
    }, {
        /* Parities of third byte */
	63, 14, 13, 60, 12, 61, 62, 15, 11, 58, 57,  8, 56,  9, 10, 59,
	10, 59, 56,  9, 57,  8, 11, 58, 62, 15, 12, 61, 13, 60, 63, 14,
	 9, 56, 59, 10, 58, 11,  8, 57, 61, 12, 15, 62, 14, 63, 60, 13,
	60, 13, 14, 63, 15, 62, 61, 12,  8, 57, 58, 11, 59, 10,  9, 56,
	 8, 57, 58, 11, 59, 10,  9, 56, 60, 13, 14, 63, 15, 62, 61, 12,
	61, 12, 15, 62, 14, 63, 60, 13,  9, 56, 59, 10, 58, 11,  8, 57,
	62, 15, 12, 61, 13, 60, 63, 14, 10, 59, 56,  9, 57,  8, 11, 58,
	11, 58, 57,  8, 56,  9, 10, 59, 63, 14, 13, 60, 12, 61, 62, 15,
	31, 46, 45, 28, 44, 29, 30, 47, 43, 26, 25, 40, 24, 41, 42, 27,
	42, 27, 24, 41, 25, 40, 43, 26, 30, 47, 44, 29, 45, 28, 31, 46,
	41, 24, 27, 42, 26, 43, 40, 25, 29, 44, 47, 30, 46, 31, 28, 45,
	28, 45, 46, 31, 47, 30, 29, 44, 40, 25, 26, 43, 27, 42, 41, 24,
	40, 25, 26, 43, 27, 42, 41, 24, 28, 45, 46, 31, 47, 30, 29, 44,
	29, 44, 47, 30, 46, 31, 28, 45, 41, 24, 27, 42, 26, 43, 40, 25,
	30, 47, 44, 29, 45, 28, 31, 46, 42, 27, 24, 41, 25, 40, 43, 26,
	43, 26, 25, 40, 24, 41, 42, 27, 31, 46, 45, 28, 44, 29, 30, 47
    }
};

/*
 *  [AleVT]
 *
 *  Table to extract the lower 4 bit from hamm24/18 encoded bytes
 */
const unsigned char hamm24val[256] = {
      0,  0,  0,  0,  1,  1,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,
      2,  2,  2,  2,  3,  3,  3,  3,  2,  2,  2,  2,  3,  3,  3,  3,
      4,  4,  4,  4,  5,  5,  5,  5,  4,  4,  4,  4,  5,  5,  5,  5,
      6,  6,  6,  6,  7,  7,  7,  7,  6,  6,  6,  6,  7,  7,  7,  7,
      8,  8,  8,  8,  9,  9,  9,  9,  8,  8,  8,  8,  9,  9,  9,  9,
     10, 10, 10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11, 11, 11,
     12, 12, 12, 12, 13, 13, 13, 13, 12, 12, 12, 12, 13, 13, 13, 13,
     14, 14, 14, 14, 15, 15, 15, 15, 14, 14, 14, 14, 15, 15, 15, 15,
      0,  0,  0,  0,  1,  1,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,
      2,  2,  2,  2,  3,  3,  3,  3,  2,  2,  2,  2,  3,  3,  3,  3,
      4,  4,  4,  4,  5,  5,  5,  5,  4,  4,  4,  4,  5,  5,  5,  5,
      6,  6,  6,  6,  7,  7,  7,  7,  6,  6,  6,  6,  7,  7,  7,  7,
      8,  8,  8,  8,  9,  9,  9,  9,  8,  8,  8,  8,  9,  9,  9,  9,
     10, 10, 10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11, 11, 11,
     12, 12, 12, 12, 13, 13, 13, 13, 12, 12, 12, 12, 13, 13, 13, 13,
     14, 14, 14, 14, 15, 15, 15, 15, 14, 14, 14, 14, 15, 15, 15, 15
};

const signed char hamm24err[64] = {
     0, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,
     0,  0,  0,  0,   0,  0,  0,  0,  -1, -1, -1, -1,  -1, -1, -1, -1,
};

/*
 *  [AleVT]
 *
 *  Mapping from parity checks made by table hamm24par to faulty bit
 *  in the decoded 18 bit word.
 */
const unsigned int hamm24cor[64] = {
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
    0x00000, 0x00000, 0x00000, 0x00001, 0x00000, 0x00002, 0x00004, 0x00008,
    0x00000, 0x00010, 0x00020, 0x00040, 0x00080, 0x00100, 0x00200, 0x00400,
    0x00000, 0x00800, 0x01000, 0x02000, 0x04000, 0x08000, 0x10000, 0x20000,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
};

/**
 * @internal
 * @param p Pointer to a byte triplet, bytes in transmission order,
 *   lsb first transmitted.
 *
 * This function decodes a Hamming 24/18 protected byte triplet
 * as specified in ETS 300 706 8.3.
 *
 * @return
 * Triplet data bits D18 [msb] ... D1 [lsb] or a negative
 * value if the triplet contained incorrectable errors.
 */
static signed int deh24(unsigned char *p)
{
	int e = hamm24par[0][p[0]]
		^ hamm24par[1][p[1]]
		^ hamm24par[2][p[2]];

	int x = hamm24val[p[0]]
		+ (p[1] & 127) * 16
		+ (p[2] & 127) * 2048;

	return (x ^ hamm24cor[e]) | hamm24err[e];
}

#else	 /* my (rm) slower but smaller solution without lookup tables */

/* calc parity */
int parity(int c)
{
	int n = 0;
	for (; c; c &= (c-1)) /* reset least significant set bit */
		n ^= 1;
	return n;
}

#if 0	/* just for testing */
/* encode hamming 24/18 */
unsigned int ham24(unsigned int val)
{
	val = ((val & 0x000001) << 2) |
		((val & 0x00000e) << 3) |
		((val & 0x0007f0) << 4) |
		((val & 0x03f800) << 5);
	val |= parity(val & 0x555554);
	val |= parity(val & 0x666664) << 1;
	val |= parity(val & 0x787870) << 3;
	val |= parity(val & 0x007f00) << 7;
	val |= parity(val & 0x7f0000) << 15;
	val |= parity(val) << 23;
	return val;
}
#endif

/* decode hamming 24/18, error=-1 */
signed int deh24(unsigned char *ph24)
{
	int h24 = *ph24 | (*(ph24+1)<<8) | (*(ph24+2)<<16);
	int a = parity(h24 & 0x555555);
	int f = parity(h24 & 0xaaaaaa) ^ a;
	a |= (parity(h24 & 0x666666) << 1) |
		(parity(h24 & 0x787878) << 2) |
		(parity(h24 & 0x007f80) << 3) |
		(parity(h24 & 0x7f8000) << 4);
	if (a != 0x1f)
	{
		if (f) /* 2 biterrors */
			return -1;
		else /* correct 1 biterror */
			h24 ^= (1 << ((a ^ 0x1f)-1));
	}
	return
		((h24 & 0x000004) >> 2) |
		((h24 & 0x000070) >> 3) |
		((h24 & 0x007f00) >> 4) |
		((h24 & 0x7f0000) >> 5);
}
#endif /* table or serial */
