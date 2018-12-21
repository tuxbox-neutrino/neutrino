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

extern "C" {
#include <libavformat/avformat.h>
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
		stream_pids_t pids;
		stream_fds_t fds;

		virtual bool Send(ssize_t r, unsigned char * _buf = NULL);
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
		t_channel_id GetChannelId() { return channel_id; }
};

class CStreamStream : public CStreamInstance
{
	private:
		AVFormatContext *ifcx;
		AVFormatContext *ofcx;
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT( 57,52,100 ))
		AVBitStreamFilterContext *bsfc;
#else
		AVBSFContext *bsfc;
#endif
		AVIOContext *avio_ctx;

		bool stopped;
		bool interrupt;
		void run();
		void Close();

	public:
		CStreamStream(int clientfd, t_channel_id chid, stream_pids_t &pids);
		~CStreamStream();

		bool Open();
		bool Start();
		bool Stop();

		static int Interrupt(void * data);
		static int write_packet(void *opaque, uint8_t *buf, int buf_size);
};

typedef std::pair<t_channel_id, CStreamInstance*> streammap_pair_t;
typedef std::map<t_channel_id, CStreamInstance*> streammap_t;
typedef streammap_t::iterator streammap_iterator_t;

class CStreamManager : public OpenThreads::Thread
{
	private:
		bool	enabled;
		bool	running;
		int	listenfd;
		int	port;

		OpenThreads::Mutex mutex;
		static	CStreamManager * sm;
		CZapitClient zapit;

		streammap_t streams;

		bool	Listen();
		bool	Parse(int fd, stream_pids_t &pids, t_channel_id &chid, CFrontend * &frontend);
		void	AddPids(int fd, CZapitChannel * channel, stream_pids_t &pids);
		void	CheckStandby(bool enter);
		CFrontend * FindFrontend(CZapitChannel * channel);
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
		bool StreamStatus(t_channel_id channel_id = 0);
		bool SetPort(int newport);
		int GetPort() { return port; }
		bool AddClient(int fd);
};
#endif
