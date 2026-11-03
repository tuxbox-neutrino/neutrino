/*
        Neutrino-GUI  -   DBoxII-Project

        Copyright (C) 2011-2012 CoolStream International Ltd

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

#ifndef __streamts_h__
#define __streamts_h__

#include <OpenThreads/Thread>
#include <OpenThreads/Condition>

#include <hardware/dmx.h>
#include <zapit/client/zapittypes.h>
#include <zapit/femanager.h>
#include <set>
#include <map>
#include <atomic>

#include <driver/streamsetup.h>
#include <driver/tsgrid.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 0, 100)
#include <libavcodec/bsf.h>
#endif
}

typedef std::set<int> stream_pids_t;
typedef std::set<int> stream_fds_t;

class CStreamInstance : public OpenThreads::Thread
{
	protected:
		bool	running;
		cDemux * dmx;
		CFrontend * frontend;
		OpenThreads::Mutex mutex;
		unsigned char * buf;

		t_channel_id channel_id;
		/* The hold zapit keeps the frontend under for this stream, dropped
		   when the stream is deleted; 0 for a web channel. */
		uint32_t hold_id;
		stream_pids_t pids;
		stream_fds_t fds;
		/* What each client in fds is still owed, under mutex. */
		tsgrid::Clients clients;
		/* The part of a packet the last read of buf ended in. */
		tsgrid::Collector grid;
		/* r bytes of whole packets, at _buf or else at buf. */
		virtual bool Send(ssize_t r, const unsigned char * _buf = NULL);
		virtual void Close();
		virtual void run();
		friend class CStreamManager;
	public:
		CStreamInstance(int clientfd, t_channel_id chid, stream_pids_t &pids);
		virtual ~CStreamInstance();
		virtual bool Open();
		virtual bool Start();
		virtual bool Stop();
		void AddClient(int clientfd);
		void RemoveClient(int clientfd);
		bool HasFd(int fd);
		stream_fds_t & GetFds() { return fds; }
		void GetFdsCopy(stream_fds_t &out);
		size_t GetFdCount();
		t_channel_id GetChannelId() { return channel_id; }
		bool is_e2_stream;
};

class CStreamStream : public CStreamInstance, private tsgrid::Out
{
	private:
		/* ffmpeg's muxer writes into buf, its own buffer: the part of a
		   packet left over is kept here and not in front of buf. */
		tsgrid::CopyCollector collector;
		AVFormatContext *ifcx;
		AVFormatContext *ofcx;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 100)
		AVBitStreamFilterContext *bsfc;
#else
		AVBSFContext *bsfc;
#endif
		AVIOContext *avio_ctx;

		bool stopped;
		bool interrupt;
		void run();
		void Close();
		void whole(const unsigned char *p, size_t n);

	public:
		CStreamStream(int clientfd, t_channel_id chid, stream_pids_t &pids);
		~CStreamStream();

		bool Open();
		bool Start();
		bool Stop();

		static int Interrupt(void * data);
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(61, 1, 100)
		static int write_packet(void *opaque, const uint8_t *buffer, int buf_size);
#else
		static int write_packet(void *opaque, uint8_t *buffer, int buf_size);
#endif
};

typedef std::pair<t_channel_id, CStreamInstance*> streammap_pair_t;
typedef std::map<t_channel_id, CStreamInstance*> streammap_t;
typedef streammap_t::iterator streammap_iterator_t;

class CStreamManager : public OpenThreads::Thread
{
	private:
		bool	enabled;
		std::atomic<bool> running;
		int	listenfd;
		int	port;

		OpenThreads::Mutex mutex;
		static	CStreamManager * sm;
		CZapitClient zapit;

		streammap_t streams;
		/* Counts the setups between accept and the streams map, under
		 * mutex. StreamStatus treats them as live so that a standby
		 * triggered by the previous stream's stop does not take the box
		 * down while the next stream is mid-setup. */
		streamsetup::StartGate gate;

		class SetupOps;
		bool	Listen();
		streamsetup::Ops::Read Parse(int fd, streamsetup::Target &target);
		void	PreparePids(CZapitChannel * channel, stream_pids_t &pids, bool is_e2);
		void	CheckStandby(bool enter);
		bool	StopAll();
		void	RemoveClient(int fd);
		void	run();
		CStreamManager();
	public:
		~CStreamManager();
		static CStreamManager * getInstance();
		bool Start(int port = 0);
		bool Stop();
		bool StopStream(t_channel_id channel_id = 0);
		bool StopStream(CFrontend * fe);
		/* Stops the streams on every frontend whose number has its bit set
		   in fe_mask, and keeps a stream being set up on one of them from
		   being published. */
		bool StopStreamsOn(uint32_t fe_mask);
		bool StreamStatus(t_channel_id channel_id = 0);
		bool SetPort(int newport);
		int GetPort() { return port; }
		/* Takes a client the accept loop has: Refused leaves the
		   descriptor to the caller, Consumed means a stream has it. */
		streamsetup::Outcome AddClient(int fd);
};
#endif
