/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2011-2014 CoolStream International Ltd

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
#include <string>
#include <vector>

#include <hardware/dmx.h>
#include <zapit/capmt.h>
#include <zapit/zapit.h>
#include <zapit/pat.h>
#include <zapit/streamhold.h>
#include <zapit/client/msgtypes.h>
#include <driver/streamts.h>
#include <driver/abstime.h>
#include <driver/record.h>
#include <driver/genpsi.h>
#include <system/set_threadname.h>
#include <gui/movieplayer.h>
#include <cs_api.h>
/* libstb-hal only; coolstream builds keep the historic block below.
 * Same gating as version_hal.h in gui/imageinfo.cpp. */
#if HAVE_LIBSTB_HAL
#include <streaminput_ffmpeg.h>
#endif

#if (LIBAVCODEC_VERSION_MAJOR > 55)
#define	av_free_packet av_packet_unref
#else
#define av_packet_unref	av_free_packet
#endif

#define TS_SIZE 188
#define DMX_BUFFER_SIZE (2048*TS_SIZE)
#define IN_SIZE (250*TS_SIZE)
/* Room for ten reads in each client's socket. */
#define SEND_BUFFER_SIZE (10*IN_SIZE)

CStreamInstance::CStreamInstance(int clientfd, t_channel_id chid, stream_pids_t &_pids)
{
	printf("CStreamInstance:: new channel %" PRIx64 " fd %d\n", chid, clientfd);
	fds.insert(clientfd);
	pids = _pids;
	channel_id = chid;
	hold_id = 0;
	running = false;
	dmx = NULL;
	buf = NULL;
	frontend = NULL;
	uses_ca = true;
}

CStreamInstance::~CStreamInstance()
{
	Stop();
	Close();
	/* zapit gives the frontend back after its current command. */
	if (hold_id)
		streamhold::drop(hold_id);
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
	printf("CStreamInstance::Start: %" PRIx64 "\n", channel_id);
	running = true;
	if (OpenThreads::Thread::start() != 0) {
		running = false;
		delete [] buf;
		buf = NULL;
		return false;
	}
	return true;
}

bool CStreamInstance::Stop()
{
	if (!running)
		return false;

	printf("CStreamInstance::Stop: %" PRIx64 "\n", channel_id);
	running = false;
	return (OpenThreads::Thread::join() == 0);
}

bool CStreamInstance::Send(ssize_t r, const unsigned char *_buf)
{
	if (r <= 0)
		return true;
	const unsigned char *b = _buf ? _buf : buf;

	/* Each send returns at once: a client that does not keep up must not
	   stall the demux reader, or the kernel's buffer overflows and every
	   client loses the stream. The lock is held over all clients, so a
	   client removed meanwhile is closed only once the loop is done and
	   its descriptor cannot reach a new client in the middle of it. */
	tsgrid::SocketSink sink;
	std::vector<std::string> report;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		clients.send(fds, b, (size_t) r, sink, time_monotonic_ms(), report);
	}
	for (size_t i = 0; i < report.size(); ++i)
		printf("CStreamInstance::%s: %s\n", __FUNCTION__, report[i].c_str());

	return true;
}

void CStreamInstance::Close()
{
	stream_fds_t cfds;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		cfds = fds;
		fds.clear();
		clients.clear();
	}
	for (stream_fds_t::iterator fit = cfds.begin(); fit != cfds.end(); ++fit)
		close(*fit);
}

void CStreamInstance::AddClient(int clientfd)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	fds.insert(clientfd);
	printf("CStreamInstance::AddClient: %d (count %d)\n", clientfd, (int)fds.size());
}

void CStreamInstance::RemoveClient(int clientfd)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	if (fds.erase(clientfd) > 0) {
		clients.forget(clientfd);
		close(clientfd);
	}
	printf("CStreamInstance::RemoveClient: %d (count %d)\n", clientfd, (int)fds.size());
}

void CStreamInstance::GetFdsCopy(stream_fds_t &out)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	out = fds;
}

size_t CStreamInstance::GetFdCount()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	return fds.size();
}

bool CStreamInstance::Open()
{
	printf("CStreamInstance::run: %" PRIx64 "\n", channel_id);
	set_threadname("n:streaminstance");

	CZapitChannel * tmpchan = CServiceManager::getInstance()->FindChannel(channel_id);
	if (!tmpchan)
		return false;

	dmx = new cDemux(tmpchan->getStreamDemux());//FIXME
	if(!dmx)
		return false;
	return dmx->Open(DMX_TP_CHANNEL, NULL, DMX_BUFFER_SIZE);
}

void CStreamInstance::run()
{
	printf("CStreamInstance::run: %" PRIx64 "\n", channel_id);
	set_threadname("n:streaminstance");

	/* pids here cannot be empty */
	stream_pids_t::iterator it = pids.begin();
	printf("CStreamInstance::run: add pid 0x%04x\n", *it);
	dmx->pesFilter(*it);
	++it;
	for (; it != pids.end(); ++it) {
		printf("CStreamInstance::run: add pid 0x%04x\n", *it);
		dmx->addPid(*it);
	}

	dmx->Start(true);

	if (uses_ca)
		CCamManager::getInstance()->Start(channel_id, CCamManager::STREAM);

	while (running) {
		/* A read need not end on a packet: the part of one it ends in
		   waits at the front of buf, and the next read goes behind it. */
		ssize_t r = dmx->Read(buf + grid.carry(), IN_SIZE - grid.carry(), 100);
		const size_t whole = grid.add(r);
		if (whole)
			Send(whole);
		grid.next(buf);
	}

	if (uses_ca)
		CCamManager::getInstance()->Stop(channel_id, CCamManager::STREAM);

	printf("CStreamInstance::run: exiting %" PRIx64 " (%d fds)\n", channel_id, (int)fds.size());

	Close();
	delete dmx;
	delete []buf;
}

bool CStreamInstance::HasFd(int fd)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
	return (fds.find(fd) != fds.end());
}

/************************************************************************/
CStreamManager *CStreamManager::sm = NULL;
CStreamManager::CStreamManager()
	: running(false), gate(mutex, running)
{
	enabled = true;
	listenfd = -1;
	port = g_settings.streaming_port;
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
	switch_wait.cancelAll();
	bool ret = false;
	if (OpenThreads::Thread::CurrentThread() == this) {
		cancel();
		ret = (OpenThreads::Thread::join() == 0);
	}
	StopAll();
	return ret;
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
		{
			close(listenfd);
			listenfd = -1;
		}
		ret = Listen();
		mutex.unlock();
	}
	return ret;
}

/* The request line names the channel: by id, in the service reference form,
   or not at all, which means the channel the box shows. A channel the box
   does not have is not found; it never falls back to the live channel. */
streamsetup::Ops::Read CStreamManager::Parse(int fd, streamsetup::Target &target)
{
	std::string line;
	if (!streamsetup::readRequest(fd, 2000, line)) {
		printf("CStreamManager::Parse: no request line in time\n");
		return streamsetup::Ops::ReadGarbage;
	}
	printf("CStreamManager::Parse: got %d bytes '%s'", (int) line.size(), line.c_str());

	const streamsetup::Request request = streamsetup::parseRequestLine(line);
	CZapitChannel * channel = NULL;
	bool is_e2 = false;

	switch (request.form) {
	case streamsetup::Garbage:
		printf("Received garbage\n");
		return streamsetup::Ops::ReadGarbage;
	case streamsetup::BadTarget:
		return streamsetup::Ops::ReadUnknown;
	case streamsetup::NoTarget:
		if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_standby)
			return streamsetup::Ops::ReadNoLive;
		channel = CZapit::getInstance()->GetCurrentChannel();
		if (channel == NULL)
			return streamsetup::Ops::ReadNoLive;
		break;
	case streamsetup::ById:
		channel = CServiceManager::getInstance()->FindChannel(request.id);
		if (channel == NULL)
			return streamsetup::Ops::ReadUnknown;
		printf("CStreamManager::Parse:N: channel_id %" PRIx64 " [%s] \n", channel->getChannelID(), channel->getName().c_str());
		break;
	case streamsetup::E2:
		channel = CServiceManager::getInstance()->FindChannel48Pos(request.id, request.satpos);
		if (channel == NULL)
			channel = CServiceManager::getInstance()->FindChannel48(request.id);
		if (channel == NULL)
			return streamsetup::Ops::ReadUnknown;
		printf("CStreamManager::Parse:E: channel_id %" PRIx64 " [%s] \n", channel->getChannelID(), channel->getName().c_str());
		is_e2 = true;
		break;
	}

	const t_channel_id chid = channel->getChannelID();
	target.channel = chid;
	target.kind = IS_WEBCHAN(chid) ? streamsetup::Web : streamsetup::Broadcast;
	target.e2 = is_e2;
	return streamsetup::Ops::ReadOk;
}

void CStreamManager::PreparePids(CZapitChannel *channel, stream_pids_t &pids, bool is_e2)
{
	pids.clear();

	pids.insert(0); //PAT
	printf("CStreamManager::PreparePids: PATpid 0x%04x \n", 0);
	pids.insert(channel->getPmtPid()); //PMT
	printf("CStreamManager::PreparePids: PMTpid 0x%04x \n", channel->getPmtPid());
	pids.insert(0x14); //TDT
	printf("CStreamManager::PreparePids: TDTpid 0x%04x \n", 0x14);

	printf("CStreamManager::PreparePids: searching channel %" PRIx64 " pids\n", channel->getChannelID());
	if (channel->getVideoPid())
	{
		pids.insert(channel->getVideoPid());
		printf("CStreamManager::PreparePids: vpid 0x%04x \n", channel->getVideoPid());
	}
	for (int i = 0; i <  channel->getAudioChannelCount(); i++)
	{
		pids.insert(channel->getAudioChannel(i)->pid);
		printf("CStreamManager::PreparePids: apid 0x%04x \n", channel->getAudioChannel(i)->pid);
	}

	if (!channel->capids.empty() && g_settings.streaming_ecmmode && is_e2)
	{
		for(casys_pids_iterator_t it = channel->capids.begin(); it != channel->capids.end(); ++it)
		{
			pids.insert((*it)); //all ECM Pids
			printf("CStreamManager::PreparePids: capid 0x%04x \n", (*it));
		}
	}

	//add pcr pid
	if (channel->getPcrPid() && (channel->getPcrPid() != channel->getVideoPid())) {
		pids.insert(channel->getPcrPid());
		printf("CStreamManager::PreparePids: PCRpid 0x%04x \n", channel->getPcrPid());
	}
	//add teletext pid
	if (channel->getTeletextPid() != 0) {
		pids.insert(channel->getTeletextPid());
		printf("CStreamManager::PreparePids: Teletext pid 0x%04x \n", channel->getTeletextPid());
	}
	//add dvb sub pid
	if ((int)channel->getSubtitleCount() > 0) {
		for (int i = 0 ; i < (int)channel->getSubtitleCount() ; ++i) {
			CZapitAbsSub* s = channel->getChannelSub(i);
			if (s->thisSubType == CZapitAbsSub::DVB) {
				CZapitDVBSub* sd = reinterpret_cast<CZapitDVBSub*>(s);
				pids.insert(sd->pId);
				printf("CStreamManager::PreparePids: Subtitle pid 0x%04x \n", sd->pId);
			}
		}
	}

}

/* The setup's steps over this manager, zapit and the hold register. The
   picture, a PiP and a recording are left as they are: the frontend is the
   one zapit held for the stream, or there is none. */
class CStreamManager::SetupOps : public streamsetup::Ops
{
	public:
		explicit SetupOps(CStreamManager &m) : sm(m), uses_ca(true) {}

		/* Whether the box will descramble the stream, an e2 stream only with
		   streaming_decryptmode: read once, told to zapit with the hold and
		   handed to the instance, which starts the CA by it. */
		Read readTarget(int fd, streamsetup::Target &t)
		{
			const Read r = sm.Parse(fd, t);
			uses_ca = !t.e2 || g_settings.streaming_decryptmode;
			return r;
		}
		bool writeOk(int fd) { return streamsetup::writeOk(fd); }
		void refuse(int fd, int status, const char *reason)
		{
			printf("CStreamManager::AddClient: %d %s, fd %d\n", status, reason, fd);
			streamsetup::refuse(fd, status, reason);
		}

		bool runningLocked(uint64_t channel)
		{
			return sm.streams.find(channel) != sm.streams.end();
		}

		void joinLocked(uint64_t channel, int fd)
		{
			printf("CStreamManager::AddClient: channel_id %" PRIx64 " already streaming, just add client %d\n", channel, fd);
			streamsetup::setSendBuffer(fd, SEND_BUFFER_SIZE);
			sm.streams[channel]->AddClient(fd);
		}

		bool recording(uint64_t channel)
		{
			if (!CRecordManager::getInstance()->RecordingStatus(channel))
				return false;
			printf("CStreamManager::AddClient: channel %" PRIx64 " recorded, aborting..\n", channel);
			return true;
		}

		/* zapit's hold. Where none is free and the setting allows it, the GUI
		   is asked to switch the picture to the channel, and the hold is then
		   asked for again: the stream takes the picture's frontend. */
		Hold hold(int fd, uint64_t channel, uint64_t since)
		{
			HoldSteps steps(sm, fd, channel, since, uses_ca);
			const Hold h = streamsetup::holdWithSwitch(steps, kHoldBudgetMs, kHoldsAfterSwitch, kHoldRetryMs);
			if (!h.id)
				printf("CStreamManager::AddClient: no frontend for %" PRIx64 " (verdict %d%s)\n", channel,
				       steps.last.verdict, steps.last.switch_frees ? ", the picture is not switched for it" : "");
			return h;
		}

		void *adopt(uint32_t id) { return streamhold::adopt(id, streamhold::nowMs()); }
		void drop(uint32_t id) { streamhold::drop(id); }

		bool preparePids(const streamsetup::Target &t)
		{
			// zapit deletes a channel's audio tracks under the channel lock
			// when it zaps away from it: the lookup and the copy of the pids
			// hold that lock, and take no other inside it
			CServiceManager::ChannelGuard guard;
			CZapitChannel *channel = CServiceManager::getInstance()->FindChannel(t.channel);
			if (!channel)
				return false;
			sm.PreparePids(channel, pids, t.e2);
			return !pids.empty();
		}

		void *buildLocked(int fd, const streamsetup::Target &t, void *fe, uint32_t hold_id)
		{
			CStreamInstance *stream;
			if (t.kind == streamsetup::Web) {
				stream = new CStreamStream(fd, t.channel, pids);
			} else {
				stream = new CStreamInstance(fd, t.channel, pids);
				stream->frontend = static_cast<CFrontend *>(fe);
				stream->uses_ca = uses_ca;
				stream->hold_id = hold_id;
			}
			streamsetup::setSendBuffer(fd, SEND_BUFFER_SIZE);
			return stream;
		}

		bool startLocked(void *s)
		{
			CStreamInstance *stream = static_cast<CStreamInstance *>(s);
			return stream->Open() && stream->Start();
		}

		void publishLocked(void *s)
		{
			CStreamInstance *stream = static_cast<CStreamInstance *>(s);
			sm.streams.insert(streammap_pair_t(stream->GetChannelId(), stream));
		}

		void discardLocked(void *s)
		{
			delete static_cast<CStreamInstance *>(s);
		}

	private:
		static const unsigned kHoldBudgetMs = 10000;
		/* After a switch: zapit has run the zap before it reads the next
		   command, so the first ask is meant to find the picture's frontend. */
		static const int kHoldsAfterSwitch = 4;
		static const unsigned kHoldRetryMs = 250;
		CStreamManager &sm;
		stream_pids_t pids;
		bool uses_ca;

		/* The steps of a hold over zapit, the GUI and the client. */
		struct HoldSteps : public streamsetup::HoldOps
		{
			CStreamManager &sm;
			int fd;
			uint64_t channel;
			uint64_t since;
			bool uses_ca;
			Answer last;

			HoldSteps(CStreamManager &m, int f, uint64_t c, uint64_t s, bool ca)
				: sm(m), fd(f), channel(c), since(s), uses_ca(ca) {}

			int64_t nowMs() { return CZapitMessages::streamHoldClockMs(); }
			bool eligible() { return CStreamManager::SwitchEligible(channel); }
			bool settingOn() { return g_settings.streaming_switch_picture != 0; }
			/* Bounds the exchange; zapit has what is left of the time to tune. */
			bool ask(int64_t until, bool ask_switch, Answer &a)
			{
				a = Answer();
				const int64_t left = until - nowMs();
				if (left <= 0)
					return false;
				CBasicClient::Deadline deadline((unsigned) left);
				CZapitClient::responseStreamHold reply;
				const bool got = sm.zapit.holdForStream(channel, (unsigned) left, reply, ask_switch, uses_ca);
				if (got) {
					a.verdict = reply.verdict;
					a.hold_id = reply.hold_id;
					a.fenumber = reply.fenumber;
					a.switch_frees = reply.switch_frees != 0;
				}
				last = a;
				return got;
			}
			bool clientGone() { return streamsetup::peerGone(fd); }
			void lockSwitch() { sm.switch_mutex.lock(); }
			void unlockSwitch() { sm.switch_mutex.unlock(); }
			int requestSwitch() { return sm.RequestSwitch(channel, since); }
			void pause(unsigned ms) { usleep(ms * 1000); }
		};
};

bool CStreamManager::SwitchEligible(t_channel_id channel_id)
{
	CNeutrinoApp *app = CNeutrinoApp::getInstance();
	streamsetup::SwitchFacts s;
	const int mode = app->getMode();
	s.tv_mode = mode == NeutrinoModes::mode_tv;
	s.radio_mode = mode == NeutrinoModes::mode_radio;
	{
		// A reload frees the channels under this lock; nothing else is taken inside it.
		CServiceManager::ChannelGuard guard;
		CZapitChannel *channel = CServiceManager::getInstance()->FindChannel(channel_id);
		if (!channel)
			return false;
		s.radio_channel = channel->getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE;
		s.pin_needed = streamsetup::pinNeeded(g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_NEVER,
						      channel->Locked(), g_settings.parentallock_defaultlocked != 0);
		// The picture zapit shows; whether the GUI holds it back is the GUI's to say.
		CZapitChannel *shown =
			CServiceManager::getInstance()->FindChannel(CZapit::getInstance()->GetCurrentChannelID());
		s.shown_pin_needed = !shown ||
				     streamsetup::pinNeeded(g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_NEVER,
							    shown->Locked(), g_settings.parentallock_defaultlocked != 0);
	}
	s.recording = app->recordingstatus != 0 || CRecordManager::getInstance()->RecordingStatus();
	return streamsetup::switchEligible(s);
}

static const unsigned kSwitchClaimMs = 3000;
static const unsigned kSwitchDoneMs = 70000;

int CStreamManager::RequestSwitch(t_channel_id channel_id, uint64_t since)
{
	uint32_t seq = 0;
	{
		// Against a stop that takes the setup back: before this it is seen, after it it cancels.
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		if (!streamsetup::beginSwitchLocked(gate, switch_wait, since, channel_id, seq))
			return streamsetup::NotSwitched;
	}
	SwitchAsk ask;
	ask.channel_id = channel_id;
	ask.seq = seq;
	unsigned char *data = new unsigned char[sizeof(ask)];
	memcpy(data, &ask, sizeof(ask));
	// A message the queue does not take is freed by it.
	if (!g_RCInput->postMsg(NeutrinoMessages::EVT_STREAM_SWITCH, (neutrino_msg_data_t) data, false)) {
		switch_wait.cancelFor(channel_id);
		return streamsetup::NotSwitched;
	}
	/* Claimed within 3 s or never; once claimed, the GUI waits for zapit's zap,
	   which turns no dish and so takes seconds, and its client gives up after 60. */
	const int outcome = switch_wait.wait(seq, kSwitchClaimMs, kSwitchDoneMs);
	printf("CStreamManager::RequestSwitch: %" PRIx64 " outcome %d\n", channel_id, outcome);
	return outcome;
}

streamsetup::Outcome CStreamManager::AddClient(int connfd)
{
	SetupOps ops(*this);
	return streamsetup::run(connfd, gate, ops);
}

void CStreamManager::RemoveClient(int fd)
{
	CStreamInstance *stream_to_stop = NULL;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		for (streammap_iterator_t it = streams.begin(); it != streams.end(); ++it) {
			if (it->second->HasFd(fd)) {
				CStreamInstance *stream = it->second;
				stream->RemoveClient(fd);
				if (stream->GetFdCount() == 0) {
					stream_to_stop = stream;
					streams.erase(stream->GetChannelId());
				}
				break;
			}
		}
	}
	if (stream_to_stop) {
		stream_to_stop->Stop();
		delete stream_to_stop;
	}
}

void CStreamManager::run()
{
	struct sockaddr_in servaddr;
	int clilen = sizeof(servaddr);

	struct pollfd pfd[128];
	int poll_cnt;
	int poll_timeout = -1;

	printf("Starting STREAM thread keeper, tid %ld\n", syscall(__NR_gettid));
	set_threadname("n:streammanager");

	while (running) {
		mutex.lock();
		pfd[0].fd = listenfd;
		pfd[0].events = (POLLIN | POLLPRI);
		pfd[0].revents = 0;
		poll_cnt = 1;
		for (streammap_iterator_t it = streams.begin(); it != streams.end(); ++it) {
			stream_fds_t fds;
			it->second->GetFdsCopy(fds);
			for (stream_fds_t::iterator fit = fds.begin(); fit != fds.end(); ++fit) {
				if (poll_cnt >= (int)(sizeof(pfd) / sizeof(pfd[0])))
					break;
				pfd[poll_cnt].fd = *fit;
				pfd[poll_cnt].events = POLLRDHUP | POLLHUP;
				pfd[poll_cnt].revents = 0;
				poll_cnt++;
			}
			if (poll_cnt >= (int)(sizeof(pfd) / sizeof(pfd[0])))
				break;
		}
		mutex.unlock();
//printf("polling, count= %d\n", poll_cnt);
		int pollres = poll (pfd, poll_cnt, poll_timeout);
		if (pollres <= 0) {
			if (pollres < 0)
				perror("CStreamManager::run(): poll");
			continue;
		}
		for (int i = poll_cnt - 1; i >= 0; i--) {
			if (pfd[i].revents & (POLLIN | POLLPRI | POLLHUP | POLLRDHUP)) {
				printf("fd %d has events %x\n", pfd[i].fd, pfd[i].revents);
				if (pfd[i].fd == listenfd) {
					int connfd = accept(listenfd, (struct sockaddr *) &servaddr, (socklen_t *) & clilen);
					printf("CStreamManager::run(): connection, fd %d\n", connfd);
					if (connfd < 0) {
						perror("CStreamManager::run(): accept");
						continue;
					}

					const streamsetup::Outcome added = AddClient(connfd);
					if (added.result == streamsetup::Refused)
					{
						close(connfd);
					}
					else if (added.published)
					{
						bool first_stream = false;
						mutex.lock();
						first_stream = (streams.size() == 1);
						mutex.unlock();
						if (first_stream)
							g_RCInput->postMsg(NeutrinoMessages::EVT_STREAM_START, 0);
					}
					mutex.lock();
					const bool idle = streamsetup::idleAfter(added, streams.empty(), gate.pendingLocked());
					mutex.unlock();
					/* zapit may have tuned a frontend for a setup that
					   started nothing, and no start was announced for
					   it: the EPG scan learns of it here. */
					if (idle)
						g_RCInput->postMsg(NeutrinoMessages::EVT_STREAM_STOP, added.asked_hold ? 1 : 0);
					poll_timeout = 1000;
				} else {
					if (pfd[i].revents & (POLLHUP | POLLRDHUP)) {
						printf("CStreamManager::run(): POLLHUP, fd %d\n", pfd[i].fd);
						RemoveClient(pfd[i].fd);
						if (streams.empty()) {
							poll_timeout = 2000;
							g_RCInput->postMsg(NeutrinoMessages::EVT_STREAM_STOP, 0);
						}
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
	streammap_t to_stop;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		to_stop.swap(streams);
	}
	bool ret = !to_stop.empty();
	for (streammap_iterator_t it = to_stop.begin(); it != to_stop.end(); ++it) {
		it->second->Stop();
		delete it->second;
	}
	to_stop.clear();
	return ret;
}

bool CStreamManager::StopStream(t_channel_id channel_id)
{
	if (!channel_id)
		return StopAll();

	CStreamInstance *stream_to_stop = NULL;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		gate.cancelChannelLocked(channel_id);
		switch_wait.cancelFor(channel_id);
		streammap_iterator_t it = streams.find(channel_id);
		if (it != streams.end()) {
			stream_to_stop = it->second;
			streams.erase(channel_id);
		}
	}
	if (stream_to_stop) {
		stream_to_stop->Stop();
		delete stream_to_stop;
		return true;
	}
	return false;
}

bool CStreamManager::StopStream(CFrontend * fe)
{
	std::vector<CStreamInstance*> to_stop;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		if (fe)
			gate.cancelFrontendLocked(fe->getNumber());
		switch_wait.cancelPending();
		for (streammap_iterator_t it = streams.begin(); it != streams.end(); ) {
			if (it->second->frontend == fe) {
				to_stop.push_back(it->second);
				streams.erase(it++);
			} else {
				++it;
			}
		}
	}
	for (size_t i = 0; i < to_stop.size(); ++i) {
		to_stop[i]->Stop();
		delete to_stop[i];
	}
	return !to_stop.empty();
}

/* Bit n of fe_mask names frontend number n. Like StopStream, it announces
   nothing: the caller knows what it stopped the streams for. */
bool CStreamManager::StopStreamsOn(uint32_t fe_mask)
{
	std::vector<CStreamInstance*> to_stop;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);
		for (int n = 0; n < 32; n++)
			if (fe_mask & (1u << n))
				gate.cancelFrontendLocked(n);
		switch_wait.cancelPending();
		for (streammap_iterator_t it = streams.begin(); it != streams.end(); ) {
			CFrontend *fe = it->second->frontend;
			const int n = fe ? fe->getNumber() : -1;
			if (n >= 0 && n < 32 && (fe_mask & (1u << n))) {
				to_stop.push_back(it->second);
				streams.erase(it++);
			} else {
				++it;
			}
		}
	}
	for (size_t i = 0; i < to_stop.size(); ++i) {
		to_stop[i]->Stop();
		delete to_stop[i];
	}
	return !to_stop.empty();
}

bool CStreamManager::StreamStatus(t_channel_id channel_id)
{
	bool ret;
	mutex.lock();
	if (channel_id)
		ret = (streams.find(channel_id) != streams.end());
	else
		ret = !streams.empty() || gate.pendingLocked() > 0;
	mutex.unlock();
	return ret;
}

bool CStreamManager::Listen()
{
	struct sockaddr_in socketAddr;
	int socketOptActive = 1;

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

	printf("CStreamManager::Listen: on %d, fd %d\n", port, listenfd);
	return true;
_error:
	close (listenfd);
	return false;
}

CStreamStream::CStreamStream(int clientfd, t_channel_id chid, stream_pids_t &_pids)
	: CStreamInstance(clientfd, chid, _pids), collector(IN_SIZE)
{
	ifcx = NULL;
	ofcx = NULL;
	bsfc = NULL;
	avio_ctx = NULL;
	stopped = true;
	interrupt = false;
}

CStreamStream::~CStreamStream()
{
	Stop();
	Close();
}

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(61, 1, 100)
int CStreamStream::write_packet(void *opaque, const uint8_t *buffer, int buf_size)
#else
int CStreamStream::write_packet(void *opaque, uint8_t *buffer, int buf_size)
#endif
{
	CStreamStream * st = (CStreamStream *) opaque;
	if (buf_size > 0)
		st->collector.feed(buffer, buf_size, *st);
	return buf_size;
}

void CStreamStream::whole(const unsigned char *p, size_t n)
{
	Send(n, p);
}

int CStreamStream::Interrupt(void * data)
{
	CStreamStream * sr = (CStreamStream*) data;
	if (sr->interrupt)
		return 1;
	return 0;
}

void CStreamStream::Close()
{
	if (ifcx)
		avformat_close_input(&ifcx);

	if (ofcx) {
		if (avio_ctx) {
			ofcx->pb = NULL;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 83, 100)
			avio_context_free(&avio_ctx);
#else
			av_freep(&avio_ctx->buffer);
			av_freep(&avio_ctx);
#endif
			buf = NULL;
		}
		avformat_free_context(ofcx);
	} else if (avio_ctx) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 83, 100)
		avio_context_free(&avio_ctx);
#else
		av_freep(&avio_ctx->buffer);
		av_freep(&avio_ctx);
#endif
		buf = NULL;
	}

	if (buf)
		av_freep(&buf);

	if (bsfc){
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 100)
		av_bitstream_filter_close(bsfc);
#else
		av_bsf_free(&bsfc);
#endif
	}

	ifcx = NULL;
	ofcx = NULL;
	bsfc = NULL;
	avio_ctx = NULL;
}

bool CStreamStream::Open()
{
	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);
	if (!channel)
		return false;

	std::string url = channel->getUrl();

	if (url.empty())
		return false;

	std::string pretty_name, livestreamInfo1, livestreamInfo2, headers, dumb;
	if (!CMoviePlayerGui::getInstance(true).getLiveUrl(channel->getUrl(), channel->getScriptName(), url, pretty_name, livestreamInfo1, livestreamInfo2,headers,dumb)) {
		printf("%s: getLiveUrl() [%s] failed!\n", __FUNCTION__, url.c_str());
		return false;
	}

	//av_log_set_level(AV_LOG_VERBOSE);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
	avcodec_register_all();
#endif
	avformat_network_init();

	printf("%s: Open input [%s]....\n", __FUNCTION__, url.c_str());

	AVDictionary *options = NULL;
#if HAVE_LIBSTB_HAL
	if (streaminput_apply_policy(url.c_str(), headers.c_str(), STREAM_PROFILE_RECORD, &options) < 0)
		printf("%s: input policy incomplete, opening with what could be set\n", __FUNCTION__);
#else
	/* No libstb-hal, no shared core: the historic block, value for value. */
	if (!headers.empty())
		av_dict_set(&options, "headers", headers.c_str(), 0);

	if (strncmp(url.c_str(), "http://", 7) == 0 || strncmp(url.c_str(), "https://", 8) == 0)
	{
		av_dict_set(&options, "timeout", "20000000", 0); //20sec
		av_dict_set(&options, "reconnect", "1", 0);
	}
#endif

	if (avformat_open_input(&ifcx, url.c_str(), NULL, &options) != 0) {
		printf("%s: Cannot open input [%s]!\n", __FUNCTION__, channel->getUrl().c_str());
			av_dict_free(&options);
		return false;
	}
	av_dict_free(&options);

	if (avformat_find_stream_info(ifcx, NULL) < 0) {
		printf("%s: Cannot find stream info [%s]!\n", __FUNCTION__, channel->getUrl().c_str());
		avformat_close_input(&ifcx);
		return false;
	}
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 27, 102)
	const char *hls = "applehttp";
#else
	const char *hls = "hls";
#endif
	if (!strstr(ifcx->iformat->name, hls) &&
			!strstr(ifcx->iformat->name, "mpegts") &&
			!strstr(ifcx->iformat->name, "matroska") &&
			!strstr(ifcx->iformat->name, "avi") &&
			!strstr(ifcx->iformat->name, "mp4")) {
		printf("%s: not supported format [%s]!\n", __FUNCTION__, ifcx->iformat->name);
		avformat_close_input(&ifcx);
		return false;
	}

	AVIOInterruptCB int_cb = { Interrupt, this };
	ifcx->interrupt_callback = int_cb;

#if (LIBAVFORMAT_VERSION_MAJOR < 58)
	snprintf(ifcx->filename, sizeof(ifcx->filename), "%s", channel->getUrl().c_str());
	av_dump_format(ifcx, 0, ifcx->filename, 0);
#else
	av_dump_format(ifcx, 0, ifcx->url, 0);
#endif

	buf = (unsigned char *) av_malloc(IN_SIZE);
	if (buf == NULL) {
		perror("CStreamStream::Open: buf");
		return false;
	}
	avio_ctx = avio_alloc_context(buf, IN_SIZE, 1, this, NULL, &write_packet, NULL);
	if (!avio_ctx) {
		printf("%s: avio_alloc_context failed\n", __FUNCTION__);
		av_freep(&buf);
		avformat_close_input(&ifcx);
		return false;
	}

	if (avformat_alloc_output_context2(&ofcx, NULL, "mpegts", NULL) < 0) {
		printf("%s: avformat_alloc_output_context2 failed\n", __FUNCTION__);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 83, 100)
		avio_context_free(&avio_ctx);
#else
		av_freep(&avio_ctx->buffer);
		av_freep(&avio_ctx);
#endif
		buf = NULL;
		avformat_close_input(&ifcx);
		return false;
	}
	ofcx->pb = avio_ctx;

	av_dict_copy(&ofcx->metadata, ifcx->metadata, 0);
	int stid = 0x200;
	for (unsigned i = 0; i < ifcx->nb_streams; i++) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(57, 25, 101)
		AVCodecContext * iccx = ifcx->streams[i]->codec;
		AVStream *ost = avformat_new_stream(ofcx, iccx->codec);
		avcodec_copy_context(ost->codec, iccx);
#else
		AVCodecParameters * iccx = ifcx->streams[i]->codecpar;
		AVStream *ost = avformat_new_stream(ofcx, NULL);
		avcodec_parameters_copy(ost->codecpar, iccx);
#endif
		av_dict_copy(&ost->metadata, ifcx->streams[i]->metadata, 0);
		ost->time_base = ifcx->streams[i]->time_base;
		ost->id = stid++;
	}
	av_log_set_level(AV_LOG_VERBOSE);
#if (LIBAVFORMAT_VERSION_MAJOR < 58)
	av_dump_format(ofcx, 0, ofcx->filename, 1);
#else
	av_dump_format(ofcx, 0, ofcx->url, 1);
#endif
	av_log_set_level(AV_LOG_WARNING);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 100)
	bsfc = av_bitstream_filter_init("h264_mp4toannexb");
	if (!bsfc)
		printf("%s: av_bitstream_filter_init h264_mp4toannexb failed!\n", __FUNCTION__);
#else
	const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
	if(!bsf) {
		Close();
		return false;
	}
	if ((av_bsf_alloc(bsf, &bsfc))) {
		Close();
		return false;
	}
#endif
	return true;
}

bool CStreamStream::Start()
{
	if (!stopped)
		return false;

	printf("%s: Starting...\n", __FUNCTION__);
	stopped = false;
	int ret = start();
	if (ret != 0)
		stopped = true;
	return (ret == 0);
}

bool CStreamStream::Stop()
{
	if (stopped)
		return false;

	printf("%s: Stopping...\n", __FUNCTION__);
	interrupt = true;
	stopped = true;
	int ret = join();
	interrupt = false;
	return (ret == 0);
}
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(58, 133, 100)
static void get_packet_defaults(AVPacket *pkt)
{
    memset(pkt, 0, sizeof(*pkt));

    pkt->pts             = AV_NOPTS_VALUE;
    pkt->dts             = AV_NOPTS_VALUE;
    pkt->pos             = -1;
}
#endif

void CStreamStream::run()
{
	AVPacket pkt;

	printf("%s: Started.\n", __FUNCTION__);
	if (avformat_write_header(ofcx, NULL) < 0) {
		printf("%s: avformat_write_header failed\n", __FUNCTION__);
		return;
	}

	while (!stopped) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
		av_init_packet(&pkt);
#else
		get_packet_defaults(&pkt);
#endif
		if (av_read_frame(ifcx, &pkt) < 0)
			break;
		if (pkt.stream_index < 0)
			continue;

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(57, 25, 101)
		AVCodecContext *codec = ifcx->streams[pkt.stream_index]->codec;
#else
		AVCodecParameters *codec = ifcx->streams[pkt.stream_index]->codecpar;
#endif
		if (bsfc && codec->codec_id == AV_CODEC_ID_H264 ) {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
			AVPacket newpkt;
			av_init_packet(&newpkt);
#else
			AVPacket newpkt;
			get_packet_defaults(&newpkt);
#endif
			newpkt.data = NULL;
			newpkt.size = 0;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 100)
			if (av_bitstream_filter_filter(bsfc, codec, NULL, &newpkt.data, &newpkt.size, pkt.data, pkt.size, pkt.flags & AV_PKT_FLAG_KEY) >= 0) {
				av_packet_unref(&pkt);
				newpkt.buf = av_buffer_create(newpkt.data, newpkt.size, av_buffer_default_free, NULL, 0);
				pkt = newpkt;
			}
#else
			int ret = av_bsf_send_packet(bsfc, &pkt);
			if (ret < 0){
				break;
			}
			ret = av_bsf_receive_packet(bsfc, &newpkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
				continue;
			}
			if (ret < 0){
				av_packet_unref(&pkt);
				break;
			}
			av_packet_unref(&pkt);
			pkt = newpkt;
#endif
		}
		pkt.pts = av_rescale_q(pkt.pts, ifcx->streams[pkt.stream_index]->time_base, ofcx->streams[pkt.stream_index]->time_base);
		pkt.dts = av_rescale_q(pkt.dts, ifcx->streams[pkt.stream_index]->time_base, ofcx->streams[pkt.stream_index]->time_base);

		av_write_frame(ofcx, &pkt);
		av_packet_unref(&pkt);
	}

	av_read_pause(ifcx);
	av_write_trailer(ofcx);
	printf("%s: Stopped.\n", __FUNCTION__);
}
