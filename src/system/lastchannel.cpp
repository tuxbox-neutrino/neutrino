/*
DBOX2 -- Projekt
 
(c) 2001 rasc Lizenz: GPL 
 
Lastchannel History buffer
 
Einfache Klasse fuer schnelles Zappen zum letzten Kanal.
Ggf. laesst sich damit ein kleines ChannelHistory-Menue aufbauen-
 
Das ganze ist als sich selbst ueberschreibender Ringpuffer realisiert,
welcher nach dem LIFO-prinzip wieder ausgelesen wird.
Es wird aber gecheckt, ob ein Neuer Wert ein Mindestzeitabstand zum alten
vorherigen Wert hat, damit bei schnellem Hochzappen, die "Skipped Channels"
nicht gespeichert werden.
*/

#include <sys/time.h>
#include <unistd.h>

#include <global.h>
#include <neutrino.h>
#include "lastchannel.h"

CLastChannel::CLastChannel (void)
: secs_diff_before_store(3)
, maxSize(11)
, shallRemoveEqualChannel(true)
{
}

// // -- Clear the last channel buffer //
void CLastChannel::clear (void)
{
  this->lastChannels.clear();
}

// -- Store a channelnumber in Buffer
// -- Store only if channel != last channel and time store delay is large enough
// forceStoreToLastChannels default to false

void CLastChannel::store (int channel, t_channel_id channel_id, bool /* forceStoreToLastChannels */)
{
	struct timeval  tv;
	unsigned long lastTimestamp(0);
	unsigned long timeDiff;
	std::list<_LastCh>::iterator It;

	gettimeofday (&tv, NULL);

	if (!this->lastChannels.empty())
		lastTimestamp  = this->lastChannels.front().timestamp;

	timeDiff = tv.tv_sec - lastTimestamp;

	/* prev zap time was less than treshhold, remove prev channel */
	if(!this->lastChannels.empty() && (timeDiff <= secs_diff_before_store))
		this->lastChannels.pop_front();

	/* push new channel to the head */
	_LastCh newChannel = {channel, channel_id, tv.tv_sec, CNeutrinoApp::getInstance()->GetChannelMode()};

	this->lastChannels.push_front(newChannel);

	/* this zap time was more than treshhold, it will stay, remove last in the list */
	if ((timeDiff > secs_diff_before_store) && (this->lastChannels.size() > this->maxSize))
		this->lastChannels.pop_back();

	/* remove this channel at other than 0 position */
	if(this->lastChannels.size() > 1) {
		It = this->lastChannels.begin();
		It++;
		for (; It != this->lastChannels.end() ; It++) {
			if (channel_id == It->channel_id) {
				this->lastChannels.erase(It);
				break;
			}
		}
	}
}

unsigned int CLastChannel::size () const
{
  return this->lastChannels.size();
}

// -- Clear store time delay
// -- means: set last time stamp to zero
// -- means: store next channel with "store" always

void CLastChannel::clear_storedelay (void)

{
  if (!this->lastChannels.empty())
  {
    this->lastChannels.front().timestamp = 0;
  }
}

// -- Get last Channel-Entry
// -- IN:   n number of last channel in queue [0..]
// --       0 = current channel
// -- Return:  channelnumber or <0  (end of list)

t_channel_id CLastChannel::getlast (int n)
{
	if((n < int(this->lastChannels.size())) && (n > -1) && (!this->lastChannels.empty()))
	{
		std::list<_LastCh>::const_iterator It = this->lastChannels.begin();
		std::advance(It, n);

		return It->channel_id;
	}

	return 0;
}

// -- set delaytime in secs, for accepting a new value
// -- get returns the value

void CLastChannel::set_store_difftime (int secs)
{
	secs_diff_before_store = secs;
}

int CLastChannel::get_store_difftime (void) const

{
	return    secs_diff_before_store;
}

int CLastChannel::get_mode(t_channel_id channel_id)
{
	std::list<_LastCh>::iterator It;

	for (It = this->lastChannels.begin(); It != this->lastChannels.end() ; ++It) {
		if (channel_id == It->channel_id)
			return It->channel_mode;
	}
	return -1;
}

bool CLastChannel::set_mode(t_channel_id channel_id)
{
	std::list<_LastCh>::iterator It;

	for (It = this->lastChannels.begin(); It != this->lastChannels.end() ; ++It) {
		if (channel_id == It->channel_id) {
			It->channel_mode = CNeutrinoApp::getInstance()->GetChannelMode();
			return true;
		}
	}
	return false;
}
