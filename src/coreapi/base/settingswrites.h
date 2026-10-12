/*
 * settingswrites.h - the box's settings files left alone while settings put
 * back from an archive wait for the restart
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#ifndef __coreapi_base_settingswrites_h__
#define __coreapi_base_settingswrites_h__

#include <OpenThreads/Condition>
#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

namespace coreapi
{

/* Settings put back from an archive are on the disc, and the program still
   holds the ones it had: any save of its own from then on, the settings file,
   the timers, the channel lists, the web server's files, would write those
   over what was put back, until the restart that reads the new ones. So every
   writer of such a file asks first, and one that is told no leaves its file
   alone. freeze() waits for the writes under way and holds every later one off;
   a restore that changed nothing takes it back with thaw(), one that did keeps
   it until the program is gone.

   Header only, the state in a function's own static, so every part of the
   program that writes a settings file can ask without linking anything. A
   writer is never made to wait, so asking cannot deadlock; only freeze() waits,
   and it is not to be called by a thread holding one of these. */
class SettingsWrite
{
	public:
		SettingsWrite() : allowed_(false)
		{
			State &s = state();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s.mutex);
			if (!s.frozen)
			{
				++s.writers;
				allowed_ = true;
			}
		}

		~SettingsWrite()
		{
			if (!allowed_)
				return;
			State &s = state();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s.mutex);
			if (--s.writers == 0)
				s.drained.broadcast();
		}

		// Whether this write may go ahead.
		bool allowed() const
		{
			return allowed_;
		}

		static void freeze()
		{
			State &s = state();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s.mutex);
			s.frozen = true;
			while (s.writers > 0)
				s.drained.wait(&s.mutex);
		}

		static void thaw()
		{
			State &s = state();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s.mutex);
			s.frozen = false;
		}

		static bool frozen()
		{
			State &s = state();
			OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s.mutex);
			return s.frozen;
		}

	private:
		struct State
		{
			State() : frozen(false), writers(0) {}
			OpenThreads::Mutex mutex;
			OpenThreads::Condition drained;
			bool frozen;
			unsigned writers;
		};

		static State &state()
		{
			static State s;
			return s;
		}

		SettingsWrite(const SettingsWrite &);
		SettingsWrite &operator=(const SettingsWrite &);

		bool allowed_;
};

} // namespace coreapi

#endif
