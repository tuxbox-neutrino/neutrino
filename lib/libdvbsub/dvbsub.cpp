#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

#include <cerrno>

#include <dmx_cs.h>

#include "Debug.hpp"
#include "PacketQueue.hpp"
#include "semaphore.h"
#include "reader_thread.hpp"
#include "dvbsub_thread.hpp"
#include "helpers.hpp"
#include "dvbsubtitle.h"

#define Log2File	printf
#define RECVBUFFER_STEPSIZE 1024

enum {NOERROR, NETWORK, DENIED, NOSERVICE, BOXTYPE, THREAD, ABOUT};
enum {GET_VOLUME, SET_VOLUME, SET_MUTE, SET_CHANNEL};

Debug sub_debug;
static PacketQueue packet_queue;
//sem_t event_semaphore;

static pthread_t threadReader;
static pthread_t threadDvbsub;

static pthread_cond_t readerCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t readerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t packetCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t packetMutex = PTHREAD_MUTEX_INITIALIZER;

static int reader_running;
static int dvbsub_running;
static int dvbsub_paused = true;
static int dvbsub_pid;
static int dvbsub_stopped;

cDvbSubtitleConverter *dvbSubtitleConverter;

int dvbsub_init() {
	int trc;

	sub_debug.set_level(42);

	reader_running = true;
	// reader-Thread starten
	trc = pthread_create(&threadReader, 0, reader_thread, (void *) NULL);
	if (trc) {
		fprintf(stderr, "[dvb-sub] failed to create reader-thread (rc=%d)\n", trc);
		reader_running = false;
		return -1;
	}

	dvbsub_running = true;
	// subtitle decoder-Thread starten
	trc = pthread_create(&threadDvbsub, 0, dvbsub_thread, NULL);
	if (trc) {
		fprintf(stderr, "[dvb-sub] failed to create dvbsub-thread (rc=%d)\n", trc);
		dvbsub_running = false;
		return -1;
	}

	return(0);
}

int dvbsub_pause()
{
	if(reader_running) {
		if(dvbSubtitleConverter) {
			dvbSubtitleConverter->Pause(true);
		}
		dvbsub_paused = true;
		printf("[dvb-sub] paused\n");
	}

	return 0;
}

int dvbsub_start(int pid)
{
	if(!dvbsub_paused && (pid == 0)) {
		return 0;
	}


	if(pid) {
		if(pid != dvbsub_pid)
			dvbsub_pause();
		dvbsub_pid = pid;
	}

	while(!dvbsub_stopped)
		usleep(10);

	if(dvbsub_pid > 0) {
		dvbsub_paused = false;
		dvbSubtitleConverter->Pause(false);
		pthread_mutex_lock(&readerMutex);
		pthread_cond_broadcast(&readerCond);
		pthread_mutex_unlock(&readerMutex);
		printf("[dvb-sub] started with pid 0x%x\n", pid);
	}

	return 0;
}

int dvbsub_stop()
{
	dvbsub_pid = 0;
	if(reader_running) {
		dvbsub_pause();
	}

	return 0;
}

int dvbsub_getpid()
{
	return dvbsub_pid;
}

int dvbsub_close()
{
	if(threadReader) {
		dvbsub_pause();
		reader_running = false;
		pthread_mutex_lock(&readerMutex);
		pthread_cond_broadcast(&readerCond);
		pthread_mutex_unlock(&readerMutex);
		pthread_join(threadReader, NULL);
		threadReader = NULL;
	}
	if(threadDvbsub) {
		dvbsub_running = false;
		pthread_join(threadDvbsub, NULL);
		threadDvbsub = NULL;
	}
	printf("[dvb-sub] stopped\n");

	return 0;
}

static cDemux * dmx;

void* reader_thread(void *arg)
{
	uint8_t tmp[16];  /* actually 6 should be enough */
	int count;
	int len;
	uint16_t packlen;
	uint8_t* buf;

        dmx = new cDemux(0);
        dmx->Open(DMX_PES_CHANNEL, NULL, 64*1024);

	while (reader_running) {
		if(dvbsub_paused) {
			sub_debug.print(Debug::VERBOSE, "%s stopped\n", __FUNCTION__);
			dmx->Stop();
			dvbsub_stopped = 1;
			pthread_mutex_lock(&readerMutex );
			int ret = pthread_cond_wait(&readerCond, &readerMutex);
			pthread_mutex_unlock(&readerMutex);
			if (ret) {
				sub_debug.print(Debug::VERBOSE, "pthread_cond_timedwait fails with %d\n", ret);
			}
			if(reader_running) {
				dmx->pesFilter(dvbsub_pid);
				dmx->Start();
				sub_debug.print(Debug::VERBOSE, "%s started: pid 0x%x\n", __FUNCTION__, dvbsub_pid);
				dvbsub_stopped = 0;
			} else
				break;
		}

		/* Wait for pes sync */
		len = 0;
		count = 0;
		int tosync = 0;

		while (!dvbsub_paused) {
			//len = read(fd, &tmp[count], 3-count);
			len = dmx->Read(&tmp[count], 3-count, 1000);
			//printf("reader: fd = %d, len = %d\n", fd, len);
			if(len <= 0)
				continue;
			else if (len == (3-count)) {
				if (	(tmp[0] == 0x00) &&
					(tmp[1] == 0x00) &&
					(tmp[2] == 0x01)) {
					count = 3;
					break;
				}
				tosync += len;
			} else {
				count += len;
				tosync += len;
				continue;
			} 
			tmp[0] = tmp[1];
			tmp[1] = tmp[2];
			count = 2;
		} // while (!dvbsub_paused)
		if(tosync)
			printf("[subtitles] sync after %d bytes\n", tosync);
		/* read stream id & length */
		while ((count < 6) && !dvbsub_paused) {
			//printf("try to read 6-count = %d\n", 6-count);
			//len = read(fd, &tmp[count], 6-count);
			len = dmx->Read(&tmp[count], 6-count, 1000);
			if (len < 0) {
				continue;
			} else {
				count += len;
			}
		}

		packlen =  getbits(tmp, 4*8, 16) + 6;	

		buf = new uint8_t[packlen];

		/* TODO: Throws an exception on out of memory */

		/* copy tmp[0..5] => buf[0..5] */
		*(uint32_t*)buf = *(uint32_t*)tmp;
		((uint16_t*)buf)[2] = ((uint16_t*)tmp)[2];

		/* read rest of the packet */
		while((count < packlen) && !dvbsub_paused) {
			//len = read(fd, buf+count, packlen-count);
			len = dmx->Read(buf+count, packlen-count, 1000);
			if (len < 0) {
				continue;
			} else {
				count += len;
			}
		}
		if(!dvbsub_paused) {
			printf("[subtitles] adding packet, len %d\n", count);
			/* Packet now in memory */
			packet_queue.push(buf);
			/* TODO: allocation exception */
			// wake up dvb thread
			pthread_mutex_lock(&packetMutex);
			pthread_cond_broadcast(&packetCond);
			pthread_mutex_unlock(&packetMutex);
		} else {
			delete[] buf;
			buf=NULL;
		}
	}

	dmx->Stop();
	delete dmx;
	dmx = NULL;

	sub_debug.print(Debug::VERBOSE, "%s shutdown\n", __FUNCTION__);
	pthread_exit(NULL);
}

void* dvbsub_thread(void* arg)
{
	struct timespec restartWait;
	struct timeval now;

	sub_debug.print(Debug::VERBOSE, "%s started\n", __FUNCTION__);
	if (!dvbSubtitleConverter)
		dvbSubtitleConverter = new cDvbSubtitleConverter;

	while(dvbsub_running) {
		uint8_t* packet;
		int64_t pts;
		int pts_dts_flag;
		int dataoffset;
		int packlen;

		gettimeofday(&now, NULL);
		TIMEVAL_TO_TIMESPEC(&now, &restartWait);
		restartWait.tv_sec += 1;

		int ret = 0;
		if(packet_queue.size() == 0) {
			pthread_mutex_lock( &packetMutex );
			ret = pthread_cond_timedwait( &packetCond, &packetMutex, &restartWait );
			pthread_mutex_unlock( &packetMutex );
		}
		if (ret == ETIMEDOUT)
		{
			dvbSubtitleConverter->Action();
			continue;
		}
		else if (ret == EINTR)
		{
			sub_debug.print(Debug::VERBOSE, "pthread_cond_timedwait fails with %s\n", strerror(errno));
		}
		sub_debug.print(Debug::VERBOSE, "\nPES: Wakeup, queue size %d\n", packet_queue.size());
		if(dvbsub_paused) {
			do {
				packet = packet_queue.pop();
				if(packet)
					delete[] packet;
			} while(packet);
			continue;
		}
		packet = packet_queue.pop();
		if (!packet) {
			sub_debug.print(Debug::VERBOSE, "Error no packet found\n");
			continue;
		}
		packlen = (packet[4] << 8 | packet[5]) + 6;

		/* Get PTS */
		pts_dts_flag = getbits(packet, 7*8, 2);
		if ((pts_dts_flag == 2) || (pts_dts_flag == 3)) {
			pts = (uint64_t)getbits(packet, 9*8+4, 3) << 30;  /* PTS[32..30] */
			pts |= getbits(packet, 10*8, 15) << 15;           /* PTS[29..15] */
			pts |= getbits(packet, 12*8, 15);                 /* PTS[14..0] */
		} else {
			pts = 0;
		}

		dataoffset = packet[8] + 8 + 1;
		if (packet[dataoffset] != 0x20) {
			sub_debug.print(Debug::VERBOSE, "Not a dvb subtitle packet, discard it (len %d)\n", packlen);
			for(int i = 0; i < packlen; i++)
				printf("%02X ", packet[i]);
			printf("\n");
			goto next_round;
		}

		sub_debug.print(Debug::VERBOSE, "PES packet: len %d PTS=%Ld (%02d:%02d:%02d.%d)\n",
				packlen, pts, (int)(pts/324000000), (int)((pts/5400000)%60),
				(int)((pts/90000)%60), (int)(pts%90000));

		if (packlen <= dataoffset + 3) {
			sub_debug.print(Debug::INFO, "Packet too short, discard\n");
			goto next_round;
		}

		if (packet[dataoffset + 2] == 0x0f) {
			dvbSubtitleConverter->Convert(&packet[dataoffset + 2],
					packlen - (dataoffset + 2), pts);
		} else {
			sub_debug.print(Debug::INFO, "End_of_PES is missing\n");
		}
		dvbSubtitleConverter->Action();

next_round:
		delete[] packet;
	}

	delete dvbSubtitleConverter;

	sub_debug.print(Debug::VERBOSE, "%s shutdown\n", __FUNCTION__);
	pthread_exit(NULL);
}

void dvbsub_get_stc(int64_t * STC)
{
	if(dmx)
		dmx->getSTC(STC);
}
