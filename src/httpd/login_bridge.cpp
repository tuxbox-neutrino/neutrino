/*
 * login_bridge.cpp - the one login, asked from outside the REST layer
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "login_bridge.h"

#include "auth.h"
#include "credentials.h"
#include "webconfig.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <cstdio>
#include <cstring>
#include <deque>

#include <strings.h>

namespace login_bridge
{

namespace
{

/* The old server's targets that change the box with a GET: ControlAPI under its
   four prefixes and the template commands under /y/. */
const char *const kChangesWithGet[] = { "/control/", "/cgi-bin/", "/fb/", "/api/v4/", "/y/" };

bool changesWithGet(const std::string &path)
{
	for (size_t i = 0; i < sizeof(kChangesWithGet) / sizeof(kChangesWithGet[0]); ++i)
		if (path == kChangesWithGet[i])
			return true;
	return false;
}

/* Whether an address names the host this request was sent to: http or https,
   then exactly that host, then nothing or a path. Compared without regard to
   case, a host name having none. */
bool namesHost(const std::string &address, const std::string &host)
{
	if (host.empty())
		return false;

	static const char *const schemes[] = { "http://", "https://" };
	for (size_t i = 0; i < 2; ++i)
	{
		const size_t n = strlen(schemes[i]);
		if (address.size() <= n || strncasecmp(address.c_str(), schemes[i], n) != 0)
			continue;
		const size_t slash = address.find('/', n);
		const std::string named = address.substr(n, (slash == std::string::npos) ? std::string::npos : slash - n);
		return named.size() == host.size() && strcasecmp(named.c_str(), host.c_str()) == 0;
	}
	return false;
}

/* Whether the request came from a page of this box. Sec-Fetch-Site is set by the
   browser and cannot be written by a page, so where it is present it decides alone;
   Origin and Referer are the older ways of saying the same and are read only from a
   browser that sends no Sec-Fetch-Site. A request carrying none of the three says
   nothing about where it came from and is not taken as coming from here. */
bool fromThisBox(const Ask &a)
{
	if (!a.sec_fetch_site.empty())
		return a.sec_fetch_site == "same-origin";
	if (!a.origin.empty())
		return namesHost(a.origin, a.host) && a.origin.find('/', a.origin.find("//") + 2) == std::string::npos;
	if (!a.referer.empty())
		return namesHost(a.referer, a.host);
	return false;
}

/* What a right Basic pair is remembered as: a digest keyed with a secret this
   process drew for itself, beside the stored form it was checked against. The
   pair itself is never kept, and a digest is worth nothing to anybody without
   the key, which never leaves memory. */
struct Remembered
{
	std::string digest;
	std::string user;
	std::string stored;
};

const size_t kRemembered = 16;

struct Memory
{
	std::string key;
	bool        key_drawn;
	std::deque<Remembered> pairs;

	std::string shipped_for;
	bool        shipped;
	bool        shipped_known;

	Memory() : key_drawn(false), shipped(false), shipped_known(false) {}
};

Memory &memory()
{
	static Memory m;
	return m;
}

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

// Empty when no key could be drawn, and then nothing is remembered.
std::string digestOf(const std::string &pair)
{
	std::string key;
	{
		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		if (!memory().key_drawn)
		{
			memory().key = httpd::randomToken();
			memory().key_drawn = true;
		}
		key = memory().key;
	}
	if (key.empty())
		return std::string();

	unsigned char out[EVP_MAX_MD_SIZE];
	unsigned int len = 0;
	if (HMAC(EVP_sha256(), key.data(), (int) key.size(),
	         (const unsigned char *) pair.data(), pair.size(), out, &len) == NULL)
		return std::string();
	return std::string((const char *) out, len);
}

/* A pair counts only against the account it was checked against: the same name
   and the same stored form. A name changed since is a pair the REST login would
   refuse, so it is refused here too. */
bool remembered(const std::string &digest, const httpd::Login &l)
{
	if (digest.empty())
		return false;
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	for (size_t i = 0; i < memory().pairs.size(); ++i)
	{
		const Remembered &r = memory().pairs[i];
		if (r.stored == l.password_hash && r.user == l.username && r.digest.size() == digest.size() &&
		    CRYPTO_memcmp(r.digest.data(), digest.data(), digest.size()) == 0)
			return true;
	}
	return false;
}

void remember(const std::string &digest, const httpd::Login &l)
{
	if (digest.empty())
		return;
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	Remembered r;
	r.digest = digest;
	r.user = l.username;
	r.stored = l.password_hash;
	memory().pairs.push_back(r);
	while (memory().pairs.size() > kRemembered)
		memory().pairs.pop_front();
}

void forget(std::string &secret)
{
	if (!secret.empty())
		OPENSSL_cleanse(&secret[0], secret.size());
	secret.clear();
}

} // namespace

bool sessionAdmits(const Ask &a)
{
	try
	{
		const std::string token = httpd::cookieValue(a.cookie, httpd::sessionCookieName());
		if (token.empty())
			return false;
		if ((changesWithGet(a.path) || a.runs_commands) && !fromThisBox(a))
			return false;

		/* Asked of the REST gate with the cookie and nothing else, so the answer
		   is System for a live session and below it for anything else, and the
		   session is moved on the way a request through that gate moves it. */
		httpd::Credentials c;
		c.peer = a.peer;
		c.cookie_token = token;
		return httpd::granted(c) == httpd::AuthLevel::System;
	}
	catch (...)
	{
		return false;
	}
}

bool basicAdmits(const Ask &a)
{
	try
	{
		std::string pair = httpd::basicCredential(a.authorization);
		const size_t colon = pair.find(':');
		if (colon == std::string::npos)
		{
			forget(pair);
			return false;
		}

		const httpd::Login l = httpd::loginInEffect();
		if (l.username.empty() || l.password_hash.empty())
		{
			forget(pair);
			return false;
		}

		const std::string digest = digestOf(pair);
		if (remembered(digest, l))
		{
			forget(pair);
			return true;
		}

		/* Through the login's own gate, ahead of the derivation for the reason it
		   gives: the derivation is what an attempt costs. */
		unsigned retry_after = 0;
		if (httpd::beginLoginAttempt(a.peer, &retry_after) != httpd::LoginAttempt::Open)
		{
			forget(pair);
			return false;
		}

		bool ok = false;
		try
		{
			std::string password = pair.substr(colon + 1);
			// Both halves asked, as the REST login asks them.
			const bool name_matches = pair.compare(0, colon, l.username) == 0;
			const bool secret_matches = httpd::verifySecret(password, l.password_hash);
			forget(password);
			ok = name_matches && secret_matches;
		}
		catch (...)
		{
			httpd::endLoginAttempt(a.peer, false);
			throw;
		}
		httpd::endLoginAttempt(a.peer, ok);

		if (ok)
			remember(digest, l);
		forget(pair);
		return ok;
	}
	catch (...)
	{
		return false;
	}
}

bool closeSessionOf(const std::string &cookie_header)
{
	try
	{
		return httpd::closeSession(httpd::cookieValue(cookie_header, httpd::sessionCookieName()));
	}
	catch (...)
	{
		return false;
	}
}

std::string forgetSessionCookie()
{
	try
	{
		return std::string(httpd::sessionCookieName()) + "=; HttpOnly; SameSite=Lax; Path=/; Max-Age=0";
	}
	catch (...)
	{
		return std::string();
	}
}

std::string user()
{
	try
	{
		return httpd::loginInEffect().username;
	}
	catch (...)
	{
		return std::string();
	}
}

bool passwordSet()
{
	try
	{
		return !httpd::loginInEffect().password_hash.empty();
	}
	catch (...)
	{
		return false;
	}
}

bool passwordIsShipped()
{
	try
	{
		const std::string stored = httpd::loginInEffect().password_hash;
		if (stored.empty())
			return false;
		{
			OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
			if (memory().shipped_known && memory().shipped_for == stored)
				return memory().shipped;
		}

		// the passwords these boxes have shipped with (webconfig.cpp)
		size_t count = 0;
		const char *const *shipped = httpd::shippedPasswords(&count);
		bool answer = false;
		for (size_t i = 0; i < count && !answer; ++i)
			answer = httpd::verifySecret(shipped[i], stored);

		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		memory().shipped_for = stored;
		memory().shipped = answer;
		memory().shipped_known = true;
		return answer;
	}
	catch (...)
	{
		return false;
	}
}

bool change(const std::string &user, const std::string &password, std::string *why,
            const Ask *from, bool reset_to_shipped)
{
	if (why != NULL)
		why->clear();
	try
	{
		// the rules every writer of the password is held to
		if (!password.empty() && !(reset_to_shipped && httpd::isShippedPassword(password)) &&
		    !httpd::passwordAcceptable(password, why))
			return false;

		if (from != NULL && !fromThisBox(*from))
		{
			if (why != NULL)
				*why = "the account is changed only from a page of this box";
			return false;
		}

		/* Across reading what is in effect, writing the file and publishing, and
		   shared with load: a reload installing a configuration in between would
		   otherwise leave the published account older than the file. */
		httpd::AccountLock held;

		const std::string path = httpd::configPath();
		if (path.empty())
		{
			if (why != NULL)
				*why = "no configuration file is in use";
			return false;
		}

		/* The latest saved state, which a REST save waiting for its restart may
		   have moved past what is in effect; the published password is the one in
		   effect until such a restart reads the file. */
		const httpd::Login before = httpd::loginInEffect();
		const httpd::WebSettings base = httpd::currentWebSettings();
		httpd::WebSettings s = base;
		s.account_only = true;
		if (!user.empty())
			s.username = user;
		if (!password.empty())
		{
			s.password = password;
			s.change_password = true;
		}
		if (s.username == base.username && !s.change_password)
			return true;

		std::string stored;
		const bool saved = httpd::saveWebSettings(path, s, &stored);
		forget(s.password);
		if (!saved)
		{
			if (why != NULL)
			{
				const std::vector<std::string> &problems = httpd::configProblems();
				for (size_t i = 0; i < problems.size(); ++i)
				{
					if (i > 0)
						*why += "; ";
					*why += problems[i];
				}
				if (why->empty())
					*why = "the configuration could not be written";
			}
			return false;
		}

		httpd::Login now;
		now.username = s.username;
		now.password_hash = s.change_password ? stored : before.password_hash;
		httpd::publishLogin(now);
		return true;
	}
	catch (...)
	{
		if (why != NULL)
			*why = "the account could not be changed";
		return false;
	}
}

} // namespace login_bridge
