/*
 * $Id: channel.h,v 1.26 2003/10/14 12:48:57 thegoodguy Exp $
 *
 * (C) 2002 Steffen Hehn <mcclean@berlios.de>
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __zapit_channel_h__
#define __zapit_channel_h__

/* system */
#include <string>
#include <inttypes.h>
#include <cstdlib>
#include <xmltree/xmlinterface.h>
#include <sectionsdclient/sectionsdclient.h>
#include <set>

/* zapit */
#include "types.h"
//#include <zapit/audio.h>

typedef struct audio_map_set {
        unsigned short apid;
        int mode;
        unsigned int volume;
        int subpid;
	int ttxpid;
	int ttxpage;
} audio_map_set_t;

/* subtitling support */
class CZapitAbsSub
{
 public:
    unsigned short pId;
    std::string ISO639_language_code;
    enum ZapitSubtitleType {
        TTX,
        DVB
    };
    ZapitSubtitleType thisSubType;
};

class CZapitDVBSub:public CZapitAbsSub {
	public:
		unsigned short subtitling_type;
		/*
		 * possible values:
		 * 0x01 EBU Teletex subtitles
		 * 0x10 DVB subtitles (normal) with no monitor aspect ratio criticality
		 * 0x11 DVB subtitles (normal) for display on 4:3 aspect ratio monitor
		 * 0x12 DVB subtitles (normal) for display on 16:9 aspect ratio monitor
		 * 0x13 DVB subtitles (normal) for display on 2.21:1 aspect ratio monitor
		 * 0x20 DVB subtitles (for the hard of hearing) with no monitor aspect ratio criticality
		 * 0x21 DVB subtitles (for the hard of hearing) for display on 4:3 aspect ratio monitor
		 * 0x22 DVB subtitles (for the hard of hearing) for display on 16:9 aspect ratio monitor
		 * 0x23 DVB subtitles (for the hard of hearing) for display on 2.21:1 aspect ratio monitor
		 */
		unsigned int composition_page_id;
		unsigned int ancillary_page_id;

		CZapitDVBSub(){thisSubType=DVB;};
};

class CZapitTTXSub:public CZapitAbsSub
{
public:
    unsigned short teletext_magazine_number;
    unsigned short teletext_page_number; // <- the actual important stuff here
    bool hearingImpaired;

    CZapitTTXSub(){thisSubType=TTX;};
};

class CZapitAudioChannel
{
	public:
		unsigned short		pid;
		std::string		description;
		unsigned char		componentTag;

	enum ZapitAudioChannelType {
		MPEG,
		AC3,
		AAC,
		AACPLUS, //?
		DTS,
		UNKNOWN
	};
	ZapitAudioChannelType audioChannelType;
};

class CChannelList;

typedef std::set<int> casys_map_t;
typedef casys_map_t::iterator casys_map_iterator_t;

class CZapitChannel
{
	private:
		/* channel name */
		std::string name;

		/* pids of this channel */
		std::vector <CZapitAbsSub* > channelSubs;
		std::vector <CZapitAudioChannel *> audioChannels;

		unsigned short			pcrPid;
		unsigned short			pmtPid;
		unsigned short			teletextPid;
		unsigned short			videoPid;
		unsigned short			audioPid;
		/*unsigned short			privatePid;*/

		/* set true when pids are set up */
		bool				pidsFlag;

		/* last selected audio channel */
		unsigned char			currentAudioChannel;

		/* chosen subtitle stream */
		unsigned char                   currentSub;

		/* read only properties, set by constructor */
		t_service_id			service_id;
		t_transport_stream_id		transport_stream_id;
		t_original_network_id		original_network_id;
		t_satellite_position		satellitePosition;
		freq_id_t			freq;

		/* read/write properties (write possibility needed by scan) */
		unsigned char			serviceType;

		/* the conditional access program map table of this channel */
		//CCaPmt * 			caPmt;
		unsigned char			* rawPmt;
		int				pmtLen;
		uint8_t				pmt_version;

		/* from neutrino CChannel class */
		uint64_t			last_unlocked_EPGid;
		time_t				last_unlocked_time;

		std::string			ttx_language_code;

		uint8_t				record_demux;
		uint8_t				pip_demux;

		void				Init();
		friend class CChannelList;

	public:
		typedef enum channel_flags {
			NEW		= 0x01,
			REMOVED		= 0x02,
			UPDATED		= 0x04,
			NOT_FOUND	= 0x08,
			PRESENT		= 0x05, // NEW + UPDATED
			ANY		= 0xFF
		} channel_flags_t;
		casys_map_t			camap;

		bool				bAlwaysLocked;

		int				number;
		CChannelEvent			currentEvent,nextEvent;
		int				type;
		t_channel_id			channel_id;
		unsigned char			scrambled;
		char *				pname;
		bool				has_bouquet;
		uint8_t				polarization;
		int				flags;
		int				deltype;

		/* constructor, desctructor */
		CZapitChannel(const std::string & p_name, t_service_id p_sid, t_transport_stream_id p_tsid, t_original_network_id p_onid, unsigned char p_service_type, t_satellite_position p_satellite_position, freq_id_t freq);
		CZapitChannel(const std::string & p_name, t_channel_id p_channel_id, unsigned char p_service_type, t_satellite_position p_satellite_position, freq_id_t p_freq);
		~CZapitChannel(void);

		/* get methods - read only variables */
		t_service_id		getServiceId(void)         	const { return service_id; }
		t_transport_stream_id	getTransportStreamId(void) 	const { return transport_stream_id; }
		t_original_network_id	getOriginalNetworkId(void) 	const { return original_network_id; }
		unsigned char        	getServiceType(bool real=false);
		bool			isHD();
		t_channel_id         	getChannelID(void)         	const { return channel_id; }
		transponder_id_t        getTransponderId(void)          const { return CREATE_TRANSPONDER_ID64(freq, satellitePosition,original_network_id,transport_stream_id); }
		freq_id_t		getFreqId()			const { return freq; }


		/* get methods - read and write variables */
		const std::string&	getName(void)			const { return name; }
		t_satellite_position	getSatellitePosition(void)	const { return satellitePosition; }
		unsigned char 		getAudioChannelCount(void)	{ return audioChannels.size(); }
		unsigned short		getPcrPid(void)			{ return pcrPid; }
		unsigned short		getPmtPid(void)			{ return pmtPid; }
		unsigned short		getTeletextPid(void)		{ return teletextPid; }
		const char *		getTeletextLang(void)		{ return ttx_language_code.c_str(); }
		unsigned short		getVideoPid(void)		{ return videoPid; }
		/*unsigned short		getPrivatePid(void)		{ return privatePid; }*/
		unsigned short		getPreAudioPid(void)		{ return audioPid; }
		bool			getPidsFlag(void)		{ return pidsFlag; }
		//CCaPmt *		getCaPmt(void)			{ return caPmt; }
		unsigned char *		getRawPmt(int &len)		{ len = pmtLen; return rawPmt; };
		uint8_t			getPmtVersion(void)		{ return pmt_version; };

		CZapitAudioChannel * 	getAudioChannel(unsigned char index = 0xFF);
		unsigned short 		getAudioPid(unsigned char index = 0xFF);
		unsigned char  		getAudioChannelIndex(void)	{ return currentAudioChannel; }

		int addAudioChannel(const unsigned short pid, const CZapitAudioChannel::ZapitAudioChannelType audioChannelType, const std::string & description, const unsigned char componentTag);

		/* set methods */
		void setServiceType(const unsigned char pserviceType)	{ serviceType = pserviceType; }
		inline void setName(const std::string pName)            { name = pName; }
		void setAudioChannel(unsigned char pAudioChannel)	{ if (pAudioChannel < audioChannels.size()) currentAudioChannel = pAudioChannel; }
		void setPcrPid(unsigned short pPcrPid)			{ pcrPid = pPcrPid; }
		void setPmtPid(unsigned short pPmtPid)			{ pmtPid = pPmtPid; }
		void setTeletextPid(unsigned short pTeletextPid)	{ teletextPid = pTeletextPid; }
		void setTeletextLang(std::string lang)			{ ttx_language_code = lang; };
		void setVideoPid(unsigned short pVideoPid)		{ videoPid = pVideoPid; }
		void setAudioPid(unsigned short pAudioPid)		{ audioPid = pAudioPid; }
		/*void setPrivatePid(unsigned short pPrivatePid)		{ privatePid = pPrivatePid; }*/
		void setPidsFlag(void)					{ pidsFlag = true; }
		//void setCaPmt(CCaPmt *pCaPmt);
		void setPmtVersion(uint8_t version)			{ pmt_version = version; }
		void setRawPmt(unsigned char * pmt, int len = 0);
		/* cleanup methods */
		void resetPids(void);
		/* subtitling related methods */
		void addTTXSubtitle(const unsigned int pid, const std::string langCode, const unsigned char magazine_number, const unsigned char page_number, const bool impaired=false);

		void addDVBSubtitle(const unsigned int pid, const std::string langCode, const unsigned char subtitling_type, const unsigned short composition_page_id, const unsigned short ancillary_page_id);

		unsigned getSubtitleCount() const { return channelSubs.size(); };
		CZapitAbsSub* getChannelSub(int index = -1);
		int getChannelSubIndex(void);
		void setChannelSub(int subIdx);

		void dumpServiceXml(FILE * fd, const char * action = NULL);
		void dumpBouquetXml(FILE * fd);
		void setRecordDemux(int num) { record_demux = num; };
		void setPipDemux(int num) { pip_demux = num; };
		int  getRecordDemux() { return record_demux; };
		int  getPipDemux() { return pip_demux; };
		static t_channel_id makeChannelId(t_satellite_position sat, freq_id_t freq,
				t_transport_stream_id tsid, t_original_network_id onid, t_service_id sid)
		{
			return (((uint64_t)(sat+freq*4) << 48) | ((uint64_t) tsid << 32) | ((uint64_t)onid << 16) | (uint64_t)sid);
		};
};

struct CmpChannelBySat: public std::binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
{
	static bool comparetolower(const char a, const char b)
	{
		return tolower(a) < tolower(b);
	};

	bool operator() (const CZapitChannel * const c1, const CZapitChannel * const c2)
	{
		if(c1->getSatellitePosition() == c2->getSatellitePosition())
			return std::lexicographical_compare(c1->getName().begin(), c1->getName().end(), c2->getName().begin(), c2->getName().end(), comparetolower);
		else
			return c1->getSatellitePosition() < c2->getSatellitePosition();
		;
	};
};

struct CmpChannelByFreq: public std::binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
{
	static bool comparetolower(const char a, const char b)
	{
		return tolower(a) < tolower(b);
	};

	bool operator() (const CZapitChannel * const c1, const CZapitChannel * const c2)
	{
		if(c1->getFreqId() == c2->getFreqId())
			return std::lexicographical_compare(c1->getName().begin(), c1->getName().end(), c2->getName().begin(), c2->getName().end(), comparetolower);
		else
			return c1->getFreqId() < c2->getFreqId();
		;
	};
};

struct CmpChannelByChName: public std::binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
{
	static bool comparetolower(const char a, const char b)
	{
		return tolower(a) < tolower(b);
	};

	bool operator() (const CZapitChannel * const c1, const CZapitChannel * const c2)
	{
		return std::lexicographical_compare(c1->getName().begin(), c1->getName().end(), c2->getName().begin(), c2->getName().end(), comparetolower);
	};
};

struct CmpChannelByChNum: public std::binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
{
	bool operator() (const CZapitChannel * const c1, const CZapitChannel * const c2)
	{
		return c1->number < c2->number;
		;
	};
};


#endif /* __zapit_channel_h__ */
