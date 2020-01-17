/* tuxtxt_common.h
 * for license info see the other tuxtxt files
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#if TUXTXT_COMPRESS == 1
#include <zlib.h>
#endif

#include <hardware/dmx.h>
#include <system/set_threadname.h>

tuxtxt_cache_struct tuxtxt_cache;
static pthread_mutex_t tuxtxt_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tuxtxt_cache_biglock = PTHREAD_MUTEX_INITIALIZER;
int tuxtxt_get_zipsize(int p,int sp)
{
    tstCachedPage* pg = tuxtxt_cache.astCachetable[p][sp];
    if (!pg) return 0;
#if TUXTXT_COMPRESS == 1
	return pg->ziplen;
#elif TUXTXT_COMPRESS == 2
	pthread_mutex_lock(&tuxtxt_cache_lock);
	int zipsize = 0,i,j;
	for (i = 0; i < 23*5; i++)
		for (j = 0; j < 8; j++)
		zipsize += pg->bitmask[i]>>j & 0x01;

	zipsize+=23*5;//bitmask
	pthread_mutex_unlock(&tuxtxt_cache_lock);
	return zipsize;
#else
	return 23*40;
#endif
}
void tuxtxt_compress_page(int p, int sp, unsigned char* buffer)
{
	pthread_mutex_lock(&tuxtxt_cache_lock);
	tstCachedPage* pg = tuxtxt_cache.astCachetable[p][sp];
	if (!pg)
	{
		printf("tuxtxt: trying to compress a not allocated page!!\n");
		pthread_mutex_unlock(&tuxtxt_cache_lock);
		return;
	}

#if TUXTXT_COMPRESS == 1
	unsigned char pagecompressed[23*40];
	uLongf comprlen = 23*40;
	if (compress2(pagecompressed,&comprlen,buffer,23*40,Z_BEST_SPEED) == Z_OK)
	{
		if (pg->pData) free(pg->pData);//realloc(pg->pData,j); realloc scheint nicht richtig zu funktionieren?
		pg->pData = malloc(comprlen);
		pg->ziplen = 0;
		if (pg->pData)
		{
			pg->ziplen = comprlen;
			memmove(pg->pData,pagecompressed,comprlen);
		}
	}
#elif TUXTXT_COMPRESS == 2
	int i,j=0;
	unsigned char cbuf[23*40];
	memset(pg->bitmask,0,sizeof(pg->bitmask));
	for (i = 0; i < 23*40; i++)
	{
		if (i && buffer[i] == buffer[i-1])
		    continue;
		pg->bitmask[i>>3] |= 0x80>>(i&0x07);
		cbuf[j++]=buffer[i];
	}
	if (pg->pData)
		free(pg->pData);//realloc(pg->pData,j); realloc scheint nicht richtig zu funktionieren?
	pg->pData = (unsigned char*)malloc(j);
	if (pg->pData)
	{
		memmove(pg->pData,cbuf,j);
	}
	else
		memset(pg->bitmask,0,sizeof(pg->bitmask));
#else
	//if (pg->pData)
		memmove(pg->data,buffer,23*40);
#endif
	pthread_mutex_unlock(&tuxtxt_cache_lock);
}

void tuxtxt_decompress_page(int p, int sp, unsigned char* buffer)
{
	pthread_mutex_lock(&tuxtxt_cache_lock);
	tstCachedPage* pg = tuxtxt_cache.astCachetable[p][sp];

	memset(buffer,' ',23*40);
	if (!pg)
	{
		printf("tuxtxt: trying to decompress a not allocated page!!\n");
		pthread_mutex_unlock(&tuxtxt_cache_lock);
		return;
	}
#if TUXTXT_COMPRESS == 1
	if (pg->pData)
	{
		if (pg->ziplen)
		{
			uLongf comprlen = 23*40;
			uncompress(buffer,&comprlen,pg->pData,pg->ziplen);
		}

#elif TUXTXT_COMPRESS == 2
	if (pg->pData)
	{
		int i,j=0;
		char c=0x20;
		for (i = 0; i < 23*40; i++)
		{
		    if (pg->bitmask[i>>3] & 0x80>>(i&0x07))
				c = pg->pData[j++];
		    buffer[i] = c;
		}
#else
	{
		memmove(buffer,pg->data,23*40);
#endif
	}
	pthread_mutex_unlock(&tuxtxt_cache_lock);
}
void tuxtxt_next_dec(int *i) /* skip to next decimal */
{
	(*i)++;

	if ((*i & 0x0F) > 0x09)
		*i += 0x06;

	if ((*i & 0xF0) > 0x90)
		*i += 0x60;

	if (*i > 0x899)
		*i = 0x100;
}

void tuxtxt_prev_dec(int *i)           /* counting down */
{
	(*i)--;

	if ((*i & 0x0F) > 0x09)
		*i -= 0x06;

	if ((*i & 0xF0) > 0x90)
		*i -= 0x60;

	if (*i < 0x100)
		*i = 0x899;
}

int tuxtxt_is_dec(int i)
{
	return ((i & 0x00F) <= 9) && ((i & 0x0F0) <= 0x90);
}

int tuxtxt_next_hex(int i) /* return next existing non-decimal page number */
{
	int startpage = i;
	if (startpage < 0x100)
		startpage = 0x100;

	do
	{
		i++;
		if (i > 0x8FF)
			i = 0x100;
		if (i == startpage)
			break;
	}  while ((tuxtxt_cache.subpagetable[i] == 0xFF) || tuxtxt_is_dec(i));
	return i;
}
/*
 * TOP-Text
 * Info entnommen aus videotext-0.6.19991029,
 * Copyright (c) 1994-96 Martin Buck  <martin-2.buck@student.uni-ulm.de>
 */
void tuxtxt_decode_btt()
{
	/* basic top table */
	int i, current, b1, b2, b3, b4;
	unsigned char btt[23*40] = {0};

	if (tuxtxt_cache.subpagetable[0x1f0] == 0xff || 0 == tuxtxt_cache.astCachetable[0x1f0][tuxtxt_cache.subpagetable[0x1f0]]) /* not yet received */
		return;
	tuxtxt_decompress_page(0x1f0,tuxtxt_cache.subpagetable[0x1f0],btt);
	if (btt[799] == ' ') /* not completely received or error */
		return;

	current = 0x100;
	for (i = 0; i < 800; i++)
	{
		b1 = btt[i];
		if (b1 == ' ')
			b1 = 0;
		else
		{
			b1 = dehamming[b1];
			if (b1 == 0xFF) /* hamming error in btt */
			{
				btt[799] = ' '; /* mark btt as not received */
				return;
			}
		}
		tuxtxt_cache.basictop[current] = b1;
		tuxtxt_next_dec(&current);
	}
	/* page linking table */
	tuxtxt_cache.maxadippg = -1; /* rebuild table of adip pages */
	for (i = 0; i < 10; i++)
	{
		b1 = dehamming[btt[800 + 8*i +0]];

		if (b1 == 0xE)
			continue; /* unused */
		else if (b1 == 0xF)
			break; /* end */

		b4 = dehamming[btt[800 + 8*i +7]];

		if (b4 != 2) /* only adip, ignore multipage (1) */
			continue;

		b2 = dehamming[btt[800 + 8*i +1]];
		b3 = dehamming[btt[800 + 8*i +2]];

		if (b1 == 0xFF || b2 == 0xFF || b3 == 0xFF)
		{
			printf("TuxTxt <Biterror in btt/plt index %d>\n", i);
			btt[799] = ' '; /* mark btt as not received */
			return;
		}

		b1 = b1<<8 | b2<<4 | b3; /* page number */
		tuxtxt_cache.adippg[++tuxtxt_cache.maxadippg] = b1;
	}
#if TUXTXT_DEBUG
	printf("TuxTxt <BTT decoded>\n");
#endif
	tuxtxt_cache.bttok = 1;
}

void tuxtxt_decode_adip() /* additional information table */
{
	int i, p, j, b1, b2, b3, charfound;
	unsigned char padip[23*40];

	for (i = 0; i <= tuxtxt_cache.maxadippg; i++)
	{
		p = tuxtxt_cache.adippg[i];
		if (!p || tuxtxt_cache.subpagetable[p] == 0xff || 0 == tuxtxt_cache.astCachetable[p][tuxtxt_cache.subpagetable[p]]) /* not cached (avoid segfault) */
			continue;

		tuxtxt_decompress_page(p,tuxtxt_cache.subpagetable[p],padip);
		for (j = 0; j < 44; j++)
		{
			b1 = dehamming[padip[20*j+0]];
			if (b1 == 0xE)
				continue; /* unused */

			if (b1 == 0xF)
				break; /* end */

			b2 = dehamming[padip[20*j+1]];
			b3 = dehamming[padip[20*j+2]];

			if (b1 == 0xFF || b2 == 0xFF || b3 == 0xFF)
			{
				printf("TuxTxt <Biterror in ait %03x %d %02x %02x %02x %02x %02x %02x>\n", p, j,
						 padip[20*j+0],
						 padip[20*j+1],
						 padip[20*j+2],
						 b1, b2, b3
						 );
				return;
			}

			if (b1>8 || b2>9 || b3>9) /* ignore extries with invalid or hex page numbers */
			{
				continue;
			}

			b1 = b1<<8 | b2<<4 | b3; /* page number */
			charfound = 0; /* flag: no printable char found */

			for (b2 = 11; b2 >= 0; b2--)
			{
				b3 = deparity[padip[20*j + 8 + b2]];
				if (b3 < ' ')
					b3 = ' ';

				if (b3 == ' ' && !charfound)
					tuxtxt_cache.adip[b1][b2] = '\0';
				else
				{
					tuxtxt_cache.adip[b1][b2] = b3;
					charfound = 1;
				}
			}
		} /* next link j */
		tuxtxt_cache.adippg[i] = 0; /* completely decoded: clear entry */
#if TUXTXT_DEBUG
		printf("TuxTxt <ADIP %03x decoded>\n", p);
#endif
	} /* next adip page i */

	while ((tuxtxt_cache.maxadippg >= 0) && !tuxtxt_cache.adippg[tuxtxt_cache.maxadippg]) /* and shrink table */
		tuxtxt_cache.maxadippg--;
}
/******************************************************************************
 * GetSubPage                                                                 *
 ******************************************************************************/
int tuxtxt_GetSubPage(int page, int subpage, int offset)
{
	int loop;


	for (loop = subpage + offset; loop != subpage; loop += offset)
	{
		if (loop < 0)
			loop = 0x79;
		else if (loop > 0x79)
			loop = 0;
		if (loop == subpage)
			break;

		if (tuxtxt_cache.astCachetable[page][loop])
		{
#if TUXTXT_DEBUG
			printf("TuxTxt <NextSubPage: %.3X-%.2X>\n", page, subpage);
#endif
			return loop;
		}
	}

#if TUXTXT_DEBUG
	printf("TuxTxt <NextSubPage: no other SubPage>\n");
#endif
	return subpage;
}

/******************************************************************************
 * clear_cache                                                                *
 ******************************************************************************/

void tuxtxt_clear_cache(void)
{
	pthread_mutex_lock(&tuxtxt_cache_biglock);
	pthread_mutex_lock(&tuxtxt_cache_lock);
	int clear_page, clear_subpage, d26;
	tuxtxt_cache.maxadippg  = -1;
	tuxtxt_cache.bttok      = 0;
	tuxtxt_cache.cached_pages  = 0;
	tuxtxt_cache.page_receiving = -1;
	tuxtxt_cache.vtxtpid = -1;
	memset(&tuxtxt_cache.subpagetable, 0xFF, sizeof(tuxtxt_cache.subpagetable));
	memset(&tuxtxt_cache.basictop, 0, sizeof(tuxtxt_cache.basictop));
	memset(&tuxtxt_cache.adip, 0, sizeof(tuxtxt_cache.adip));
	memset(&tuxtxt_cache.flofpages, 0 , sizeof(tuxtxt_cache.flofpages));
	memset(&tuxtxt_cache.timestring, 0x20, 8);
	unsigned char magazine;
	for (magazine = 1; magazine < 9; magazine++)
	{
		tuxtxt_cache.current_page  [magazine] = -1;
		tuxtxt_cache.current_subpage [magazine] = -1;
	}

	for (clear_page = 0; clear_page < 0x900; clear_page++)
		for (clear_subpage = 0; clear_subpage < 0x80; clear_subpage++)
			if (tuxtxt_cache.astCachetable[clear_page][clear_subpage])
			{
				tstPageinfo *p = &(tuxtxt_cache.astCachetable[clear_page][clear_subpage]->pageinfo);
				if (p->p24)
					free(p->p24);
				if (p->ext)
				{
					if (p->ext->p27)
						free(p->ext->p27);
					for (d26=0; d26 < 16; d26++)
						if (p->ext->p26[d26])
							free(p->ext->p26[d26]);
					free(p->ext);
				}
#if TUXTXT_COMPRESS >0
				if (tuxtxt_cache.astCachetable[clear_page][clear_subpage]->pData)
					free(tuxtxt_cache.astCachetable[clear_page][clear_subpage]->pData);
#endif
				free(tuxtxt_cache.astCachetable[clear_page][clear_subpage]);
				tuxtxt_cache.astCachetable[clear_page][clear_subpage] = 0;
			}
	for (clear_page = 0; clear_page < 9; clear_page++)
	{
		if (tuxtxt_cache.astP29[clear_page])
		{
		    if (tuxtxt_cache.astP29[clear_page]->p27)
			free(tuxtxt_cache.astP29[clear_page]->p27);
		    for (d26=0; d26 < 16; d26++)
			if (tuxtxt_cache.astP29[clear_page]->p26[d26])
			    free(tuxtxt_cache.astP29[clear_page]->p26[d26]);
		    free(tuxtxt_cache.astP29[clear_page]);
		    tuxtxt_cache.astP29[clear_page] = 0;
		}
		tuxtxt_cache.current_page  [clear_page] = -1;
		tuxtxt_cache.current_subpage [clear_page] = -1;
	}
	memset(&tuxtxt_cache.astCachetable, 0, sizeof(tuxtxt_cache.astCachetable));
	memset(&tuxtxt_cache.astP29, 0, sizeof(tuxtxt_cache.astP29));
#if TUXTXT_DEBUG
	printf("TuxTxt cache cleared\n");
#endif
	pthread_mutex_unlock(&tuxtxt_cache_lock);
	pthread_mutex_unlock(&tuxtxt_cache_biglock);
}
/******************************************************************************
 * init_demuxer                                                               *
 ******************************************************************************/
static cDemux * dmx = NULL;
int tuxtxt_init_demuxer(int source = 0);

int tuxtxt_init_demuxer(int source)
{

	if(dmx == NULL) {
		dmx = new cDemux(source);
		dmx->Open(DMX_PES_CHANNEL, NULL, 2* 3008 * 62 /*64*1024*/);
		printf("TuxTxt: source demux %d\n", source);
	}
#if TUXTXT_DEBUG
	printf("TuxTxt: initialized\n");
#endif
	/* init successfull */

	return 1;
}
/******************************************************************************
 * CacheThread support functions                                              *
 ******************************************************************************/

void tuxtxt_decode_p2829(unsigned char *vtxt_row, tstExtData **ptExtData)
{
	int bitsleft, colorindex;
	unsigned char *p;
	int t1 = deh24(&vtxt_row[7-4]);
	int t2 = deh24(&vtxt_row[10-4]);

	if (t1 < 0 || t2 < 0)
	{
#if TUXTXT_DEBUG
		printf("TuxTxt <Biterror in p28>\n");
#endif
		return;
	}

	if (!(*ptExtData))
		(*ptExtData) = (tstExtData*)calloc(1, sizeof(tstExtData));
	if (!(*ptExtData))
		return;

	(*ptExtData)->p28Received = 1;
	(*ptExtData)->DefaultCharset = (t1>>7) & 0x7f;
	(*ptExtData)->SecondCharset = ((t1>>14) & 0x0f) | ((t2<<4) & 0x70);
	(*ptExtData)->LSP = !!(t2 & 0x08);
	(*ptExtData)->RSP = !!(t2 & 0x10);
	(*ptExtData)->SPL25 = !!(t2 & 0x20);
	(*ptExtData)->LSPColumns = (t2>>6) & 0x0f;

	bitsleft = 8; /* # of bits not evaluated in val */
	t2 >>= 10; /* current data */
	p = &vtxt_row[13-4];	/* pointer to next data triplet */
	for (colorindex = 0; colorindex < 16; colorindex++)
	{
		if (bitsleft < 12)
		{
			t2 |= deh24(p) << bitsleft;
			if (t2 < 0)	/* hamming error */
				break;
			p += 3;
			bitsleft += 18;
		}
		(*ptExtData)->bgr[colorindex] = t2 & 0x0fff;
		bitsleft -= 12;
		t2 >>= 12;
	}
	if (t2 < 0 || bitsleft != 14)
	{
#if TUXTXT_DEBUG
		printf("TuxTxt <Biterror in p28/29 t2=%d b=%d>\n", t2, bitsleft);
#endif
		(*ptExtData)->p28Received = 0;
		return;
	}
	(*ptExtData)->DefScreenColor = t2 & 0x1f;
	t2 >>= 5;
	(*ptExtData)->DefRowColor = t2 & 0x1f;
	(*ptExtData)->BlackBgSubst = !!(t2 & 0x20);
	t2 >>= 6;
	(*ptExtData)->ColorTableRemapping = t2 & 0x07;
}

void tuxtxt_erase_page(int magazine)
{
	tstCachedPage* pg = tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]];
	if (pg)
	{
		memset(&(pg->pageinfo), 0, sizeof(tstPageinfo));	/* struct pageinfo */
		memset(pg->p0, ' ', 24);
#if TUXTXT_COMPRESS == 1
		if (pg->pData) {
			free(pg->pData);
			pg->pData = NULL;
		}
#elif TUXTXT_COMPRESS == 2
		memset(pg->bitmask, 0, 23*5);
#else
		memset(pg->data, ' ', 23*40);
#endif
	}
}

void tuxtxt_clear_p26(tstExtData* extData)
{
	pthread_mutex_lock(&tuxtxt_cache_lock);

	for(int i = 0; i < 16; ++i)
	{
		if(0 != extData->p26[i])
		{
			memset(extData->p26[i], 0x00, 13 * 3);
		}
	}

	pthread_mutex_unlock(&tuxtxt_cache_lock);
}

void tuxtxt_allocate_cache(int magazine)
{
	// Lock here as we have a possible race here with
	// tuxtxt_clear_cache(). We should not be allocating and
	// freeing at the same time.
	// *** this is probably worked around by tuxtxt_cacehe_biglock now *** --seife
	pthread_mutex_lock(&tuxtxt_cache_lock);

	/* check cachetable and allocate memory if needed */
	if (tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]] == 0)
	{
		tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]] = (tstCachedPage*) malloc(sizeof(tstCachedPage));
		if (tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]] )
		{
#if TUXTXT_COMPRESS >0
			tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->pData = 0;
#endif
			tuxtxt_erase_page(magazine);
			tuxtxt_cache.cached_pages++;
		}
		else // Be a little verbose in case a crash is going to happen.
		{
			printf("tuxtxt: memory allocation failed!!! expect a crash\n");
		}
	}
	pthread_mutex_unlock(&tuxtxt_cache_lock);
}

/******************************************************************************
 * CacheThread                                                                *
 ******************************************************************************/
static int stop_cache = 0;
void *tuxtxt_CacheThread(void * /*arg*/)
{
	const unsigned char rev_lut[32] = {
		0x00,0x08,0x04,0x0c, /*  upper nibble */
		0x02,0x0a,0x06,0x0e,
		0x01,0x09,0x05,0x0d,
		0x03,0x0b,0x07,0x0f,
		0x00,0x80,0x40,0xc0, /*  lower nibble */
		0x20,0xa0,0x60,0xe0,
		0x10,0x90,0x50,0xd0,
		0x30,0xb0,0x70,0xf0 };
	unsigned char pes_packet[184*20];
	unsigned char vtxt_row[42];
	int line, byte/*, bit*/;
	int b1, b2, b3, b4;
	int packet_number;
	int doupdate=0;
	unsigned char magazine = 0xff;
	unsigned char pagedata[9][23*40];
	tstPageinfo *pageinfo_thread;

	set_threadname("tuxtxt:cache");
	int err = nice(3);
	printf("TuxTxt running thread...(%04x)%s\n",tuxtxt_cache.vtxtpid,err!=0?" nice error":"");
	tuxtxt_cache.receiving = 1;
	while (!stop_cache)
	{
		/* check stopsignal */
		pthread_testcancel();

		if (!tuxtxt_cache.receiving)
			continue;

		/* read packet */
		ssize_t readcnt = 0;

		readcnt = dmx->Read(pes_packet, sizeof (pes_packet), 1000);
		//if (readcnt != sizeof(pes_packet))
		if ((readcnt <= 0) || (readcnt % 184))
		{
#if TUXTXT_DEBUG
			if(readcnt > 0)
				printf ("TuxTxt: readerror: %d\n", readcnt);
#endif
			continue;
		}

		/* this "big hammer lock" is a hack: it avoids a crash if
		 * tuxtxt_clear_cache() is called while the cache thread is in the
		 * middle of the following loop, leading to tuxtxt_cache.current_page[]
		 * etc. being set to -1 and tuxtxt_cache.astCachetable[] etc. being set
		 * to NULL
		 * it probably also avoids the possible race in tuxtxt_allocate_cache() */
		pthread_mutex_lock(&tuxtxt_cache_biglock);
		/* analyze it */
		for (line = 0; line < readcnt/0x2e /*4*/; line++)
		{
			unsigned char *vtx_rowbyte = &pes_packet[line*0x2e];
			if ((vtx_rowbyte[1] == 0x2C) && (vtx_rowbyte[0] == 0x02 || vtx_rowbyte[0] == 0x03))
			{
				/* clear rowbuffer */
				/* convert row from lsb to msb (begin with magazin number) */
				for (byte = 4; byte < 46; byte++)
				{
					unsigned char upper,lower;
					upper = (vtx_rowbyte[byte] >> 4) & 0xf;
					lower = vtx_rowbyte[byte] & 0xf;
					vtxt_row[byte-4] = (rev_lut[upper]) | (rev_lut[lower+16]);
				}

				/* get packet number */
				b1 = dehamming[vtxt_row[0]];
				b2 = dehamming[vtxt_row[1]];

				if (b1 == 0xFF || b2 == 0xFF)
				{
#if TUXTXT_DEBUG
					printf("TuxTxt <Biterror in Packet>\n");
#endif
					continue;
				}

				b1 &= 8;

				packet_number = b1>>3 | b2<<1;

				/* get magazine number */
				magazine = dehamming[vtxt_row[0]] & 7;
				if (!magazine) magazine = 8;

				if (packet_number == 0 && tuxtxt_cache.current_page[magazine] != -1 && tuxtxt_cache.current_subpage[magazine] != -1)
					tuxtxt_compress_page(tuxtxt_cache.current_page[magazine],tuxtxt_cache.current_subpage[magazine],pagedata[magazine]);

				//printf("********************** receiving packet %d page %03x subpage %02x\n",packet_number, tuxtxt_cache.current_page[magazine],tuxtxt_cache.current_subpage[magazine]);//FIXME

				/* analyze row */
				if (packet_number == 0)
				{
					/* get pagenumber */
					b2 = dehamming[vtxt_row[3]];
					b3 = dehamming[vtxt_row[2]];

					if (b2 == 0xFF || b3 == 0xFF)
					{
						tuxtxt_cache.current_page[magazine] = tuxtxt_cache.page_receiving = -1;
#if TUXTXT_DEBUG
						printf("TuxTxt <Biterror in Page>\n");
#endif
						continue;
					}

					tuxtxt_cache.current_page[magazine] = tuxtxt_cache.page_receiving = magazine<<8 | b2<<4 | b3;

					if (b2 == 0x0f && b3 == 0x0f)
					{
						tuxtxt_cache.current_subpage[magazine] = -1; /* ?ff: ignore data transmissions */
						continue;
					}

					/* get subpagenumber */
					b1 = dehamming[vtxt_row[7]];
					b2 = dehamming[vtxt_row[6]];
					b3 = dehamming[vtxt_row[5]];
					b4 = dehamming[vtxt_row[4]];

					if (b1 == 0xFF || b2 == 0xFF || b3 == 0xFF || b4 == 0xFF)
					{
#if TUXTXT_DEBUG
						printf("TuxTxt <Biterror in SubPage>\n");
#endif
						tuxtxt_cache.current_subpage[magazine] = -1;
						continue;
					}
#if 0	/* ? */
					b1 &= 3;
#endif
					b3 &= 7;

					if (tuxtxt_is_dec(tuxtxt_cache.page_receiving)) /* ignore other subpage bits for hex pages */
					{
#if 0	/* ? */
						if (b1 != 0 || b2 != 0)
						{
#if TUXTXT_DEBUG
							printf("TuxTxt <invalid subpage data p%03x %02x %02x %02x %02x>\n", tuxtxt_cache.page_receiving, b1, b2, b3, b4);
#endif
							tuxtxt_cache.current_subpage[magazine] = -1;
							continue;
						}
						else
#endif
							tuxtxt_cache.current_subpage[magazine] = b3<<4 | b4;
					}
					else
						tuxtxt_cache.current_subpage[magazine] = b4; /* max 16 subpages for hex pages */

					/* store current subpage for this page */
					tuxtxt_cache.subpagetable[tuxtxt_cache.current_page[magazine]] = tuxtxt_cache.current_subpage[magazine];

					tuxtxt_allocate_cache(magazine);
					tuxtxt_decompress_page(tuxtxt_cache.current_page[magazine],tuxtxt_cache.current_subpage[magazine],pagedata[magazine]);
					pageinfo_thread = &(tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->pageinfo);

					if ((tuxtxt_cache.page_receiving & 0xff) == 0xfe) /* ?fe: magazine organization table (MOT) */
						pageinfo_thread->function = FUNC_MOT;

					/* check controlbits */
					if (dehamming[vtxt_row[5]] & 8)   /* C4 -> erase page */
					{
#if TUXTXT_COMPRESS == 1
						tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->ziplen = 0;
#elif TUXTXT_COMPRESS == 2
						memset(tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->bitmask, 0, 23*5);
#else
						memset(tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->data, ' ', 23*40);
#endif
						memset(pagedata[magazine],' ', 23*40);
					}
					if (dehamming[vtxt_row[9]] & 8)   /* C8 -> update page */
						doupdate = tuxtxt_cache.page_receiving;

					pageinfo_thread->boxed = !!(dehamming[vtxt_row[7]] & 0x0c);

					/* get country control bits */
					b1 = dehamming[vtxt_row[9]];
					if (b1 == 0xFF)
					{
#if TUXTXT_DEBUG
						printf("TuxTxt <Biterror in CountryFlags>\n");
#endif
					}
					else
					{
						//pageinfo_thread->nationalvalid = 1;// FIXME without full eval some is broken
						pageinfo_thread->national = rev_lut[b1] & 0x07;
//printf("TuxTxt 0: b1=%x\n", rev_lut[b1]);
					}

					/* check parity, copy line 0 to cache (start and end 8 bytes are not needed and used otherwise) */
					unsigned char *p = tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->p0;
					for (byte = 10; byte < 42-8; byte++)
						*p++ = deparity[vtxt_row[byte]];

					if (!tuxtxt_is_dec(tuxtxt_cache.page_receiving))
						continue; /* valid hex page number: just copy headline, ignore timestring */

					/* copy timestring */
					p = tuxtxt_cache.timestring;
					for (; byte < 42; byte++)
						*p++ = deparity[vtxt_row[byte]];
				} /* (packet_number == 0) */
				else if (packet_number == 29 && dehamming[vtxt_row[2]]== 0) /* packet 29/0 replaces 28/0 for a whole magazine */
				{
					tuxtxt_decode_p2829(vtxt_row, &(tuxtxt_cache.astP29[magazine]));
				}
				else if (tuxtxt_cache.current_page[magazine] != -1 && tuxtxt_cache.current_subpage[magazine] != -1)
					/* packet>0, 0 has been correctly received, buffer allocated */
				{
					pageinfo_thread = &(tuxtxt_cache.astCachetable[tuxtxt_cache.current_page[magazine]][tuxtxt_cache.current_subpage[magazine]]->pageinfo);
					/* pointer to current info struct */

					if (packet_number <= 25)
					{
						unsigned char *p = NULL;
						if (packet_number < 24)
							p = pagedata[magazine] + 40*(packet_number-1);
						else
						{
							if (!(pageinfo_thread->p24))
								pageinfo_thread->p24 = (unsigned char*) calloc(2, 40);
							if (pageinfo_thread->p24)
								p = pageinfo_thread->p24 + (packet_number - 24) * 40;
						}
						if (p)
						{
							if (tuxtxt_is_dec(tuxtxt_cache.current_page[magazine]))
								for (byte = 2; byte < 42; byte++)
									*p++ = deparity[vtxt_row[byte]]; /* check/remove parity bit */
							else if ((tuxtxt_cache.current_page[magazine] & 0xff) == 0xfe)
								for (byte = 2; byte < 42; byte++)
									*p++ = dehamming[vtxt_row[byte]]; /* decode hamming 8/4 */
							else /* other hex page: no parity check, just copy */
								memmove(p, &vtxt_row[2], 40);
						}
					}
					else if (packet_number == 27)
					{
						int descode = dehamming[vtxt_row[2]]; /* designation code (0..15) */

						if (descode == 0xff)
						{
#if TUXTXT_DEBUG
							printf("TuxTxt <Biterror in p27>\n");
#endif
							continue;
						}
						if (descode == 0) // reading FLOF-Pagelinks
						{
							b1 = dehamming[vtxt_row[0]];
							if (b1 != 0xff)
							{
								b1 &= 7;

								for (byte = 0; byte < FLOFSIZE; byte++)
								{
									b2 = dehamming[vtxt_row[4+byte*6]];
									b3 = dehamming[vtxt_row[3+byte*6]];

									if (b2 != 0xff && b3 != 0xff)
									{
										b4 = ((b1 ^ (dehamming[vtxt_row[8+byte*6]]>>1)) & 6) |
											((b1 ^ (dehamming[vtxt_row[6+byte*6]]>>3)) & 1);
										if (b4 == 0)
											b4 = 8;
										if (b2 <= 9 && b3 <= 9)
											tuxtxt_cache.flofpages[tuxtxt_cache.current_page[magazine] ][byte] = b4<<8 | b2<<4 | b3;
									}
								}

								/* copy last 2 links to adip for TOP-Index */
								if (pageinfo_thread->p24) /* packet 24 received */
								{
									int a, a1, e=39, l=3;
									char *p = (char*) pageinfo_thread->p24;
									do
									{
										for (;
											  l >= 2 && 0 == tuxtxt_cache.flofpages[tuxtxt_cache.current_page[magazine]][l];
											  l--)
											; /* find used linkindex */
										for (;
											  e >= 1 && !isalnum(p[e]);
											  e--)
											; /* find end */
										for (a = a1 = e - 1;
											  a >= 0 && p[a] >= ' ';
											  a--) /* find start */
											if (p[a] > ' ')
											a1 = a; /* first non-space */
										if (a >= 0 && l >= 2)
										{
											strncpy((char *) tuxtxt_cache.adip[tuxtxt_cache.flofpages[tuxtxt_cache.current_page[magazine]][l]],
													  &p[a1],
													  12);
											if (e-a1 < 11)
												tuxtxt_cache.adip[tuxtxt_cache.flofpages[tuxtxt_cache.current_page[magazine]][l]][e-a1+1] = '\0';
#if 0 //TUXTXT_DEBUG
											printf(" %03x/%02x %d %d %d %d %03x %s\n",
													 tuxtxt_cache.current_page[magazine], tuxtxt_cache.current_subpage[magazine],
													 l, a, a1, e,
													 tuxtxt_cache.flofpages[tuxtxt_cache.current_page[magazine]][l],
													 tuxtxt_cache.adip[tuxtxt_cache.flofpages[tuxtxt_cache.current_page[magazine]][l]]
													 );
#endif
										}
										e = a - 1;
										l--;
									} while (l >= 2);
								}
							}
						}
						else if (descode == 4)	/* level 2.5 links (ignore level 3.5 links of /4 and /5) */
						{
							int i;
							tstp27 *p;

							if (!pageinfo_thread->ext)
								pageinfo_thread->ext = (tstExtData*) calloc(1, sizeof(tstExtData));
							if (!pageinfo_thread->ext)
								continue;
							if (!(pageinfo_thread->ext->p27))
								pageinfo_thread->ext->p27 = (tstp27*) calloc(4, sizeof(tstp27));
							if (!(pageinfo_thread->ext->p27))
								continue;
							p = pageinfo_thread->ext->p27;
							for (i = 0; i < 4; i++)
							{
								int d1 = deh24(&vtxt_row[6*i + 3]);
								int d2 = deh24(&vtxt_row[6*i + 6]);
								if (d1 < 0 || d2 < 0)
								{
#if TUXTXT_DEBUG
									printf("TuxTxt <Biterror in p27/4-5>\n");
#endif
									continue;
								}
								p->local = i & 0x01;
								p->drcs = !!(i & 0x02);
								p->l25 = !!(d1 & 0x04);
								p->l35 = !!(d1 & 0x08);
								p->page =
									(((d1 & 0x000003c0) >> 6) |
									 ((d1 & 0x0003c000) >> (14-4)) |
									 ((d1 & 0x00003800) >> (11-8))) ^
									(dehamming[vtxt_row[0]] << 8);
								if (p->page < 0x100)
									p->page += 0x800;
								p->subpage = d2 >> 2;
								if ((p->page & 0xff) == 0xff)
									p->page = 0;
								else if (p->page > 0x899)
								{
									// workaround for crash on RTL
									printf("[TuxTxt] page > 0x899 ... ignore!!!!!!\n");
									continue;
								}
								else if (tuxtxt_cache.astCachetable[p->page][0])	/* link valid && linked page cached */
								{
									tstPageinfo *pageinfo_link = &(tuxtxt_cache.astCachetable[p->page][0]->pageinfo);
									if (p->local)
										pageinfo_link->function = p->drcs ? FUNC_DRCS : FUNC_POP;
									else
										pageinfo_link->function = p->drcs ? FUNC_GDRCS : FUNC_GPOP;
								}
								p++; /*  */
							}
						}
					}

					else if (packet_number == 26)
					{
						int descode = dehamming[vtxt_row[2]]; /* designation code (0..15) */

						if (descode == 0xff)
						{
#if TUXTXT_DEBUG
							printf("TuxTxt <Biterror in p26>\n");
#endif
							continue;
						}
						if (!pageinfo_thread->ext)
							pageinfo_thread->ext = (tstExtData*) calloc(1, sizeof(tstExtData));
						if (!pageinfo_thread->ext)
							continue;
						if (!(pageinfo_thread->ext->p26[descode]))
							pageinfo_thread->ext->p26[descode] = (unsigned char*) malloc(13 * 3);
						if (pageinfo_thread->ext->p26[descode])
							memmove(pageinfo_thread->ext->p26[descode], &vtxt_row[3], 13 * 3);
#if 0//TUXTXT_DEBUG
						int i, t, m;

						printf("P%03x/%02x %02d/%x",
								 tuxtxt_cache.current_page[magazine], tuxtxt_cache.current_subpage[magazine],
								 packet_number, dehamming[vtxt_row[2]]);
						for (i=7-4; i <= 45-4; i+=3) /* dump all triplets */
						{
							t = deh24(&vtxt_row[i]); /* mode/adr/data */
							m = (t>>6) & 0x1f;
							printf(" M%02xA%02xD%03x", m, t & 0x3f, (t>>11) & 0x7f);
							if (m == 0x1f)	/* terminator */
								break;
						}
						putchar('\n');
#endif
					}
					else if (packet_number == 28)
					{
						int descode = dehamming[vtxt_row[2]]; /* designation code (0..15) */

						if (descode == 0xff)
						{
#if TUXTXT_DEBUG
							printf("TuxTxt <Biterror in p28>\n");
#endif
							continue;
						}
						if (descode != 2)
						{
							int t1 = deh24(&vtxt_row[7-4]);
							pageinfo_thread->function = t1 & 0x0f;
							if (!pageinfo_thread->nationalvalid)
							{
//printf("TuxTxt 28: t1=%x\n", t1>>4);
								// pageinfo_thread->nationalvalid = 1; // FIXME without full eval some is broken
								pageinfo_thread->national = (t1>>4) & 0x07;
							}
						}

						switch (descode) /* designation code */
						{
						case 0: /* basic level 1 page */
						{
							tuxtxt_decode_p2829(vtxt_row, &(pageinfo_thread->ext));
							break;
						}
						case 1: /* G0/G1 designation for older decoders, level 3.5: DCLUT4/16, colors for multicolored bitmaps */
						{
							break; /* ignore */
						}
						case 2: /* page key */
						{
							break; /* ignore */
						}
						case 3: /* types of PTUs in DRCS */
						{
							break; /* TODO */
						}
						case 4: /* CLUTs 0/1, only level 3.5 */
						{
							break; /* ignore */
						}
						default:
						{
							break; /* invalid, ignore */
						}
						} /* switch designation code */
					}
					else if (packet_number == 30)
					{
#if 0//TUXTXT_DEBUG
						int i;

						printf("p%03x/%02x %02d/%x ",
								 tuxtxt_cache.current_page[magazine], tuxtxt_cache.current_subpage[magazine],
								 packet_number, dehamming[vtxt_row[2]]);
						for (i=26-4; i <= 45-4; i++) /* station ID */
							putchar(deparity[vtxt_row[i]]);
						putchar('\n');
#endif
					}
				}
				/* set update flag */
				if (tuxtxt_cache.current_page[magazine] == tuxtxt_cache.page && tuxtxt_cache.current_subpage[magazine] != -1)
				{
 				    tuxtxt_compress_page(tuxtxt_cache.current_page[magazine],tuxtxt_cache.current_subpage[magazine],pagedata[magazine]);
					tuxtxt_cache.pageupdate = 1+(doupdate == tuxtxt_cache.page ? 1: 0);
					doupdate=0;
					if (!tuxtxt_cache.zap_subpage_manual)
						tuxtxt_cache.subpage = tuxtxt_cache.current_subpage[magazine];
				}
			}
#if 0
			else
				printf("line %d row %X %X, continue\n", line, vtx_rowbyte[0], vtx_rowbyte[1]);
#endif
		}
		pthread_mutex_unlock(&tuxtxt_cache_biglock);
	}

	pthread_exit(NULL);
}
/******************************************************************************
 * start_thread                                                               *
 ******************************************************************************/
int tuxtxt_start_thread(int source = 0);
int tuxtxt_start_thread(int source)
{
	if (tuxtxt_cache.vtxtpid == -1)
		return 0;

	tuxtxt_cache.thread_starting = 1;
	tuxtxt_init_demuxer(source);

	dmx->pesFilter(tuxtxt_cache.vtxtpid);
	dmx->Start();
	stop_cache = 0;

	/* create decode-thread */
	if (pthread_create(&tuxtxt_cache.thread_id, NULL, tuxtxt_CacheThread, NULL) != 0)
	{
		perror("TuxTxt <pthread_create>");
		tuxtxt_cache.thread_starting = 0;
		return 0;
	}
#if 1//TUXTXT_DEBUG
	printf("TuxTxt service started %x\n", tuxtxt_cache.vtxtpid);
#endif
	tuxtxt_cache.receiving = 1;
	tuxtxt_cache.thread_starting = 0;
	return 1;
}
/******************************************************************************
 * stop_thread                                                                *
 ******************************************************************************/

int tuxtxt_stop_thread()
{
	/* stop decode-thread */
	if (tuxtxt_cache.thread_id != 0)
	{
#if 0
		if (pthread_cancel(tuxtxt_cache.thread_id) != 0)
		{
			perror("TuxTxt <pthread_cancel>");
			return 0;
		}
#endif
		stop_cache = 1;
		if (pthread_join(tuxtxt_cache.thread_id, &tuxtxt_cache.thread_result) != 0)
		{
			perror("TuxTxt <pthread_join>");
			return 0;
		}
		tuxtxt_cache.thread_id = 0;
	}
	if(dmx) {
		dmx->Stop();
		delete dmx;
		dmx = NULL;
	}
#if 1//TUXTXT_DEBUG
	printf("TuxTxt stopped service %x\n", tuxtxt_cache.vtxtpid);
#endif
	return 1;
}
