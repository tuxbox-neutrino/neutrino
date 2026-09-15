/*
 * test_zapitchannel_copy.cpp - tests for copying a zapit channel
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

#include "support/catch.hpp"

#include <zapit/channel.h>
#include <zapit/zapit.h>

#include <string>

/* channel.cpp reaches this from a path this test never runs. Linking the
 * translation unit that defines it would pull in the whole channel stack.
 * The xml converter it also reaches used to be answered here by a stub that
 * handed its argument back, and that stub silently answered for the layer that
 * escapes a value with the real one, because an object beats an archive. */
Zapit_config zapitCfg;

namespace
{

CZapitChannel *makeTunedChannel()
{
	// The name has to be typed, or an empty literal picks the webtv
	// constructor, which returns early on the null url.
	CZapitChannel *c = new CZapitChannel(std::string("probe"), 0x2b66, 1, 192, 11493);
	c->addAudioChannel(0x100, CZapitAudioChannel::MPEG, "deu", 0);
	c->addAudioChannel(0x101, CZapitAudioChannel::AC3, "eng", 1);
	c->addDVBSubtitle(0x200, "deu", 0x10, 1, 1);
	c->setRawPmt(new unsigned char[8], 8);
	return c;
}

} // anonymous namespace

TEST_CASE("a copied channel takes over no allocation of the original", "[zapitchannel]")
{
	CZapitChannel *live = makeTunedChannel();

	REQUIRE(live->getAudioChannelCount() == 2);
	REQUIRE(live->getSubtitleCount() == 1);
	int live_len = 0;
	unsigned char *live_pmt = live->getRawPmt(live_len);
	REQUIRE(live_pmt != NULL);
	REQUIRE(live_len == 8);

	{
		CZapitChannel copy = *live;

		REQUIRE(copy.getName() == "probe");
		REQUIRE(copy.getChannelID() == 0x2b66);
		REQUIRE(copy.getSatellitePosition() == 192);
		REQUIRE(copy.getFreqId() == 11493);

		REQUIRE(copy.getAudioChannelCount() == 0);
		REQUIRE(copy.getSubtitleCount() == 0);
		int len = -1;
		REQUIRE(copy.getRawPmt(len) == NULL);
		REQUIRE(len == 0);

		// Assigned over a target that holds its own, which is what a reused
		// output buffer looks like.
		CZapitChannel *target = makeTunedChannel();
		*target = *live;
		REQUIRE(target->getChannelID() == 0x2b66);
		REQUIRE(target->getAudioChannelCount() == 0);
		REQUIRE(target->getSubtitleCount() == 0);
		int alen = -1;
		REQUIRE(target->getRawPmt(alen) == NULL);
		REQUIRE(alen == 0);
		delete target;
	}

	// Destroying the copies must not have freed anything the original owns.
	REQUIRE(live->getAudioChannelCount() == 2);
	REQUIRE(live->getSubtitleCount() == 1);
	REQUIRE(live->getAudioPid(0) == 0x100);
	REQUIRE(live->getAudioPid(1) == 0x101);
	int len_after = 0;
	REQUIRE(live->getRawPmt(len_after) == live_pmt);
	REQUIRE(len_after == 8);

	delete live;
}

TEST_CASE("a copied channel does not carry the borrowed provider name", "[zapitchannel]")
{
	// What a bouquet does: hands the channel a pointer into its own name.
	std::string bouquet_name = "Astra 19.2E";
	CZapitChannel *live = makeTunedChannel();
	live->pname = (char *) bouquet_name.c_str();
	live->has_bouquet = true;

	CZapitChannel copy = *live;
	REQUIRE(copy.pname == NULL);
	REQUIRE(live->pname == bouquet_name.c_str());

	CZapitChannel *target = makeTunedChannel();
	target->pname = (char *) bouquet_name.c_str();
	*target = *live;
	REQUIRE(target->pname == NULL);
	delete target;

	delete live;
}
