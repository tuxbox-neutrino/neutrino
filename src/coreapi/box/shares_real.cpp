/*
 * shares_real.cpp - the network shares as the program keeps and mounts them
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include <config.h>

#include "coreapi/shares.h"
#include "coreapi/base/deps.h"

#include <string>
#include <vector>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <neutrinoMessages.h>
#include <system/fsmounter.h>
#include <system/settings.h>

extern SNeutrinoSettings g_settings;

namespace coreapi
{
namespace shares
{

namespace
{

bool (*saveSettings)() = NULL;

Kind kindOf(int type)
{
	switch (type)
	{
		case CFSMounter::CIFS: return Kind::Cifs;
		case CFSMounter::LUFS: return Kind::Lufs;
	}
	return Kind::Nfs;
}

CFSMounter::FSType typeOf(Kind k)
{
	switch (k)
	{
		case Kind::Cifs: return CFSMounter::CIFS;
		case Kind::Lufs: return CFSMounter::LUFS;
		case Kind::Nfs:  break;
	}
	return CFSMounter::NFS;
}

/* The slots as the program last published them, and a write held until the
   program's loop carries it.

   Never the program's own slots from a request thread: the screen of the
   network shares writes their numbers without a lock anybody else could take,
   and edits their strings in place. So the loop publishes a copy whenever they
   change (refresh): after it read its settings, when that screen closes, and
   after it carried a write. A request reads that copy, a held write answering
   over it, both under the one lock the carry takes, so a reader never meets a
   write half carried or one gone from the hold and not yet in the copy.

   The loop takes a held write over when no text input of the screen is open
   (APPLY_SETTINGS in neutrino.cpp waits for one to close), then saves.

   One write at a time from holding it to its post. A password left out is the
   one a write still held named, and otherwise the program's own, which the
   loop reads as it carries the write: the screen at the box may have changed
   it since the copy was published. A write that could not be posted takes
   back exactly its own; one the loop carried meanwhile has been applied, and
   is answered so. */
class RealShares : public Source
{
	public:
		RealShares() : published(kSlots), held(kSlots), named(kSlots, false) {}

		Status read(std::vector<Share> &shares, std::vector<State> &states) const
		{
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				shares = published;
				for (size_t i = 0; i < kSlots; ++i)
				{
					if (!named[i])
						continue;
					shares[i] = held[i];
					// one left out stays what the program has, as far as the copy knows it
					if (!held[i].change_password)
						shares[i].password = published[i].password;
				}
			}
			states.assign(kSlots, State::Unmounted);
			for (size_t i = 0; i < kSlots; ++i)
			{
				CFSMounter::MountInfo top;
				if (!CFSMounter::topMount(shares[i].local_dir, top))
					continue;
				const bool ours = !shares[i].host.empty() &&
					CFSMounter::isShare(top, typeOf(shares[i].kind), shares[i].host, shares[i].remote_dir);
				states[i] = ours ? State::Mounted : State::Foreign;
			}
			return Status::Ok;
		}

		Status write(size_t slot, const Share &share)
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> one_at_a_time(writing);
			Share before;
			bool was_named = false;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				before = held[slot];
				was_named = named[slot];
				Share now = share;
				if (!now.change_password && was_named && held[slot].change_password)
				{
					now.password = held[slot].password;
					now.change_password = true;
				}
				held[slot] = now;
				named[slot] = true;
			}
			/* Posted with the lock let go: the loop takes it to carry what was
			   posted, and waiting for room in its queue while holding it would
			   wait for itself. */
			Result<void> posted = postCommand(NeutrinoMessages::APPLY_SETTINGS, 0);
			if (posted.ok())
				return Status::Ok;
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			// Taken by a carry another message started: it is in the program.
			if (!named[slot])
				return Status::Ok;
			held[slot] = before;
			named[slot] = was_named;
			return posted.error().status;
		}

		Mounted mount(const Share &s)
		{
			switch (CFSMounter::mount(s.host, s.remote_dir, s.local_dir, typeOf(s.kind), s.username,
						  s.password, s.options1, s.options2))
			{
				case CFSMounter::MRES_OK:                 return Mounted::Ok;
				case CFSMounter::MRES_FS_ALREADY_MOUNTED: return Mounted::Busy;
				case CFSMounter::MRES_FS_NOT_SUPPORTED:   return Mounted::Unsupported;
				case CFSMounter::MRES_TIMEOUT:            return Mounted::Timeout;
				case CFSMounter::MRES_UNKNOWN:            break;
			}
			return Mounted::Failed;
		}

		Unmounted unmount(const Share &s)
		{
			switch (CFSMounter::umountShare(s.local_dir, typeOf(s.kind), s.host, s.remote_dir))
			{
				case CFSMounter::UMRES_OK:       return Unmounted::Ok;
				case CFSMounter::UMRES_NOT_OURS: return Unmounted::NotOurs;
				case CFSMounter::UMRES_ERR:      break;
			}
			return Unmounted::Failed;
		}

		// On the loop's thread: the slots into the program, the copy, then the save.
		void carry()
		{
			bool any = false;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				for (size_t i = 0; i < kSlots; ++i)
				{
					if (!named[i])
						continue;
					const Share &s = held[i];
					// the numbers only ever on this thread, the strings under their lock
					auto &n = g_settings.network_nfs[i];
					n.automount = s.automount ? 1 : 0;
					n.type = typeOf(s.kind);
					setSettingsText(n.ip, s.host);
					setSettingsText(n.dir, s.remote_dir);
					setSettingsText(n.local_dir, s.local_dir);
					setSettingsText(n.username, s.username);
					if (s.change_password)
						setSettingsText(n.password, s.password);
					setSettingsText(n.mount_options1, s.options1);
					setSettingsText(n.mount_options2, s.options2);
					setSettingsText(n.mac, s.mac);
					held[i] = Share();
					named[i] = false;
					any = true;
				}
				if (any)
					publishLocked();
			}
			if (any && saveSettings != NULL)
				saveSettings();
		}

		// On the loop's thread, after the program changed the slots itself.
		void refresh()
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			publishLocked();
		}

	private:
		// On the loop's thread, the only one that writes the slots.
		void publishLocked()
		{
			CSettingsTextGuard text;
			for (size_t i = 0; i < kSlots; ++i)
			{
				const auto &n = g_settings.network_nfs[i];
				Share &s = published[i];
				s = Share();
				s.automount = n.automount != 0;
				s.kind = kindOf(n.type);
				s.host = n.ip;
				s.remote_dir = n.dir;
				s.local_dir = n.local_dir;
				s.username = n.username;
				s.password = n.password;
				s.options1 = n.mount_options1;
				s.options2 = n.mount_options2;
				s.mac = n.mac;
			}
		}

		mutable OpenThreads::Mutex guard;
		OpenThreads::Mutex writing;
		std::vector<Share> published;
		std::vector<Share> held;
		std::vector<bool> named;
};

} // namespace

void installRealSource(bool (*save)())
{
	static RealShares real;
	saveSettings = save;
	setSource(&real);
	real.refresh();
}

} // namespace shares
} // namespace coreapi
