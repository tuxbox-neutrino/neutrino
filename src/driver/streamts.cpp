#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <poll.h>
#include <syscall.h>

/* work around for building with old kernel headers */
#ifndef POLLRDHUP
#define POLLRDHUP 0
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>

#include <string.h>

#include <dmx.h>
#include <zapit/cam.h>
#include <zapit/zapit.h>


#define TS_SIZE 188
#define IN_SIZE		(2048 * TS_SIZE)
//#define IN_SIZE         (TS_SIZE * 362)

#define DMX_BUFFER_SIZE (2 * 3008 * 62)

/* maximum number of pes pids */
#define MAXPIDS		64

/* tcp packet data size */
//#define PACKET_SIZE	1448
#define PACKET_SIZE	7*TS_SIZE

//unsigned char * buf;

extern CCam *cam0;

//int demuxfd[MAXPIDS];

static unsigned char exit_flag = 0;
static unsigned int writebuf_size = 0;
static unsigned char writebuf[PACKET_SIZE];

void packet_stdout (int fd, unsigned char * buf, int count, void * /*p*/)
{

	unsigned int size;
	unsigned char * bp;
	ssize_t written;

//printf("packet_stdout count %d\n", count);
	/* ensure, that there is always at least one complete
	 * packet inside of the send buffer */
	while (writebuf_size + count >= PACKET_SIZE) {

		/* how many bytes are to be sent from the input buffer? */
		size = PACKET_SIZE - writebuf_size;

		/* send buffer is not empty, so copy from
		   input buffer to get a complete packet */
		if (writebuf_size) {
			memmove(writebuf + writebuf_size, buf, size);
			bp = writebuf;
		}

		/* if send buffer is empty, then do not memcopy,
		   but send directly from input buffer */
		else {
			bp = buf;
		}

		/* write the packet, count the amount of really written bytes */
		written = write(fd, bp, PACKET_SIZE);

		/*  exit on error */
		if (written == -1) {
			perror("write");
			exit_flag = 1;
			return;
		}

		/* if the packet could not be written completely, then
		 * how many bytes must be stored in the send buffer
		 * until the next packet is to be sent?  */
		writebuf_size = PACKET_SIZE - written;

		/* move all bytes of the packet which were not sent
		 * to the beginning of the send buffer */
		if (writebuf_size)
			memmove(writebuf, bp + written, writebuf_size);

		/* * advance in the input buffer */
		buf += size;

		/* * decrease the todo size */
		count -= size;
	}

	/* if there are still some bytes left in the input buffer,
	 * then store them in the send buffer and increase send
	 * buffer size */
	if (count) {
		memmove(writebuf + writebuf_size, buf, count);
		writebuf_size += count;
	}
}

int open_incoming_port (int port)
{
	struct sockaddr_in socketAddr;
	int socketOptActive = 1;
	int handle;

	if (!port)
		return -1;

	if ((handle = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fprintf (stderr, "network port %u open: ", port);
		perror ("socket");
		return -1;
	}

	if (setsockopt (handle, SOL_SOCKET, SO_REUSEADDR, (const void *)&socketOptActive, sizeof (int)) < 0)
	{
		fprintf (stderr, "network port %u open: error setsockopt\n", port);
		close (handle);
		return -1;
	}

	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons (port);
	socketAddr.sin_addr.s_addr = htonl (INADDR_ANY);

	if (bind (handle, (struct sockaddr *) &socketAddr, sizeof (socketAddr)) < 0)
	{
		fprintf (stderr, "network port %u open: ", port);
		perror ("bind");
		close (handle);
		return -1;
	}

	if (listen (handle, 5) < 0)
	{
		fprintf (stderr, "network port %u open: ", port);
		perror ("listen");
		close (handle);
		return -1;
	}
	return handle;
}

void * streamts_live_thread(void *data);
int streamts_stop;

void streamts_main_thread(void * /*data*/)
{
	struct sockaddr_in servaddr;
	int clilen;

	struct pollfd pfd[128];
	int poll_cnt, tcnt;
	int listenfd;
	int connfd = -1;
	int pollres;
	int i;
	pthread_t st = 0;

	printf("Starting STREAM thread keeper, tid %ld\n", syscall(__NR_gettid));

	listenfd = open_incoming_port(31339);
	if(listenfd < 0) {
		printf("Open incoming port failed\n");
		return;
	}
	printf("listenfd %d\n", listenfd);

	clilen = sizeof (servaddr);
	pfd[0].fd = listenfd;
	pfd[0].events = (POLLIN | POLLPRI);
	pfd[0].revents = 0;
	tcnt = 1;
	streamts_stop = 0;

	while (!streamts_stop) {
		poll_cnt = tcnt;
//printf("polling, count= %d\n", poll_cnt);
		pollres = poll (pfd, poll_cnt, 1000);
		if (pollres < 0) {
			perror("streamts_main_thread poll");
			continue;
		}
		if(pollres == 0)
			continue;
		for (i = poll_cnt - 1; i >= 0; i--) {
			if (pfd[i].revents & (POLLIN | POLLPRI | POLLHUP | POLLRDHUP)) {
				printf("fd %d has events %x\n", pfd[i].fd, pfd[i].revents);
				if (pfd[i].fd == listenfd) {
					if(connfd >= 0)
						close(connfd);
					connfd = accept (listenfd, (struct sockaddr *) &servaddr, (socklen_t *) & clilen);
					printf("new connection, fd %d\n", connfd);
					if(connfd < 0) {
						perror("accept");
						continue;
					}
					if(st != 0) {
						printf("New connection, stopping stream thread\n");
						exit_flag = 1;
						pthread_join(st, NULL);
						tcnt --;
					}
					pfd[tcnt].fd = connfd;
					pfd[tcnt].events = POLLRDHUP | POLLHUP;
					pfd[tcnt].revents = 0;
					tcnt++;
					exit_flag = 0;
					pthread_create (&st, NULL, streamts_live_thread, (void *) connfd);
				} else {
					if (pfd[i].revents & (POLLHUP | POLLRDHUP)) {
						connfd = -1;
						printf("Client disconnected, stopping stream thread\n");
						exit_flag = 1;
						if(st)
							pthread_join(st, NULL);
						st = 0;
						tcnt --;
					}
				}
			}
		}
	}
	printf("Stopping STREAM thread keeper\n");
	close(listenfd);
	if(st != 0) {
		printf("Stopping stream thread\n");
		exit_flag = 1;
		pthread_cancel(st);
		pthread_join(st, NULL);
		close(connfd);
	}
	return;
}

void * streamts_live_thread(void *data)
{
	unsigned char * buf;
	int pid;
	int pids[MAXPIDS];
	char cbuf[512];
	char *bp;
	int fd = (int) data;
	FILE * fp;
	unsigned char demuxfd_count = 0;

	printf("Starting LIVE STREAM thread, fd %d\n", fd);
	fp = fdopen(fd, "r+");
	if(fp == NULL) {
		perror("fdopen");
		return 0;
	}

	writebuf_size = 0;

	cbuf[0] = 0;
	bp = &cbuf[0];

	/* read one line */
	while (bp - &cbuf[0] < (int) sizeof(cbuf)) {
		unsigned char c;
		int res = read(fd, &c, 1);
		if(res < 0) {
			perror("read");
			return 0;
		}
		if ((*bp++ = c) == '\n')
			break;
	}

	*bp++ = 0;
	bp = &cbuf[0];

	printf("stream: got %s\n", cbuf);

	/* send response to http client */
	if (!strncmp(cbuf, "GET /", 5)) {
		fprintf(fp, "HTTP/1.1 200 OK\r\nServer: streamts (%s)\r\n\r\n", "ts" /*&argv[1][1]*/);
		fflush(fp);
		bp += 5;
	} else {
		printf("Received garbage\n");
		return 0;
	}

	/* parse stdin / url path, start dmx filters */
	do {
		int res = sscanf(bp, "%x", &pid);
		if(res == 1) {
			printf("New pid: 0x%x\n", pid);
			pids[demuxfd_count++] = pid;
		}
	}
	while ((bp = strchr(bp, ',')) && (bp++) && (demuxfd_count < MAXPIDS));

	if(demuxfd_count == 0) {
		printf("No pids!\n");
		return 0;
	}

	buf = (unsigned char *) malloc(IN_SIZE);
	if (buf == NULL) {
		perror("malloc");
		return 0;
	}

	cDemux * dmx = new cDemux(STREAM_DEMUX);//FIXME

	dmx->Open(DMX_TP_CHANNEL, NULL, DMX_BUFFER_SIZE);

	dmx->pesFilter(pids[0]);
	for(int i = 1; i < demuxfd_count; i++)
		dmx->addPid(pids[i]);

	dmx->Start(true);//FIXME

	CCamManager::getInstance()->Start(CZapit::getInstance()->GetCurrentChannelID(), CCamManager::STREAM);
	ssize_t r;

	while (!exit_flag) {
		r = dmx->Read(buf, IN_SIZE, 100);
		if(r > 0)
			packet_stdout(fd, buf, r, NULL);
	}

	printf("Exiting LIVE STREAM thread, fd %d\n", fd);

	CCamManager::getInstance()->Stop(CZapit::getInstance()->GetCurrentChannelID(), CCamManager::STREAM);

	delete dmx;
	free(buf);
	close(fd);
	return 0;
}

void streamts_file_thread(void *data)
{
	int dvrfd;
	unsigned char * buf;
	char cbuf[512];
	char *bp;
	unsigned char mode = 0;
	char tsfile[IN_SIZE];
	int tsfilelen = 0;
	int fileslice = 0;
	int i = 0;
	int fd = (int) data;

	buf = (unsigned char *) malloc(IN_SIZE);

	if (buf == NULL) {
		perror("malloc");
		return;
	}

	bp = &cbuf[0];

	/* read one line */
	while (bp - &cbuf[0] < IN_SIZE) {
		unsigned char c;
		read(fd, &c, 1);
		if ((*bp++ = c) == '\n')
			break;
	}

	*bp++ = 0;
	bp = &cbuf[0];

	/* send response to http client */
	if (!strncmp(cbuf, "GET /", 5)) {
		printf("HTTP/1.1 200 OK\r\nServer: streamts (%s)\r\n\r\n", "ts" /*&argv[1][1]*/);
		fflush(stdout);
		bp += 5;
	}

	/* ts filename */
	int j = 0;
	i = 0;
	while (i < (int) strlen(bp) - 3)
	{
		if ((bp[i] == '.') && (bp[i + 1] == 't') && (bp[i + 2] == 's'))
		{
			tsfile[j] = bp[i];
			tsfile[j + 1] = bp[i + 1];
			tsfile[j + 2] = bp[i + 2];
			tsfile[j + 3] = '\0';
			break;
		}
		else
			if ((bp[i] == '%') && (bp[i + 1] == '2') && (bp[i + 2] == '0'))
			{
				tsfile[j++] = ' ';
				i += 3;
			}
			else
				tsfile[j++] = bp[i++];
	}
	tsfilelen = strlen(tsfile);
	/* open ts file */
	if ((dvrfd = open(tsfile, O_RDONLY)) < 0) {
		free(buf);
		return;
	}

	size_t pos;
	ssize_t r;

	while (!exit_flag) {
		/* always read IN_SIZE bytes */
		for (pos = 0; pos < IN_SIZE; pos += r) {
			r = read(dvrfd, buf + pos, IN_SIZE - pos);
			if (r == -1) {
				/* Error */
				exit_flag = 1;
				break;
			} else if (r == 0) {
				/* End of file */
				if (mode == 3) {
					close(dvrfd);
					sprintf(&tsfile[tsfilelen], ".%03d", ++fileslice);
					dvrfd = open(tsfile, O_RDONLY);
				}
				if ((dvrfd == -1) || (mode != 3)) {
					exit_flag = 1;
					break;
				}
			}
		}
		packet_stdout(fd, buf, pos, NULL);
	}
	close(dvrfd);
	free(buf);

	return;
}
