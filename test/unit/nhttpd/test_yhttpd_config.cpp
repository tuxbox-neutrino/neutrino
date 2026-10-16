/*
 * test_yhttpd_config.cpp - what nhttpd's configuration migration does to a
 * list it finds
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 *
 * The file types a box sends are kept in nhttpd.conf, and a list written
 * there once is preferred to the built-in one ever after. The step that
 * brings such a list up to date must add what is new without taking away
 * what an owner set.
 */

#include "support/catch.hpp"

#include "helper.h"

#include <string>

TEST_CASE("a saved file type list keeps its order and gains the missing defaults at its end", "[nhttpd]")
{
	const std::string got = mergeMimeTypes("htm:text/html,png:image/png",
		"html:text/html,png:image/png,svg:image/svg+xml");
	CHECK(got == "htm:text/html,png:image/png,html:text/html,svg:image/svg+xml");
}

TEST_CASE("a js sent as plain text is sent as JavaScript after the merge", "[nhttpd]")
{
	CHECK(mergeMimeTypes("css:text/css,js:text/plain", "js:application/javascript")
		== "css:text/css,js:application/javascript");
}

TEST_CASE("a type an owner chose for an extension stays", "[nhttpd]")
{
	CHECK(mergeMimeTypes("js:text/javascript,png:image/x-png", "js:application/javascript,png:image/png")
		== "js:text/javascript,png:image/x-png");
}

TEST_CASE("a list with nothing in it becomes the defaults", "[nhttpd]")
{
	CHECK(mergeMimeTypes("", "a:x/a,b:x/b") == "a:x/a,b:x/b");
}

TEST_CASE("blanks, empty entries and entries without a type are dropped from the list", "[nhttpd]")
{
	CHECK(mergeMimeTypes(" htm : text/html ,, junk,png:image/png,", "png:image/png")
		== "htm:text/html,png:image/png");
}

TEST_CASE("an extension named twice is kept twice, as the reader keeps the last", "[nhttpd]")
{
	CHECK(mergeMimeTypes("png:image/x-png,png:image/png", "png:image/png,gif:image/gif")
		== "png:image/x-png,png:image/png,gif:image/gif");
}
