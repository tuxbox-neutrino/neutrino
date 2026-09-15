/*
 * deps.cpp - the seams the core API is built on, and their wiring
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

#include "deps.h"
#include "errors.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <driver/rcinput.h>
#include <neutrinoMessages.h>

namespace coreapi
{

static ChannelSource *g_channel_source = 0;

// Aborts instead of returning a null reference: the caller would otherwise
// fault far away from the missing installation, with nothing naming it.
ChannelSource &channelSource()
{
	if (!g_channel_source)
	{
		std::fputs("coreapi: no ChannelSource installed\n", stderr);
		std::abort();
	}
	return *g_channel_source;
}

void setChannelSource(ChannelSource *s) { g_channel_source = s; }

static EpgSource *g_epg_source = 0;

EpgSource &epgSource()
{
	if (!g_epg_source)
	{
		std::fputs("coreapi: no EpgSource installed\n", stderr);
		std::abort();
	}
	return *g_epg_source;
}

void setEpgSource(EpgSource *s) { g_epg_source = s; }

static TimerSource *g_timer_source = 0;

TimerSource &timerSource()
{
	if (!g_timer_source)
	{
		std::fputs("coreapi: no TimerSource installed\n", stderr);
		std::abort();
	}
	return *g_timer_source;
}

void setTimerSource(TimerSource *s) { g_timer_source = s; }

static SystemSource *g_system_source = 0;

SystemSource &systemSource()
{
	if (!g_system_source)
	{
		std::fputs("coreapi: no SystemSource installed\n", stderr);
		std::abort();
	}
	return *g_system_source;
}

void setSystemSource(SystemSource *s) { g_system_source = s; }

static TunerSource *g_tuner_source = 0;

TunerSource &tunerSource()
{
	if (!g_tuner_source)
	{
		std::fputs("coreapi: no TunerSource installed\n", stderr);
		std::abort();
	}
	return *g_tuner_source;
}

void setTunerSource(TunerSource *s) { g_tuner_source = s; }
static InputDevice *g_input_device = 0;

InputDevice &inputDevice()
{
	if (!g_input_device)
	{
		std::fputs("coreapi: no InputDevice installed\n", stderr);
		std::abort();
	}
	return *g_input_device;
}

void setInputDevice(InputDevice *d) { g_input_device = d; }

static ScreenshotSource *g_screenshot_source = 0;

ScreenshotSource &screenshotSource()
{
	if (!g_screenshot_source)
	{
		std::fputs("coreapi: no ScreenshotSource installed\n", stderr);
		std::abort();
	}
	return *g_screenshot_source;
}

void setScreenshotSource(ScreenshotSource *s) { g_screenshot_source = s; }

static LogoSource *g_logo_source = 0;

LogoSource &logoSource()
{
	if (!g_logo_source)
	{
		std::fputs("coreapi: no LogoSource installed\n", stderr);
		std::abort();
	}
	return *g_logo_source;
}

void setLogoSource(LogoSource *s) { g_logo_source = s; }

static PluginSource *g_plugin_source = 0;

PluginSource &pluginSource()
{
	if (!g_plugin_source)
	{
		std::fputs("coreapi: no PluginSource installed\n", stderr);
		std::abort();
	}
	return *g_plugin_source;
}

void setPluginSource(PluginSource *s) { g_plugin_source = s; }

static RecordingSource *g_recording_source = 0;

RecordingSource &recordingSource()
{
	if (!g_recording_source)
	{
		std::fputs("coreapi: no RecordingSource installed\n", stderr);
		std::abort();
	}
	return *g_recording_source;
}

void setRecordingSource(RecordingSource *s) { g_recording_source = s; }

namespace
{
/* What a read reaches while no store is installed. It refuses rather than
   answering nothing: a box whose settings cannot be reached at all and one whose
   settings are all at their default look the same from a value that is missing,
   and only the first is worth reporting. */
class NoStore : public SettingsSource
{
	public:
		Status readInt(const char *, long &) const { return Status::NotSupported; }
		Status readString(const char *, std::string &) const { return Status::NotSupported; }
		Status writeInt(const char *, long) { return Status::NotSupported; }
		Status writeString(const char *, const std::string &) { return Status::NotSupported; }
		Status persist() { return Status::NotSupported; }
};

NoStore g_no_store;
} // anonymous namespace

static SettingsSource *g_settings_source = 0;

SettingsSource &settingsSource()
{
	if (!g_settings_source)
		return g_no_store;
	return *g_settings_source;
}

void setSettingsSource(SettingsSource *s) { g_settings_source = s; }

namespace
{
/* What a read reaches while no catalog is installed: every name answers
   NotFound, which is what a name really absent from the catalog answers too, so
   a caller never mistakes "nothing is wired in" for licence to print the key it
   was given. */
class NoLocale : public LocaleSource
{
	public:
		Status text(const char *, std::string &) const { return Status::NotFound; }
};

NoLocale g_no_locale;
} // anonymous namespace

static LocaleSource *g_locale_source = 0;

LocaleSource &localeSource()
{
	if (!g_locale_source)
		return g_no_locale;
	return *g_locale_source;
}

void setLocaleSource(LocaleSource *s) { g_locale_source = s; }

namespace
{
struct ApplierEntry
{
	const char      *section;
	SettingsApplier *applier;
};

/* A fixed array at file scope, so it is zeroed before anything runs and putting
   an applier in it allocates nothing. A free entry is one with no section name:
   a null applier under a name would be an entry a later registration could not
   tell from a used one. */
ApplierEntry g_appliers[SETTINGS_APPLIER_LIMIT];

// By the name and not by the pointer, so a caller may hand over an equal name
// held somewhere else than the one it registered with.
ApplierEntry *findApplier(const char *section)
{
	for (size_t i = 0; i < SETTINGS_APPLIER_LIMIT; ++i)
	{
		if (g_appliers[i].section != 0 &&
		    std::strcmp(g_appliers[i].section, section) == 0)
			return &g_appliers[i];
	}
	return 0;
}
} // anonymous namespace

Status registerSettingsApplier(const char *section, SettingsApplier *a)
{
	if (section == 0 || section[0] == '\0')
		return Status::InvalidArgument;

	ApplierEntry *e = findApplier(section);

	if (a == 0)
	{
		if (e != 0)
		{
			e->section = 0;
			e->applier = 0;
		}
		return Status::Ok;
	}

	for (size_t i = 0; i < SETTINGS_APPLIER_LIMIT && e == 0; ++i)
	{
		if (g_appliers[i].section == 0)
			e = &g_appliers[i];
	}
	// The ceiling is one this layer chose, so reaching it is a fault here.
	if (e == 0)
		return Status::Internal;

	e->section = section;
	e->applier = a;
	return Status::Ok;
}

SettingsApplier *settingsApplier(const char *section)
{
	if (section == 0)
		return 0;
	ApplierEntry *e = findApplier(section);
	return e != 0 ? e->applier : 0;
}

static EventSink *g_event_sink = 0;

EventSink &eventSink()
{
	if (!g_event_sink)
	{
		std::fputs("coreapi: no EventSink installed\n", stderr);
		std::abort();
	}
	return *g_event_sink;
}

void setEventSink(EventSink *s) { g_event_sink = s; }

/* A case per command rather than a table read by the enumerator, so a value cast
   in from outside the enum has nothing it could read past the end of. An
   enumerator added without a number here answers zero and is refused as no
   command at all; what catches it is the case per command in the tests, because
   neither build turns on the warning that would catch it here.

   Count is a length and not a command, and neither is a number cast in from
   outside. Zero, which no message carries, is the answer for both. */
unsigned eventNumber(BoxEvent e)
{
	switch (e)
	{
		case BoxEvent::StandbyOn:      return NeutrinoMessages::STANDBY_ON;
		case BoxEvent::StandbyOff:     return NeutrinoMessages::STANDBY_OFF;
		case BoxEvent::Shutdown:       return NeutrinoMessages::SHUTDOWN;
		case BoxEvent::Reboot:         return NeutrinoMessages::REBOOT;
		case BoxEvent::Restart:        return NeutrinoMessages::RESTART;
		case BoxEvent::HdmiCecViewOn:  return NeutrinoMessages::EVT_HDMI_CEC_VIEW_ON;
		case BoxEvent::HdmiCecStandby: return NeutrinoMessages::EVT_HDMI_CEC_STANDBY;
		case BoxEvent::Hint:           return NeutrinoMessages::EVT_POPUP;
		case BoxEvent::Message:        return NeutrinoMessages::EVT_EXTMSG;
		case BoxEvent::SetVolume:      return NeutrinoMessages::EVT_SET_VOLUME;
		case BoxEvent::SetMute:        return NeutrinoMessages::EVT_SET_MUTE;
		case BoxEvent::LockRemote:     return NeutrinoMessages::LOCK_RC;
		case BoxEvent::UnlockRemote:   return NeutrinoMessages::UNLOCK_RC;
		case BoxEvent::StartPlugin:    return NeutrinoMessages::EVT_START_PLUGIN;
		case BoxEvent::ReloadPlugins:  return NeutrinoMessages::EVT_RELOAD_PLUGINS;
		case BoxEvent::ReloadSetup:    return NeutrinoMessages::RELOAD_SETUP;
		case BoxEvent::StartTimeshift: return NeutrinoMessages::EVT_START_TIMESHIFT;
		case BoxEvent::StopTimeshift:  return NeutrinoMessages::EVT_STOP_TIMESHIFT;
		case BoxEvent::Count:          break;
	}
	return 0;
}

Result<void> postEvent(BoxEvent e, const void *body, size_t size)
{
	unsigned id = eventNumber(e);
	if (id == 0)
		return fail(Status::InvalidArgument, ErrorCode::NotACommand,
			    "that is not a command this layer sends");

	Status s = eventSink().send(id, body, size);
	if (s != Status::Ok)
		return fail(s, ErrorCode::CommandNotSent,
			    "the box did not take the command");
	return ok();
}

static CommandSink *g_command_sink = 0;

CommandSink &commandSink()
{
	if (!g_command_sink)
	{
		std::fputs("coreapi: no CommandSink installed\n", stderr);
		std::abort();
	}
	return *g_command_sink;
}

void setCommandSink(CommandSink *s) { g_command_sink = s; }

Result<void> postCommand(neutrino_msg_t msg, neutrino_msg_data_t data)
{
	Status s = commandSink().post(msg, data);
	if (s != Status::Ok)
		return fail(s, ErrorCode::CommandNotPosted,
			    "the box did not take the command");
	return ok();
}

namespace
{
/* The queue frees a refused payload itself for the messages whose number falls
   in this range, so a release here would be the second one. It says nothing for
   the messages outside it, and several of those carry a block all the same,
   which is why the free below is not simply dropped. Both bounds come from the
   queue's own names. */
bool queueFreesRefusedPayload(neutrino_msg_t msg)
{
	return msg >= CRCInput::RC_WithData &&
	       msg < CRCInput::RC_WithDataEnd;
}
} // anonymous namespace

Result<void> postPayload(neutrino_msg_t msg, const void *payload, size_t size)
{
	unsigned char *block = new unsigned char[size];
	std::memcpy(block, payload, size);

	Status s = commandSink().post(msg, (neutrino_msg_data_t) block);
	if (s != Status::Ok)
	{
		/* Both facts have to hold before the release is left to the queue: the
		   message has to be one it releases for, and the sink has to have
		   reached it at all. Busy is the only unsuccessful answer that says so.
		   delete[] against the new[] above, which is also the pair the loop
		   frees a delivered payload with. */
		if (!(s == Status::Busy && queueFreesRefusedPayload(msg)))
			delete[] block;
		return fail(s, ErrorCode::CommandNotPosted,
			    "the box did not take the command");
	}
	return ok();
}

/* The seams whose accessors above abort, and nothing else. Written as one
   expression over the same variables those accessors read, so a seam added there
   with an abort and forgotten here is one this reports as present while reaching
   it still ends the process; there is no way to enumerate them automatically.

   No lock. Every one of these is written once during startup by a thread that is
   the only one running at the time, and read afterwards by threads that start
   later, so the writes are ordered before the reads by the act of starting
   them. */
namespace
{
/* What a read reaches while no daemon is wired in. NotSupported and not a pair
   of noughts: nought is a real setting, and answering it for a daemon nobody can
   reach would report the box as recording from the second the programme starts. */
class NoRecordingSafety : public RecordingSafetySource
{
	public:
		Status read(int &, int &) const { return Status::NotSupported; }
		Status write(int, int) { return Status::NotSupported; }
};

NoRecordingSafety g_no_recording_safety;
} // anonymous namespace

static RecordingSafetySource *g_recording_safety_source = 0;

RecordingSafetySource &recordingSafetySource()
{
	if (!g_recording_safety_source)
		return g_no_recording_safety;
	return *g_recording_safety_source;
}

void setRecordingSafetySource(RecordingSafetySource *s) { g_recording_safety_source = s; }

namespace
{
/* What a read reaches while no screen has registered its lists. Every name
   answers NotFound, so a row whose values are asked for offers none and takes
   none. The alternative is accepting a value on a box that cannot show it. */
class NoChoices : public SettingChoices
{
	public:
		Status values(const char *, std::vector<SettingChoice> &) const
		{
			return Status::NotFound;
		}
};

NoChoices g_no_choices;
} // anonymous namespace

static SettingChoices *g_setting_choices = 0;

SettingChoices &settingChoices()
{
	if (!g_setting_choices)
		return g_no_choices;
	return *g_setting_choices;
}

void setSettingChoices(SettingChoices *s) { g_setting_choices = s; }

namespace
{
/* What a read reaches while no screen has handed its copy over. NotSupported and
   not the smaller of the two sizes: both are real values of this setting, so
   answering either would report the box as drawing at a size nothing knows it is
   drawing at. */
class NoOsdResolution : public OsdResolutionSource
{
	public:
		Status read(int &) const { return Status::NotSupported; }
		Status write(int) { return Status::NotSupported; }
};

NoOsdResolution g_no_osd_resolution;
} // anonymous namespace

static OsdResolutionSource *g_osd_resolution_source = 0;

OsdResolutionSource &osdResolutionSource()
{
	if (!g_osd_resolution_source)
		return g_no_osd_resolution;
	return *g_osd_resolution_source;
}

void setOsdResolutionSource(OsdResolutionSource *s) { g_osd_resolution_source = s; }

bool dependenciesInstalled()
{
	return g_channel_source != 0 &&
	       g_epg_source != 0 &&
	       g_timer_source != 0 &&
	       g_system_source != 0 &&
	       g_tuner_source != 0 &&
	       g_input_device != 0 &&
	       g_screenshot_source != 0 &&
	       g_logo_source != 0 &&
	       g_plugin_source != 0 &&
	       g_recording_source != 0 &&
	       g_command_sink != 0 &&
	       g_event_sink != 0;
}

} // namespace coreapi
