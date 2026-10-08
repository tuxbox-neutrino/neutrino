/*
 * test_choicesources.cpp - tests for the lists a text setting offers
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/catch.hpp"

#include "coreapi/base/errors.h"
#include "coreapi/base/schema.h"
#include "coreapi/settings/choicesources.h"
#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingsfield.h"
#include "support/fakes.h"

#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace coreapi;

namespace
{

// A directory of the case's own, removed with everything the case put in it.
struct ScratchDir
{
	std::string path;
	std::vector<std::string> files;
	ScratchDir()
	{
		char tmpl[] = "/tmp/coreapi-choices-XXXXXX";
		path = mkdtemp(tmpl) ? tmpl : "";
	}
	~ScratchDir()
	{
		for (size_t i = 0; i < files.size(); ++i)
			unlink(files[i].c_str());
		for (size_t i = dirs.size(); i > 0; --i)
			rmdir(dirs[i - 1].c_str());
		rmdir(path.c_str());
	}
	std::vector<std::string> dirs;
	std::string sub(const std::string &name)
	{
		const std::string p = path + "/" + name;
		mkdir(p.c_str(), 0700);
		dirs.push_back(p);
		return p;
	}
	void put(const std::string &full, const std::string &content)
	{
		std::ofstream f(full.c_str());
		f << content;
		files.push_back(full);
	}
};

std::vector<std::string> labelsOf(const std::vector<SettingChoice> &list)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < list.size(); ++i)
		out.push_back(list[i].label);
	return out;
}

// What the fixture row below offers, as a case sets it; false is a box that cannot say.
bool g_offers = true;
std::vector<std::string> g_offered;

bool fixtureChoices(std::vector<SettingChoice> &out)
{
	if (!g_offers)
		return false;
	std::vector<SettingChoice> found;
	for (size_t i = 0; i < g_offered.size(); ++i)
	{
		SettingChoice one;
		one.text = g_offered[i];
		found.push_back(one);
	}
	out.swap(found);
	return true;
}

const Descriptor kOffered[] =
{
	{
		"fixture_name", ValueType::String, "fixture", "label", NULL,
		0, 0, NULL, 0, 0, "default name", false, false, COREAPI_ALWAYS,
		COREAPI_TEXT_FIELD_OFFERED(language, fixtureChoices)
	},
};

} // namespace

TEST_CASE("the catalogs of the directories are listed once each by their name", "[language][choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	const std::string shipped = dir.sub("shipped"), extra = dir.sub("extra");
	dir.put(shipped + "/english.locale", "x");
	dir.put(shipped + "/deutsch.locale", "x");
	dir.put(shipped + "/readme.txt", "x");
	dir.put(extra + "/english.locale", "x");
	dir.put(extra + "/polski.locale", "x");

	std::vector<std::string> dirs;
	dirs.push_back(shipped);
	dirs.push_back(extra);
	dirs.push_back(dir.path + "/missing");
	std::vector<SettingChoice> got;
	REQUIRE(localesIn(dirs, got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 3);
	CHECK(names[0] == "deutsch");
	CHECK(names[1] == "english");
	CHECK(names[2] == "polski");
	// A text row stores the name, so the entry has to carry it as its text.
	for (size_t i = 0; i < got.size(); ++i)
		CHECK(got[i].text == got[i].label);

	// Nothing to offer leaves what the caller had.
	std::vector<SettingChoice> kept(1);
	kept[0].label = "kept";
	std::vector<std::string> none;
	none.push_back(dir.path + "/missing");
	CHECK_FALSE(localesIn(none, kept));
	REQUIRE(kept.size() == 1);
	CHECK(kept[0].label == "kept");
}

TEST_CASE("the preferred languages offer none and the names of the box's table once each in the alphabet", "[language][choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	dir.put(dir.path + "/iso-639.tab",
		"## a comment\n"
		"nno\tnno\tnn\tNorwegian\n"
		"nob\tnob\tnb\tNorwegian\n"
		"ger\tdeu\tde\tGerman\n"
		"aar\taar\taa\tOld High Dutch\n");

	std::vector<SettingChoice> got;
	REQUIRE(languagesFrom(dir.path + "/iso-639.tab", got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 4);
	CHECK(names[0] == "none");
	CHECK(names[1] == "German");
	CHECK(names[2] == "Norwegian");
	CHECK(names[3] == "Old High Dutch");
	CHECK(got[2].text == "Norwegian");

	std::vector<SettingChoice> kept(1);
	CHECK_FALSE(languagesFrom(dir.path + "/absent.tab", kept));
	CHECK(kept.size() == 1);
}

TEST_CASE("the zones of the list are offered when their zone file is installed", "[language][choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	dir.sub("usr");
	dir.sub("usr/share");
	dir.sub("usr/share/zoneinfo");
	const std::string europe = dir.sub("usr/share/zoneinfo/Europe");
	dir.put(europe + "/Berlin", "tz");
	dir.put(dir.path + "/timezone.xml",
		"<?xml version=\"1.0\"?>\n<zones>\n"
		"<zone name=\"Berlin time\" zone=\"Europe/Berlin\"/>\n"
		"<zone name=\"Nowhere time\" zone=\"Europe/Nowhere\"/>\n"
		"</zones>\n");

	std::vector<SettingChoice> got;
	REQUIRE(timezonesFrom(dir.path + "/timezone.xml", dir.path, got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 1);
	CHECK(names[0] == "Berlin time");
	CHECK(got[0].text == "Berlin time");

	std::vector<SettingChoice> kept(1);
	CHECK_FALSE(timezonesFrom(dir.path + "/absent.xml", dir.path, kept));
	CHECK(kept.size() == 1);
}

TEST_CASE("the interfaces are what the directory lists but the loopback, in the alphabet", "[choices]")
{
	ScratchDir dir;
	REQUIRE(!dir.path.empty());
	dir.sub("wlan0");
	dir.sub("eth0");
	dir.sub("lo");
	dir.put(dir.path + "/.hidden", "x");

	std::vector<SettingChoice> got;
	REQUIRE(interfacesIn(dir.path, got));
	const std::vector<std::string> names = labelsOf(got);
	REQUIRE(names.size() == 2);
	CHECK(names[0] == "eth0");
	CHECK(names[1] == "wlan0");
	CHECK(got[0].text == "eth0");

	std::vector<SettingChoice> kept(1);
	CHECK_FALSE(interfacesIn(dir.path + "/missing", kept));
	CHECK(kept.size() == 1);
}

TEST_CASE("the language, the zone and the interface offer the box's lists", "[choices]")
{
	const char *const keys[] = { "language", "timezone", "ifname" };
	for (size_t i = 0; i < 3; ++i)
	{
		INFO(keys[i]);
		Result<Descriptor> d = settings::describe(keys[i]);
		REQUIRE(d.ok());
		CHECK(d.value().type == ValueType::String);
		CHECK(d.value().field.choices_from != NULL);
		CHECK(descriptorIsSane(d.value()));
	}
	CHECK(settings::describe("language").value().field.choices_from == installedLocales);
	CHECK(settings::describe("timezone").value().field.choices_from == timezoneNames);
	CHECK(settings::describe("ifname").value().field.choices_from == interfaceNames);
}

TEST_CASE("a text the box offers a list for is held to it, its default and what it holds", "[choices]")
{
	InstalledSettingsTable table(kOffered, sizeof(kOffered) / sizeof(kOffered[0]));
	FakeSettingsSource f;
	setSettingsSource(&f);
	struct Restore { ~Restore() { setSettingsSource(NULL); g_offers = true; g_offered.clear(); } } restore;

	g_offers = true;
	g_offered.clear();
	g_offered.push_back("first");
	g_offered.push_back("second");

	Result<std::vector<SettingChoice> > listed = settings::choices("fixture_name");
	REQUIRE(listed.ok());
	REQUIRE(listed.value().size() == 2);
	CHECK(listed.value()[1].text == "second");
	// The label of an entry that brings none is its text.
	CHECK(listed.value()[1].label == "second");

	REQUIRE(settings::set("fixture_name", "second").ok());
	CHECK(f.strings["fixture_name"] == "second");

	Result<void> other = settings::set("fixture_name", "third");
	REQUIRE_FALSE(other.ok());
	CHECK(other.error().code == ErrorCode::NotAListedValue);
	CHECK(f.strings["fixture_name"] == "second");

	// The default is the way back, offered or not.
	REQUIRE(settings::set("fixture_name", "default name").ok());

	// What the box holds stays writable after the list has moved on without it.
	f.strings["fixture_name"] = "gone";
	REQUIRE(settings::set("fixture_name", "gone").ok());

	// A box that cannot say what it has holds the text to nothing more.
	g_offers = false;
	REQUIRE(settings::set("fixture_name", "anything").ok());
	CHECK_FALSE(settings::choices("fixture_name").ok());
}
