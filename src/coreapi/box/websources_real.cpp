/*
 * websources_real.cpp - the web channel and guide lists of the running program
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/websources.h"
#include "coreapi/base/deps.h"

#include <list>
#include <string>
#include <vector>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <neutrinoMessages.h>
#include <sectionsdclient/sectionsdclient.h>
#include <system/settings.h>
#include <zapit/bouquets.h>
#include <zapit/client/zapitclient.h>
#include <zapit/zapit.h>

extern SNeutrinoSettings g_settings;
extern CZapitClient *g_Zapit;
extern CBouquetManager *g_bouquetManager;
extern CSectionsdClient *g_Sectionsd;

namespace coreapi
{
namespace websources
{

namespace
{

bool saveSettings();

/* The lists as the program last published them, and a write held until the
   program's loop carries it.

   Never the program's own lists from a request thread: those are std::lists the
   loop rebuilds, from the screens that edit them and from a reload of the
   settings, without a lock anybody else could take, and a copy walked while they
   are rebuilt walks freed nodes. So the loop publishes a copy whenever they
   change (refresh) and a request reads that copy, a held write answering first
   so a caller reads back what it wrote.

   One write at a time from holding it to its post: a write that could not be
   posted then takes back exactly its own, and gives back the one it replaced,
   which that one's own message still carries. A write a carry already took
   has been applied, and is answered so. */
class RealWebSources : public Source
{
	public:
		RealWebSources() : stamp(0) {}

		Status read(Lists &out) const
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			out = published;
			if (held[WebTv].named)
				out.webtv = held[WebTv].entries;
			if (held[WebRadio].named)
				out.webradio = held[WebRadio].entries;
			if (held[XmlTv].named)
				out.xmltv = held[XmlTv].entries;
			return Status::Ok;
		}

		Status write(Kind kind, const std::vector<std::string> &entries, bool reload)
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> one_at_a_time(writing);

			Held before;
			unsigned mine = 0;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				before = held[kind];
				mine = ++stamp;
				Held now;
				now.named = true;
				now.entries = entries;
				// A write it replaces was posted and asked for its own reload.
				now.reload = reload || (before.named && before.reload);
				now.stamp = mine;
				held[kind] = now;
			}

			/* Posted with the lock let go: the loop takes the same lock to carry
			   what was posted, and waiting for room in its queue while holding it
			   would wait for itself. */
			Result<void> posted = postCommand(NeutrinoMessages::APPLY_SETTINGS, 0);
			if (posted.ok())
				return Status::Ok;

			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			if (held[kind].named && held[kind].stamp == mine)
			{
				held[kind] = before;
				return posted.error().status;
			}
			// Taken by a carry another message started: it is in the program.
			return Status::Ok;
		}

		/* On the loop's thread: the lists into the program, the save, then the
		   reloads, in that order, so the save writes the new lists and the
		   reload reads them. The steps the screen takes after it changed a list
		   (gui/webchannels_setup.cpp): the channel lists read again, and the
		   guides the playlists name as well as the ones listed. */
		void carry()
		{
			bool any = false;
			bool guide = false;
			bool reload = false;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				for (int k = WebTv; k <= XmlTv; ++k)
				{
					if (!held[k].named)
						continue;
					std::list<std::string> &target = settingsList(static_cast<Kind>(k));
					{
						// a channel reload copies the web channel lists on zapit's thread
						OpenThreads::ScopedLock<OpenThreads::Mutex> lists(CZapit::webListsMutex());
						target.assign(held[k].entries.begin(), held[k].entries.end());
					}
					reload = reload || held[k].reload;
					guide = guide || k == XmlTv;
					held[k] = Held();
					any = true;
				}
				publishLocked();
			}
			if (!any)
				return;

			CZapit::getInstance()->SetWebTVXML(&g_settings.webtv_xml);
			CZapit::getInstance()->SetWebRadioXML(&g_settings.webradio_xml);
			saveSettings();

			if (!reload)
				return;
			if (g_bouquetManager)
				g_bouquetManager->setWebchannelsReloadReason("api");
			if (g_Zapit)
				g_Zapit->reinitChannels();
			if (!g_Sectionsd)
				return;
			if (guide)
				readGuides(g_settings.xmltv_xml);
			readGuides(g_settings.xmltv_xml_auto);
		}

		// On the loop's thread, after the program changed the lists itself.
		void refresh()
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			publishLocked();
		}

	private:
		struct Held
		{
			Held() : named(false), reload(false), stamp(0) {}
			bool named;
			std::vector<std::string> entries;
			bool reload;
			unsigned stamp;
		};

		static std::list<std::string> &settingsList(Kind kind)
		{
			switch (kind)
			{
				case WebRadio: return g_settings.webradio_xml;
				case XmlTv:    return g_settings.xmltv_xml;
				case WebTv:    break;
			}
			return g_settings.webtv_xml;
		}

		static void readGuides(const std::list<std::string> &guides)
		{
			for (std::list<std::string>::const_iterator it = guides.begin(); it != guides.end(); ++it)
				g_Sectionsd->readSIfromXMLTV(it->c_str());
		}

		// The guard held, on the loop's thread.
		void publishLocked()
		{
			published.webtv.assign(g_settings.webtv_xml.begin(), g_settings.webtv_xml.end());
			published.webradio.assign(g_settings.webradio_xml.begin(), g_settings.webradio_xml.end());
			published.xmltv.assign(g_settings.xmltv_xml.begin(), g_settings.xmltv_xml.end());
		}

		mutable OpenThreads::Mutex guard;
		OpenThreads::Mutex writing;
		Lists published;
		Held held[3];
		unsigned stamp;
};

RealWebSources g_real;
bool (*g_save)() = 0;

bool saveSettings()
{
	return g_save != 0 && g_save();
}

} // namespace

void installRealSource(bool (*save)())
{
	g_save = save;
	// On the program's way up, on the thread that read the settings.
	g_real.refresh();
	setSource(&g_real);
}

} // namespace websources
} // namespace coreapi
