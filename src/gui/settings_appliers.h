/*
	Neutrino-GUI

	Copyright (C) 2026

	License: GPL
*/

#ifndef __settings_appliers__
#define __settings_appliers__

#include <coreapi/base/deps.h>

#include <system/locals.h>

#include <cstddef>

/* The GUI side of the seam a written setting takes effect through. The facade
   may not reach into the screens, so what applies a change registers itself
   there and is called back; one applier stands for one section, which is one
   notifier group, and the key says which setting moved.

   Registered by CNeutrinoApp::registerSettingsAppliers at start-up. A section
   whose applier nobody registers is written and no more, which is why the
   registration is held to by a check over the application. */

/* One declared setting a section acts on, and the option its notifier takes for
   it. Named by the key and not by the label, because a label is not one setting:
   in the display section led_standby_mode and backlight_standby both carry
   ledcontroler.mode.standby, and elsewhere the sharing runs to eight rows on one
   label. A route on the label hands one setting's write to another setting's
   notifier and neither the key nor the answer says so.

   These lists are also the whole of what an applier does. A section's notifier
   branches on a handful of options and does nothing for the rest: what is
   registered says a section can be told, what is listed says which of its
   settings it will be told about. */
struct AppliedSetting
{
	const char       *key;
	neutrino_locale_t option;
};

class CSectionSettingsApplier : public coreapi::SettingsApplier
{
	public:
		/* False for a key this section's notifier does nothing for, so a row
		   nothing acts on is not reported as applied. What comes back from the
		   notifiers is whether a menu wants repainting and is not this. */
		bool apply(const char *key);

	protected:
		CSectionSettingsApplier(const AppliedSetting *r, size_t n) : rows(r), count(n) {}

		/* What the section's notifier is told, which is the locale the list
		   above pairs with the key. A key the list does not name never reaches
		   here. */
		virtual bool applyOption(const char *key, neutrino_locale_t option) = 0;

		/* The value the row holds, for the notifiers that read what they are
		   handed rather than the program's settings. Read out of the settings
		   because the write has already reached them. */
		static bool numberOf(const char *key, int &out);

	private:
		const AppliedSetting *rows;
		size_t                count;
};

class CAudioSettingsApplier : public CSectionSettingsApplier
{
	public:
		CAudioSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CVideoSettingsApplier : public CSectionSettingsApplier
{
	public:
		CVideoSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CMiscSettingsApplier : public CSectionSettingsApplier
{
	public:
		CMiscSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CCamSettingsApplier : public CSectionSettingsApplier
{
	public:
		CCamSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CKeybindingsSettingsApplier : public CSectionSettingsApplier
{
	public:
		CKeybindingsSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CRecordingSettingsApplier : public CSectionSettingsApplier
{
	public:
		CRecordingSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CUpdateSettingsApplier : public CSectionSettingsApplier
{
	public:
		CUpdateSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class CDisplaySettingsApplier : public CSectionSettingsApplier
{
	public:
		CDisplaySettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

class COsdSettingsApplier : public CSectionSettingsApplier
{
	public:
		COsdSettingsApplier();

	protected:
		bool applyOption(const char *key, neutrino_locale_t option);
};

#endif
