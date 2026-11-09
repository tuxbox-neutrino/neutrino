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
#include "router.h"
#include "webconfig.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <new>

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

// What a change to the account that fromThisBox refuses is answered with.
const char kOnlyFromThisBox[] = "the account is changed only from a page of this box";

// What a change to the account a session alone asks for is answered with.
const char kNotWithSessionAlone[] =
	"a browser signed in changes the password and the user name in the settings of the web "
	"interface, which ask for the current password; this route takes them only with the Basic "
	"pair of the account or from the box itself";

// What a change Transaction::admits refuses is answered with.
const char kAccountMoved[] = "the account changed while the request was being checked";
const char kSessionEnded[] = "the session the request came with ended while it was being checked";
const char kNotChanged[]   = "the account could not be changed";

// Set by setAdmittedProbeForTest and taken back by the admission it runs after.
AdmittedProbe admitted_probe = NULL;

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

bool mayChange(const Ask &a, const std::string &user, const std::string &password, std::string *why,
               Admitted *admitted)
{
	if (why != NULL)
		why->clear();
	if (admitted != NULL)
		*admitted = Admitted();
	try
	{
		/* Before anything is checked against the account: a change landing while the
		   check runs then reads as one that landed after it, and the transaction that
		   follows refuses. Read after, it could name an account the check never saw. */
		const unsigned long long revision = httpd::accountRevision();

		if (!fromThisBox(a))
		{
			if (why != NULL)
				*why = kOnlyFromThisBox;
			return false;
		}

		/* The box itself, or the Basic pair, which carries the password in effect. A
		   session says who signed in and not that the caller knows the password now, so
		   it changes the account here no more than it does over the REST routes without
		   the current password; a session alone is refused before any derivation. The
		   session is asked all the same, being the one a change keeps signed in. */
		const bool by_session = sessionAdmits(a);
		const bool here = !httpd::proxiesConfigured(httpd::config()) && httpd::isLoopbackPeer(a.peer);
		if (!here && !basicAdmits(a))
		{
			if (why != NULL)
				*why = by_session ? kNotWithSessionAlone : "changing the account needs a sign-in";
			return false;
		}

		// the REST layer's own answer to a caller signed in while this holds
		if (passwordOwed())
		{
			if (why != NULL)
				*why = httpd::passwordChangeRequiredDetail();
			return false;
		}

		if (!user.empty() && !httpd::usernameAcceptable(user, why))
			return false;
		if (!password.empty() && !httpd::passwordAcceptable(password, why))
			return false;
		if (admitted != NULL)
		{
			admitted->revision = revision;
			if (by_session)
				admitted->session = httpd::cookieValue(a.cookie, httpd::sessionCookieName());
			admitted->owed_refuses = true;
		}
		return true;
	}
	catch (...)
	{
		if (why != NULL)
			*why = kNotChanged;
		if (admitted != NULL)
			*admitted = Admitted();
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

namespace
{

// Whether a stored form is one of the passwords these boxes have shipped with.
bool shippedStored(const std::string &stored)
{
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

// What the file said it was refused for, as one line.
void sayWhyNotSaved(std::string *why)
{
	if (why == NULL)
		return;
	why->clear();
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

/* A new first password for the account, under the name user or under the one it
   has where user is empty, written and in effect when this returns. Under the
   account's lock, which the caller holds. */
bool putFirstPassword(const std::string &user, std::string *first, std::string *why)
{
	const std::string path = httpd::configPath();
	if (path.empty())
	{
		if (why != NULL)
			*why = "no configuration file is in use";
		return false;
	}

	httpd::WebSettings s = httpd::currentWebSettings();
	s.account_only = true;
	if (!user.empty())
		s.username = user;
	s.password = httpd::drawFirstPassword();
	if (s.password.empty())
	{
		if (why != NULL)
			*why = "the random source of the system could not be read";
		return false;
	}
	s.change_password = true;
	s.first_password = true;

	std::string stored;
	if (!httpd::saveWebSettings(path, s, &stored))
	{
		forget(s.password);
		sayWhyNotSaved(why);
		return false;
	}

	httpd::Login now;
	now.username = s.username;
	now.password_hash = stored;
	httpd::publishLogin(now, s.password);
	if (first != NULL)
		*first = s.password;
	forget(s.password);
	return true;
}

} // namespace

bool passwordOwed()
{
	try
	{
		if (httpd::firstPasswordStands())
			return true;
		return shippedStored(httpd::loginInEffect().password_hash);
	}
	catch (...)
	{
		return false;
	}
}

bool replaceShippedPassword(std::string *why)
{
	if (why != NULL)
		why->clear();
	try
	{
		httpd::AccountLock held;
		if (!shippedStored(httpd::loginInEffect().password_hash))
			return true;
		return putFirstPassword(std::string(), NULL, why);
	}
	catch (...)
	{
		if (why != NULL)
			*why = "the first password could not be set";
		return false;
	}
}

bool resetToFirstPassword(std::string *first, std::string *why)
{
	if (first != NULL)
		first->clear();
	if (why != NULL)
		why->clear();
	try
	{
		httpd::AccountLock held;
		return putFirstPassword("root", first, why);
	}
	catch (...)
	{
		if (first != NULL)
			forget(*first);
		if (why != NULL)
			*why = "the first password could not be set";
		return false;
	}
}

namespace
{

/* The go-ahead of the session a change is made with, asked by the save right before
   it writes (httpd::SaveCommit): no session, no go-ahead to take. ended says the
   session was gone by then. A session the go-ahead was taken with is let go of however
   the change is left, an exception included, unless the change handed it on: held
   for good, it would stay out of the walk of the table and a logout of it would wait
   its whole allowance for a write that is not coming. */
struct SessionCommit : public httpd::SaveCommit
{
	const std::string &token;
	unsigned long long revision;
	bool ended;
	bool held;

	SessionCommit(const std::string &t, unsigned long long r) : token(t), revision(r), ended(false), held(false) {}
	~SessionCommit() { release(); }
	bool commit()
	{
		ended = !token.empty() && !httpd::commitSession(token, revision);
		held = !ended && !token.empty();
		return !ended;
	}
	// the change is done with the session it did not move
	void release()
	{
		if (!held)
			return;
		held = false;
		httpd::releaseSession(token);
	}
	// moved to new tokens, which let go of it
	void handedOn() { held = false; }
};

/* The change Transaction::change makes, with the account's lock held by the caller
   across reading what is in effect, writing the file and publishing: shared with load,
   a reload installing a configuration in between would otherwise leave the published
   account older than the file. */
bool changeHeld(const std::string &user, const std::string &password, std::string *why,
                const Ask *from, std::string &keep_session, bool &ended)
{
	{
		// the rules every writer of the password is held to
		if (!password.empty() && !httpd::passwordAcceptable(password, why))
			return false;

		if (from != NULL && !fromThisBox(*from))
		{
			if (why != NULL)
				*why = kOnlyFromThisBox;
			return false;
		}

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

		/* The commit, and the one moment a logout and this change are ordered by, taken
		   by the save once the new password is derived and both files are built, right
		   before it writes: the session that asked still stands, and is held from there
		   to its new tokens (httpd::commitSession). A logout before it, during the
		   derivation as well, ended the session, and nothing is written; one after it
		   waits for this change and ends the session it would have renewed, and the
		   caller holds nothing once the change has gone through. */
		SessionCommit go(keep_session, before.revision);
		std::string stored;
		const bool saved = httpd::saveWebSettings(path, s, &stored, NULL, &go);
		forget(s.password);
		if (!saved)
		{
			if (go.ended)
			{
				ended = true;
				if (why != NULL)
					*why = kSessionEnded;
				return false;
			}
			go.release();
			sayWhyNotSaved(why);
			return false;
		}

		// a first password goes with a new password and stays with a new name
		std::string first;
		if (!s.change_password)
			first = httpd::firstPasswordInEffect();
		httpd::Login now;
		now.username = s.username;
		now.password_hash = s.change_password ? stored : before.password_hash;
		/* Every session of the account before ends here; the one that asked goes on
		   under the account it set, with new tokens, so a copy of its cookie ends with
		   the rest. Both under the account's lock, so no other change lands in between
		   and moves the account on again. keep_session is left naming the token the
		   caller holds now. */
		const unsigned long long revision = httpd::publishLogin(now, first);
		forget(first);
		if (revision != before.revision && !keep_session.empty())
		{
			std::string fresh = httpd::reissueSession(keep_session, before.revision, revision);
			go.handedOn();
			forget(keep_session);
			keep_session = fresh;
			forget(fresh);
		}
		else if (!keep_session.empty())
		{
			// signed out meanwhile, it ends with this, and the caller holds nothing
			go.release();
			if (!httpd::sessionIsLive(keep_session))
				forget(keep_session);
		}
		return true;
	}
}

} // namespace

const std::string &Transaction::session() const
{
	return keep_;
}

bool Transaction::sessionEnded() const
{
	return ended_;
}

void admitted()
{
	if (admitted_probe != NULL)
	{
		const AdmittedProbe p = admitted_probe;
		admitted_probe = NULL;
		p();
	}
}

void setAdmittedProbeForTest(AdmittedProbe p)
{
	admitted_probe = p;
}

std::string sessionCookie(const std::string &token)
{
	try
	{
		return httpd::sessionSetCookie(token);
	}
	catch (...)
	{
		return std::string();
	}
}

Transaction::Transaction() : held_(NULL), refused_(false), ended_(false)
{
	try
	{
		held_ = new (std::nothrow) httpd::AccountLock;
	}
	catch (...)
	{
		held_ = NULL;
	}
}

Transaction::~Transaction()
{
	forget(keep_);
	delete held_;
}

bool Transaction::admits(const Admitted &a, std::string *why)
{
	if (why != NULL)
		why->clear();
	forget(keep_);
	refused_ = true;
	try
	{
		if (held_ == NULL)
		{
			if (why != NULL)
				*why = kNotChanged;
			return false;
		}
		if (httpd::accountRevision() != a.revision)
		{
			if (why != NULL)
				*why = kAccountMoved;
			return false;
		}
		if (!a.session.empty() && !httpd::sessionIsLive(a.session))
		{
			if (why != NULL)
				*why = kSessionEnded;
			return false;
		}
		if (a.owed_refuses && passwordOwed())
		{
			if (why != NULL)
				*why = httpd::passwordChangeRequiredDetail();
			return false;
		}
		keep_ = a.session;
		refused_ = false;
		admitted();
		return true;
	}
	catch (...)
	{
		if (why != NULL)
			*why = kNotChanged;
		return false;
	}
}

bool Transaction::change(const std::string &user, const std::string &password, std::string *why,
                         const Ask *from)
{
	if (why != NULL)
		why->clear();
	if (held_ == NULL || refused_)
	{
		if (why != NULL)
			*why = kNotChanged;
		return false;
	}
	try
	{
		return changeHeld(user, password, why, from, keep_, ended_);
	}
	catch (...)
	{
		if (why != NULL)
			*why = kNotChanged;
		return false;
	}
}

bool change(const std::string &user, const std::string &password, std::string *why)
{
	Transaction account;
	return account.change(user, password, why);
}

} // namespace login_bridge
