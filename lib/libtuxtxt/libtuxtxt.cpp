/******************************************************************************
 *                      <<< TuxTxt - Teletext Plugin >>>                      *
 *                                                                            *
 *             (c) Thomas "LazyT" Loewe 2002-2003 (LazyT@gmx.net)             *
 *                                                                            *
 *    TOP-Text Support 2004 by Roland Meier <RolandMeier@Siemens.com>         *
 *    Info entnommen aus videotext-0.6.19991029,                              *
 *    Copyright (c) 1994-96 Martin Buck  <martin-2.buck@student.uni-ulm.de>   *
 *                                                                            *
 ******************************************************************************/

#define TUXTXT_DEBUG 0
#include <pthread.h>

#include "tuxtxt_def.h"
#include "tuxtxt_common.h"
int tuxtxt_stop();
/******************************************************************************
 * Initialize                                                                 *
 ******************************************************************************/

static int tuxtxt_initialized=0;

int tuxtxt_init()
{
	if ( tuxtxt_initialized )
		return 0;

	tuxtxt_initialized=1;

	/* init data */
	tuxtxt_stop();
	tuxtxt_clear_cache();
	tuxtxt_cache.thread_starting = 0;
	tuxtxt_cache.vtxtpid = -1;
	tuxtxt_cache.thread_id = 0;
	/* not sure if this is correct here... */
	tuxtxt_cache.page = 0x100;
	return 1;//tuxtxt_init_demuxer();
}

/******************************************************************************
 * Interface to caller                                                        *
 ******************************************************************************/

int tuxtxt_stop()
{
#if TUXTXT_DEBUG
	fprintf(stderr, "[tuxtxt][stop] ENTER: receiving=%d, thread_id=%lu, vpid=%d\n",
		tuxtxt_cache.receiving, (unsigned long)tuxtxt_cache.thread_id, tuxtxt_cache.vtxtpid);
#endif
	if (!tuxtxt_cache.receiving) return 1;
	tuxtxt_cache.receiving = 0;
	int rc = tuxtxt_stop_thread();
#if TUXTXT_DEBUG
	fprintf(stderr, "[tuxtxt][stop] EXIT rc=%d\n", rc);
#endif
	return rc;
}

void tuxtxt_start(int tpid, int source)
{
#if TUXTXT_DEBUG
	fprintf(stderr, "[tuxtxt][start] ENTER: curr_vpid=%d -> new_vpid=%d, source=%d, receiving=%d, thread_starting=%d\n",
		tuxtxt_cache.vtxtpid, tpid, source, tuxtxt_cache.receiving, tuxtxt_cache.thread_starting);
#endif
	if (tpid == -1)
	{
		printf("tuxtxt: invalid PID!\n");
		return;
	}

	if (tuxtxt_cache.vtxtpid != tpid)
	{
		tuxtxt_stop();
		tuxtxt_clear_cache();
		tuxtxt_cache.page = 0x100;
		tuxtxt_cache.vtxtpid = tpid;
		tuxtxt_start_thread(source);
	}
	else if (!tuxtxt_cache.thread_starting && !tuxtxt_cache.receiving)
	{
		tuxtxt_start_thread(source);
	}
#if TUXTXT_DEBUG
	fprintf(stderr, "[tuxtxt][start] EXIT: receiving=%d, thread_id=%lu, vpid=%d\n",
		tuxtxt_cache.receiving, (unsigned long)tuxtxt_cache.thread_id, tuxtxt_cache.vtxtpid);
#endif
}

void tuxtxt_close()
{
#if TUXTXT_DEBUG
	printf ("libtuxtxt: cleaning up\n");
	fprintf(stderr, "[tuxtxt][close] ENTER\n");
#endif
	tuxtxt_stop();
	tuxtxt_clear_cache();
	tuxtxt_initialized=0;
#if TUXTXT_DEBUG
	fprintf(stderr, "[tuxtxt][close] EXIT\n");
#endif
}
