/*
DBoX2 -- Projekt
 
(c) 2001 rasc
 
Lizenz: GPL 
 
 
Lastchannel History buffer
 
Einfache Klasse fuer schnelles Zappen zum letzten Kanal.
Ggf. laesst sich damit ein kleines ChannelHistory-Menue aufbauen-
 
Das ganze ist als sich selbst ueberschreibender Ringpuffer realisiert,
welcher nach dem LIFO-prinzip wieder ausgelesen wird.
Es wird aber gecheckt, ob ein Neuer Wert ein Mindestzeitabstand zum alten
vorherigen Wert hat, damit bei schnellem Hochzappen, die "Skipped Channels"
nicht gespeichert werden.
 
*/


#ifndef SEEN_LastChannel
#define SEEN_LastChannel

#include <zapit/types.h>
#include <list>

class CLastChannel
{

	private:
		struct _LastCh
		{
			t_channel_id channel_id;
			long int   timestamp;
			int channel_mode;
		};

		std::list<_LastCh> lastChannels;

		unsigned long  secs_diff_before_store;
		unsigned int maxSize;
		bool shallRemoveEqualChannel;

	public:
		CLastChannel  (void);
		void clear   (void);
		void store   (t_channel_id channel_id);
		t_channel_id getlast (int n);
		unsigned int size () const;
		void clear_storedelay (void);
		void set_store_difftime (int secs);
		int  get_store_difftime (void) const;
		int get_mode(t_channel_id channel_id);
		bool set_mode(t_channel_id channel_id);
};


#endif
