/*
 * $Id: bouquets.h,v 1.57 2004/08/02 08:13:45 thegoodguy Exp $
 */

#ifndef __bouquets_h__
#define __bouquets_h__

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <vector>
#include <string.h>
#include <ctype.h>

#include <inttypes.h>
#include <zapit/client/zapitclient.h>

#include "channel.h"
#include "xmlinterface.h"

using namespace std;

typedef map<t_channel_id, CZapitChannel> tallchans;
typedef tallchans::iterator tallchans_iterator;

typedef vector<CZapitChannel*> ZapitChannelList;

class CZapitBouquet
{
 public:
	std::string Name;
	bool        bHidden;
	bool        bLocked;
	bool        bUser;
	bool        bFav;
	t_satellite_position satellitePosition;

	ZapitChannelList radioChannels;
	ZapitChannelList tvChannels;

	inline CZapitBouquet(const std::string name) { Name = name; bHidden = false; bLocked = false; bUser = true; }

	void addService(CZapitChannel* newChannel);

	void removeService(CZapitChannel* oldChannel);
	void removeService(const t_channel_id channel_id, unsigned char serviceType = ST_RESERVED) { removeService(getChannelByChannelID(channel_id, serviceType)); }
	
	void moveService (const unsigned int oldPosition, const unsigned int newPosition, const unsigned char serviceType);
	
	size_t recModeRadioSize(const transponder_id_t transponder_id);
	size_t recModeTVSize   (const transponder_id_t transponder_id);
	CZapitChannel* getChannelByChannelID(const t_channel_id channel_id, const unsigned char serviceType = ST_RESERVED);
	void sortBouquet(void);
};

typedef vector<CZapitBouquet *> BouquetList;

class CBouquetManager
{
 private:
	CZapitBouquet * remainChannels;

	void makeRemainingChannelsBouquet(void);
	void parseBouquetsXml            (const xmlNodePtr root, bool ub = false);
	void writeBouquetHeader          (FILE * bouq_fd, uint32_t i, const char * bouquetName);
	void writeBouquetFooter          (FILE * bouq_fd);
	void writeBouquetChannels        (FILE * bouq_fd, uint32_t i, bool bUser = false);

 public:
		CBouquetManager() { remainChannels = NULL; };
		class ChannelIterator
		{
			private:
				CBouquetManager* Owner;
				bool tv;           // true -> tvChannelIterator, false -> radioChannelIterator
				unsigned int b;
				int c;
				ZapitChannelList* getBouquet() { return (tv ? &(Owner->Bouquets[b]->tvChannels) : &(Owner->Bouquets[b]->radioChannels)); };
			public:
				ChannelIterator(CBouquetManager* owner, const bool TV = true);
				ChannelIterator operator ++(int);
				CZapitChannel* operator *();
				ChannelIterator FindChannelNr(const unsigned int channel);
				int getLowestChannelNumberWithChannelID(const t_channel_id channel_id);
				int getNrofFirstChannelofBouquet(const unsigned int bouquet_nr);
				bool EndOfChannels() { return (c == -2); };
		};

		ChannelIterator tvChannelsBegin() { return ChannelIterator(this, true); };
		ChannelIterator radioChannelsBegin() { return ChannelIterator(this, false); };

		BouquetList Bouquets;

		void saveBouquets(void);
		void saveUBouquets(void);
		void saveBouquets(const CZapitClient::bouquetMode bouquetMode, const char * const providerName);
		void loadBouquets(bool ignoreBouquetFile = false);
		void renumServices();

		CZapitBouquet* addBouquet(const std::string & name, bool ub = false, bool myfav = false);
		void deleteBouquet(const unsigned int id);
		void deleteBouquet(const CZapitBouquet* bouquet);
		int  existsBouquet(char const * const name);
		int  existsUBouquet(char const * const name, bool myfav = false);
		void moveBouquet(const unsigned int oldId, const unsigned int newId);
		bool existsChannelInBouquet(unsigned int bq_id, const t_channel_id channel_id);

		void clearAll();

		CZapitChannel* findChannelByChannelID(const t_channel_id channel_id);
		void sortBouquets(void);

};

/*
 * Struct for channel comparison by channel names 
 *
 * TODO:
 * Channel names are not US-ASCII, but UTF-8 encoded.
 * Hence we need a compare function that considers the whole unicode charset.
 * For instance all countless variants of the letter a have to be regarded as the same letter.
 */
struct CmpChannelByChName: public binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
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

struct CmpBouquetByChName: public binary_function <const CZapitBouquet * const, const CZapitBouquet * const, bool>
{
	static bool comparetolower(const char a, const char b)
		{
			return tolower(a) < tolower(b);
		};

	bool operator() (const CZapitBouquet * const c1, const CZapitBouquet * const c2)
		{
			return std::lexicographical_compare(c1->Name.begin(), c1->Name.end(), c2->Name.begin(), c2->Name.end(), comparetolower);
			//return strcasecmp(c1->Name.c_str(), c2->Name.c_str());
		};
};

#endif /* __bouquets_h__ */
