/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2011-2012 CoolStream International Ltd

	based on code which is
	Copyright (C) 2002 Andreas Oberritter <obi@tuxbox.org>
	Copyright (C) 2001 TripleDES
	Copyright (C) 2000, 2001 Marcus Metzler <marcus@convergence.de>

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

#include <global.h>
#include <neutrino.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>

#include <string.h>

#include <dmx.h>
#include <zapit/capmt.h>
#include <zapit/zapit.h>
#include <zapit/pat.h>
#include <driver/streamts.h>
#include <driver/record.h>
#include <driver/genpsi.h>

/* experimental mode:
 * stream not possible, if record running
 * pids in url ignored, and added from channel, with fake PAT/PMT
 * different channels supported, only from the same transponder - no zap is done,
 * with url like http://coolstream:31339/id=c32400030070283e (channel id)
 * TODO: multi-tuner support
 */
#define ENABLE_MULTI_CHANNEL

#define TS_SIZE 188
#define DMX_BUFFER_SIZE (2048*TS_SIZE)
#define IN_SIZE (250*TS_SIZE)

CStreamInstance::CStreamInstance(int clientfd, t_channel_id chid, stream_pids_t &_pids)
{
	printf("CStreamInstance:: new channel %" PRIx64 " fd %d\n", chid, clientfd);
	fds.insert(clientfd);
	pids = _pids;
	channel_id = chid;
	running = false;
	dmx = NULL;
	buf = NULL;
}

CStreamInstance::~CStreamInstance()
{
	Stop();
	Close();
}

bool CStreamInstance::Start()
{
	if (running)
		return false;

	buf =  new unsigned char [IN_SIZE];
	if (buf == NULL) {
		perror("CStreamInstance::Start: buf");
		return false;
	}
	running = true;
	printf("CStreamInstance::Start: %" PRIx64 "\n", channel_id);
	return (OpenThreads::Thread::start() == 0);
}

bool CStreamInstance::Stop()
{
	if (!running)
		return false;

	printf("CStreamInstance::Stop: %" PRIx64 "\n", channel_id);
	running = false;
	return (OpenThreads::Thread::join() == 0);
}

bool CStreamInstance::Send(ssize_t r)
{
	mutex.lock();
	for (stream_fds_t::iterator it = fds.begin(); it != fds.end(); ++it) {
		int ret, i = 10;
		do {
			ret = send(*it, buf, r, MSG_DONTWAIT);
#if 0
			if (ret != r)
				usleep(100);
#endif
		} while ((ret != r) && (i-- > 0));
		if (ret != r) {
			if (r < 0)
				perror("send");
			printf("send err, fd %d: (%d from %d)\n", *it, ret, (int)r);
		}
	}
	mutex.unlock();
	return true;
}

void CStreamInstance::Close()
{
	for (stream_fds_t::iterator fit = fds.begin(); fit != fds.end(); ++fit)
		close(*fit);
	fds.clear();
}

void CStreamInstance::AddClient(int clientfd)
{
	mutex.lock();
	fds.insert(clientfd);
	printf("CStreamInstance::AddClient: %d (count %d)\n", clientfd, (int)fds.size());
	mutex.unlock();
}

void CStreamInstance::RemoveClient(int clientfd)
{
	mutex.lock();
	fds.erase(clientfd);
	close(clientfd);
	printf("CStreamInstance::RemoveClient: %d (count %d)\n", clientfd, (int)fds.size());
	mutex.unlock();
}

void CStreamInstance::run()
{
	printf("CStreamInstance::run: %" PRIx64 "\n", channel_id);

#if 0
// TODO: check if this works... #ifndef HAVE_COOL_HARDWARE
	/* right now, only one stream is possible anyway and it is not possible
	 * to stream a different channel than the live channel AFAICT, so we can
	 * as well use the live demux */
	dmx = new cDemux(0);
#endif
#if 0
	dmx = new cDemux(STREAM_DEMUX);//FIXME
#endif
	CZapitChannel * tmpchan = CServiceManager::getInstance()->FindChannel(channel_id);
	if (!tmpchan)
		return;

	dmx = new cDemux(tmpchan->getRecordDemux());//FIXME
	dmx->Open(DMX_TP_CHANNEL, NULL, DMX_BUFFER_SIZE);

	/* pids here cannot be empty */
	stream_pids_t::iterator it = pids.begin();
	printf("CStreamInstance::run: add pid %x\n", *it);
	dmx->pesFilter(*it);
	++it;
	for (; it != pids.end(); ++it) {
		printf("CStreamInstance::run: add pid %x\n", *it);
		dmx->addPid(*it);
	}
#ifdef ENABLE_MULTI_CHANNEL
	dmx->Start();//FIXME
#else
	dmx->Start(true);//FIXME
#endif

	CCamManager::getInstance()->Start(channel_id, CCamManager::STREAM);

	while (running) {
		ssize_t r = dmx->Read(buf, IN_SIZE, 100);
		if(r > 0)
			Send(r);
	}

	CCamManager::getInstance()->Stop(channel_id, CCamManager::STREAM);

	printf("CStreamInstance::run: exiting %" PRIx64 " (%d fds)\n", channel_id, (int)fds.size());

	Close();
	delete dmx;
	delete []buf;
}

bool CStreamInstance::HasFd(int fd)
{
	if (fds.find(fd) != fds.end())
		return true;
	return false;
}

/************************************************************************/
CStreamManager *CStreamManager::sm = NULL;
CStreamManager::CStreamManager()
{
	enabled = true;
	running = false;
	listenfd = -1;
	port = 31339;
}

CStreamManager::~CStreamManager()
{
	Stop();
}

CStreamManager * CStreamManager::getInstance()
{
	if (sm == NULL)
		sm = new CStreamManager();
	return sm;
}

bool CStreamManager::Start(int _port)
{
	if (running)
		return false;

	if (_port)
		port = _port;
	if (!Listen())
		return false;

	running = true;
	return (OpenThreads::Thread::start() == 0);
}

bool CStreamManager::Stop()
{
	if (!running)
		return false;
	running = false;
	return (OpenThreads::Thread::join() == 0);
}

bool CStreamManager::SetPort(int newport)
{
	bool ret = false;
	if (port != newport) {
		port = newport;
#if 0
		Stop();
		ret = Start(newport);
#endif
		mutex.lock();
		if (listenfd >= 0)
			close(listenfd);
		ret = Listen();
		mutex.unlock();
	}
	return ret;
}

bool CStreamManager::Parse(int fd, stream_pids_t &pids, t_channel_id &chid)
{
	char cbuf[512];
	char *bp;

	FILE * fp = fdopen(fd, "r+");
	if(fp == NULL) {
		perror("fdopen");
		return false;
	}
	cbuf[0] = 0;
	bp = &cbuf[0];

	/* read one line */
	while (bp - &cbuf[0] < (int) sizeof(cbuf)) {
		unsigned char c;
		int res = read(fd, &c, 1);
		if(res < 0) {
			perror("read");
			return false;
		}
		if ((*bp++ = c) == '\n')
			break;
	}

	*bp++ = 0;
	bp = &cbuf[0];

	printf("CStreamManager::Parse: got %s\n", cbuf);

	/* send response to http client */
	if (!strncmp(cbuf, "GET /", 5)) {
		fprintf(fp, "HTTP/1.1 200 OK\r\nServer: streamts (%s)\r\n\r\n", "ts" /*&argv[1][1]*/);
		fflush(fp);
		bp += 5;
	} else {
		printf("Received garbage\n");
		return false;
	}

#ifndef ENABLE_MULTI_CHANNEL
	/* parse stdin / url path, start dmx filters */
	do {
		int pid;
		int res = sscanf(bp, "%x", &pid);
		if(res == 1) {
			printf("New pid: 0x%x\n", pid);
			pids.insert(pid);
		}
	}
	while ((bp = strchr(bp, ',')) && (bp++));
#endif

	chid = CZapit::getInstance()->GetCurrentChannelID();
	CZapitChannel * channel = CZapit::getInstance()->GetCurrentChannel();

	int mode = CNeutrinoApp::getInstance()->getMode();
	if (mode == NeutrinoMessages::mode_standby && streams.empty()) {
		printf("CStreamManager::Parse: wakeup zapit..\n");
		g_Zapit->setStandby(false);
		g_Zapit->getMode();
	}
	if(pids.empty()) {
#ifdef ENABLE_MULTI_CHANNEL
		t_channel_id tmpid;
		bp = &cbuf[5];
		if (sscanf(bp, "id=%" SCNx64, &tmpid) == 1) {
			printf("############################# channel_id %" PRIx64 "\n", tmpid);

			CZapitChannel * tmpchan = CServiceManager::getInstance()->FindChannel(tmpid);
			if (tmpchan && (tmpid != chid) && SAME_TRANSPONDER(tmpid, chid)) {
				printf("############################# channel_id %" PRIx64 " -> zap\n", tmpid);
				bool ret = g_Zapit->zapTo_record(tmpid) > 0;
				if (ret) {
					channel = tmpchan;
					chid = tmpid;
				}
			}
		}
		if(CRecordManager::getInstance()->RecordingStatus(chid)) {
			printf("CStreamManager::Parse: channel %" PRIx64 " recorded, aborting..\n", chid);
			return false;
		}
#ifdef ENABLE_PIP
		t_channel_id pip_channel_id = CZapit::getInstance()->GetPipChannelID();
		if ((chid == pip_channel_id) && (channel->getRecordDemux() == channel->getPipDemux())) {
			printf("CStreamManager::Parse: channel %llx used for pip, aborting..\n", chid);
			return false;
		}
#endif
#endif

		printf("CStreamManager::Parse: no pids in url, using channel %" PRIx64 " pids\n", chid);
		if(!channel)
			return false;
		//pids.insert(0);
		//pids.insert(channel->getPmtPid());
		pids.insert(channel->getVideoPid());
		for (int i = 0; i <  channel->getAudioChannelCount(); i++)
			pids.insert(channel->getAudioChannel(i)->pid);
	}
	CGenPsi psi;
	for (stream_pids_t::iterator it = pids.begin(); it != pids.end(); ++it) {
		if (*it == channel->getVideoPid()) {
			printf("CStreamManager::Parse: genpsi vpid %x (%d)\n", *it, channel->type);
			psi.addPid(*it, channel->type ? EN_TYPE_AVC : EN_TYPE_VIDEO, 0);
		} else {
			for (int i = 0; i <  channel->getAudioChannelCount(); i++) {
				if (*it == channel->getAudioChannel(i)->pid) {
					CZapitAudioChannel::ZapitAudioChannelType atype = channel->getAudioChannel(i)->audioChannelType;
					printf("CStreamManager::Parse: genpsi apid %x (%d)\n", *it, atype);
					psi.addPid(*it, EN_TYPE_AUDIO, atype);
				}
			}
		}
	}
	psi.genpsi(fd);

	return !pids.empty();
}

void CStreamManager::run()
{
	struct sockaddr_in servaddr;
	int clilen = sizeof(servaddr);;

	struct pollfd pfd[128];
	int poll_cnt;

	printf("Starting STREAM thread keeper, tid %ld\n", syscall(__NR_gettid));

	while (running) {
		mutex.lock();
		pfd[0].fd = listenfd;
		pfd[0].events = (POLLIN | POLLPRI);
		pfd[0].revents = 0;
		poll_cnt = 1;
		for (streammap_iterator_t it = streams.begin(); it != streams.end(); ++it) {
			stream_fds_t fds = it->second->GetFds();
			for (stream_fds_t::iterator fit = fds.begin(); fit != fds.end(); ++fit) {
				pfd[poll_cnt].fd = *fit;
				pfd[poll_cnt].events = POLLRDHUP | POLLHUP;
				pfd[poll_cnt].revents = 0;
				poll_cnt++;
			}
		}
		mutex.unlock();
//printf("polling, count= %d\n", poll_cnt);
		int pollres = poll (pfd, poll_cnt, 1000);
		if (pollres < 0) {
			perror("CStreamManager::run(): poll");
			continue;
		}
		if(pollres == 0)
			continue;
		for (int i = poll_cnt - 1; i >= 0; i--) {
			if (pfd[i].revents & (POLLIN | POLLPRI | POLLHUP | POLLRDHUP)) {
				printf("fd %d has events %x\n", pfd[i].fd, pfd[i].revents);
				if (pfd[i].fd == listenfd) {
					int connfd = accept (listenfd, (struct sockaddr *) &servaddr, (socklen_t *) & clilen);
					printf("CStreamManager::run(): connection, fd %d\n", connfd);
					if(connfd < 0) {
						perror("CStreamManager::run(): accept");
						continue;
					}
					stream_pids_t pids;
					t_channel_id channel_id;
					if (Parse(connfd, pids, channel_id)) {
						mutex.lock();
						streammap_iterator_t it = streams.find(channel_id);
						if (it != streams.end()) {
							it->second->AddClient(connfd);
						} else {
							CStreamInstance * stream = new CStreamInstance(connfd, channel_id, pids);
							if (stream->Start())
								streams.insert(streammap_pair_t(channel_id, stream));
							else
								delete stream;
						}
						mutex.unlock();
					} else {
						close(connfd);
					}
				} else {
					if (pfd[i].revents & (POLLHUP | POLLRDHUP)) {
						printf("CStreamManager::run(): POLLHUP, fd %d\n", pfd[i].fd);
						mutex.lock();
						for (streammap_iterator_t it = streams.begin(); it != streams.end(); ++it) {
							if (it->second->HasFd(pfd[i].fd)) {
								CStreamInstance *stream = it->second;
								stream->RemoveClient(pfd[i].fd);
								if (stream->GetFds().empty()) {
									streams.erase(stream->GetChannelId());
									delete stream;
								}
								break;
							}
						}
						mutex.unlock();
					}
				}
			}
		}
	}
	printf("CStreamManager::run: stopping...\n");
	close(listenfd);
	listenfd = -1;
	StopAll();
}

bool CStreamManager::StopAll()
{
	bool ret = !streams.empty();
	for (streammap_iterator_t it = streams.begin(); it != streams.end(); ++it) {
		it->second->Stop();
		delete it->second;
	}
	streams.clear();
	return ret;
}

bool CStreamManager::StopStream(t_channel_id channel_id)
{
	bool ret = false;
	mutex.lock();
	if (channel_id) {
		streammap_iterator_t it = streams.find(channel_id);
		if (it != streams.end()) {
			delete it->second;
			streams.erase(channel_id);
			ret = true;
		}
	} else {
		ret = StopAll();
	}
	mutex.unlock();
	return ret;
}

bool CStreamManager::StreamStatus(t_channel_id channel_id)
{
	bool ret;
	mutex.lock();
	if (channel_id)
		ret = (streams.find(channel_id) != streams.end());
	else
		ret = !streams.empty();
	mutex.unlock();
	return ret;
}

bool CStreamManager::Listen()
{
	struct sockaddr_in socketAddr;
	int socketOptActive = 1;
	int sendsize = 10*IN_SIZE;
	unsigned int m = sizeof(sendsize);

	if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf (stderr, "network port %u open: ", port);
		perror ("socket");
		return false;
	}

	if (setsockopt (listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&socketOptActive, sizeof (int)) < 0) {
		fprintf (stderr, "network port %u open: error setsockopt\n", port);
		perror ("setsockopt");
		goto _error;
	}

	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons (port);
	socketAddr.sin_addr.s_addr = htonl (INADDR_ANY);

	if (bind (listenfd, (struct sockaddr *) &socketAddr, sizeof (socketAddr)) < 0) {
		fprintf (stderr, "network port %u open: ", port);
		perror ("bind");
		goto _error;
	}

	if (listen (listenfd, 5) < 0) {
		fprintf (stderr, "network port %u open: ", port);
		perror ("listen");
		goto _error;
	}

#if 1
	setsockopt(listenfd, SOL_SOCKET, SO_SNDBUF, (void *)&sendsize, m);
	sendsize = 0;
	getsockopt(listenfd, SOL_SOCKET, SO_SNDBUF, (void *)&sendsize, &m);
	printf("CStreamManager::Listen: on %d, fd %d (%d)\n", port, listenfd, sendsize);
#endif
	return true;
_error:
	close (listenfd);
	return false;
}
