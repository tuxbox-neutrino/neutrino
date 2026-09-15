/*
 * settingssource_real.cpp - settings read from and written to the live configuration
 *
 * Copyright (C) 2026 NI-Team
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
 */

#include "coreapi/base/deps.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/settingstable.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <pthread.h>

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <neutrinoMessages.h>

namespace coreapi
{

namespace
{

/* Reads the values the program is running on, not the file it saves them to.
   Between a load and a save the file holds what was last written and the struct
   holds what is in effect, so a read of the file answers a setting the box has
   already changed, and a write to it is overwritten by the next save.

   A write is held here and carried to the struct by the program's own loop,
   because a caller reaches this from another thread while the loop is using
   those values and several of them are strings. Saving is asked of that loop in
   the same message, so the two cannot be split: a reload arriving between them
   would throw the write away. */
class RealSettingsSource : public SettingsSource
{
	public:
		RealSettingsSource() : values(0), save(0), stamp(0) {}

		void bind(SNeutrinoSettings *v, bool (*s)())
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.clear();
			values = v;
			save = s;
		}

		Status readInt(const char *key, long &out) const
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);

			/* A value a daemon holds rather than a field. What was written here answers first, and
			   the daemon is asked with the guard let go: reaching it is a blocking exchange over a
			   socket. A daemon that cannot be reached is an answer and not a nought: nought is a real
			   value, so answering it would report a recording as starting when the programme does. */
			if (d->field.ask != 0)
			{
				{
					OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
					const Write *w = latest(d);
					if (w)
					{
						out = w->number;
						return Status::Ok;
					}
				}
				return d->field.ask(out) ? Status::Ok : Status::NotSupported;
			}

			if (d->field.read_number == 0)
				return Status::NotSupported;

			// What was written here reads back before the loop has taken it, or
			// a caller that writes and reads is told its own write did nothing.
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			const Write *w = latest(d);
			out = w ? w->number : d->field.read_number(*values);
			return Status::Ok;
		}

		Status readString(const char *key, std::string &out) const
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			if (d->field.read_text == 0)
				return Status::NotSupported;

			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			const Write *w = latest(d);
			if (w)
				out = w->text;
			else
				d->field.read_text(*values, out);
			return Status::Ok;
		}

		Status writeInt(const char *key, long value)
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			if (d->field.write_number == 0 && d->field.tell == 0)
				return Status::NotSupported;
			/* The fields are narrower than a long, so a value that does not fit is refused now
			   rather than stored as a different one later: what stores it runs on another thread and
			   has nobody left to answer. A value a daemon holds has no field to fit it into, so the
			   row's own bounds are the whole rule. */
			if (d->field.fits_number != 0 && !d->field.fits_number(value))
				return Status::InvalidArgument;

			Write w;
			w.row = d;
			w.number = value;
			w.owner = caller();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.push_back(w);
			return Status::Ok;
		}

		Status writeString(const char *key, const std::string &value)
		{
			const Descriptor *d = find(key);
			if (d == 0)
				return status(d);
			if (d->field.write_text == 0)
				return Status::NotSupported;

			Write w;
			w.row = d;
			w.text = value;
			w.owner = caller();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			held.push_back(w);
			return Status::Ok;
		}

		/* Asks the loop to take what was written and save it. What comes back says whether the
		   loop took the message, not whether the file was written: the loop answers nothing, and
		   waiting for it from here is what deadlocks.

		   What this call promises is what this caller wrote and no more. A call that stamped
		   everything held would take a second caller's write with it and, on a refused message,
		   throw that write away while answering its own caller ok. */
		Status persist()
		{
			if (!values)
				return Status::Internal;
			if (save == 0)
				return Status::NotSupported;

			const pthread_t mine = caller();
			unsigned batch = 0;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				// Nought is the mark of a write nobody has promised, so the
				// counter steps over it where it wraps.
				batch = stamp + 1;
				if (batch == 0)
					batch = 1;
				stamp = batch;
				for (size_t i = 0; i < held.size(); ++i)
				{
					if (held[i].batch == 0 && pthread_equal(held[i].owner, mine))
						held[i].batch = batch;
				}
			}

			/* Posted with the lock let go, because the loop takes the same lock
			   to drain what was posted: waiting for room in its queue while
			   holding it is what deadlocks. */
			Result<void> posted = postCommand(NeutrinoMessages::APPLY_SETTINGS, 0);
			if (posted.ok())
				return Status::Ok;

			/* Nothing will carry this batch now, so it is taken back rather than left for the next
			   message to sweep up: a caller told its value was not written must not read it back,
			   and a write of some other setting must not be what puts it into the box. */
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
			std::vector<Write> kept;
			kept.reserve(held.size());
			for (size_t i = 0; i < held.size(); ++i)
			{
				if (held[i].batch != batch)
					kept.push_back(held[i]);
			}
			held.swap(kept);
			return posted.error().status;
		}

		/* On the loop's own thread: everything written since the last time, then the program's
		   save, then whoever applies each of them. In that order, or the save writes the values it
		   had before and the notifiers read the value the box was running on. */
		void applyAndSave()
		{
			std::vector<Write> taken;
			{
				OpenThreads::ScopedLock<OpenThreads::Mutex> lock(guard);
				if (!values)
					return;
				// Everything held. What a refused post would have carried is
				// not here: that call took its own back before answering.
				taken.swap(held);

				/* Under the same lock the reads take, or a read of a value runs
				   beside the write of it. This one holds the list of pending
				   writes; the struct's own text lock is taken inside each field
				   function, which is what the screens on the box's loop take
				   too. Only ever in that order: nothing holds the text lock and
				   then asks for this one. */
				for (size_t i = 0; i < taken.size(); ++i)
				{
					const FieldRef &f = taken[i].row->field;
					if (f.write_number)
						f.write_number(*values, taken[i].number);
					else if (f.write_text)
						f.write_text(*values, taken[i].text);
				}
			}

			/* The daemons are told with the guard let go, each being a blocking exchange over a
			   socket. Before the save, so what a caller sees is the order the members take. The save
			   carries none of these, none of them ever having been in the file. */
			for (size_t i = 0; i < taken.size(); ++i)
			{
				const FieldRef &f = taken[i].row->field;
				// Reported here because it reaches no caller: the write was
				// answered before this runs.
				if (f.tell && !f.tell(taken[i].number))
					std::fprintf(stderr, "coreapi: %s was written and the daemon holding it could not be told\n",
						     taken[i].row->key);
			}

			if (save)
				save();

			/* Driven by the writes themselves rather than by a list of keys kept beside them: a
			   second record of the same thing can come apart from the first, and a value that landed
			   without its notifier is the defect this exists to stop. A setting only a restart
			   applies has nobody to tell. */
			for (size_t i = 0; i < taken.size(); ++i)
			{
				const Descriptor *row = taken[i].row;
				if (row->needs_restart)
					continue;
				/* A row whose value is not in the member it is named after has nobody to tell. Every
				   notifier reads that member, and this layer never wrote it: one run for such a row
				   would apply whatever a screen last left there, over the value just written. */
				if (!valueIsInNamedMember(row->field))
					continue;
				SettingsApplier *a = settingsApplier(row->section);
				// Reported here because it reaches no caller: the value was
				// stored and saved before anything was asked to apply it.
				if (a != 0 && !a->apply(row->key))
					std::fprintf(stderr, "coreapi: %s was written and no applier acted on it\n",
						     row->key);
			}
		}

	private:
		struct Write
		{
			const Descriptor *row;
			long              number;
			std::string       text;
			// Which post promised this write to the loop. Nought is one nobody
			// has promised, and a post that fails takes its own back by it.
			unsigned          batch;
			// Who wrote it, so that a post promises what its own caller wrote
			// and a refused one takes back no more than that.
			pthread_t         owner;

			Write() : row(0), number(0), batch(0), owner(pthread_self()) {}
		};

		/* Linear over the table, read once per call and a few hundred rows: what an index would
		   save is below what building it costs. The one table this layer declares, rather than one
		   handed in beside the values, so the lookup that finds the key and the lookup that finds
		   its field are the same table. */
		const Descriptor *find(const char *key) const
		{
			if (!values || key == 0)
				return 0;
			const Descriptor *table = settingsTable();
			const size_t count = settingsTableCount();
			for (size_t i = 0; i < count; ++i)
			{
				if (std::strcmp(table[i].key, key) == 0)
					return &table[i];
			}
			return 0;
		}

		// The last write of a setting is the one that counts, so the search runs
		// from the end. Caller holds the lock.
		const Write *latest(const Descriptor *row) const
		{
			for (size_t i = held.size(); i > 0; --i)
			{
				if (held[i - 1].row == row)
					return &held[i - 1];
			}
			return 0;
		}

		/* Who is asking. The thread, because that is what a caller is here: the layer above reaches
		   this on the thread its request runs on. Asked of the system rather than of the thread
		   wrapper beside it, which answers with nothing for a thread it did not start, and the
		   callers here are not its. */
		static pthread_t caller()
		{
			return pthread_self();
		}

		// Why the lookup found nothing: a source with nothing to read is not the
		// same answer as a key nothing declares, and only the first is a fault.
		Status status(const Descriptor *) const
		{
			if (!values)
				return Status::Internal;
			return Status::NotFound;
		}

		SNeutrinoSettings *values;
		bool             (*save)();

		// What the last post stamped its writes with. Only ever read and raised
		// under the lock.
		unsigned                    stamp;

		std::vector<Write>          held;
		mutable OpenThreads::Mutex  guard;
};

RealSettingsSource g_real_settings;

} // anonymous namespace

void installRealSettingsSource(SNeutrinoSettings *values, bool (*save)())
{
	g_real_settings.bind(values, save);
	setSettingsSource(&g_real_settings);
}

void applyPendingSettings() { g_real_settings.applyAndSave(); }

} // namespace coreapi
