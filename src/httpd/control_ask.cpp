/*
 * control_ask.cpp - the box asks before a browser runs a command of the old interface
 *
 * Copyright (C) 2026 Thilo Graf
 *
 * License: GPL-2.0-or-later
 */

#include "control_ask.h"

#include "credentials.h"
#include "webconfig.h"
#include "ylogging.h"

#include <OpenThreads/Mutex>
#include <OpenThreads/ScopedLock>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace httpd
{

const char kAskTokenField[] = "ask_token";
const char kAskRememberField[] = "control_without_asking";

namespace
{

// The most tokens held at once: a page of another site can have a browser open
// addresses, and every one it opens is a token, so the oldest give way.
const size_t kHandedMax = 32;

/* A token handed out with the page, the request it was handed out for, and
   when: the target as written, the method and the parameters, never a body. */
struct Handed
{
	std::string token;
	std::string target;
	std::string method;
	std::map<std::string, std::string> params;
	time_t at;

	Handed() : at(0) {}
};

struct State
{
	std::deque<Handed> handed;
	time_t clock;

	/* The access file the switch was read from, how it looked then, and what it
	   said: read again only when it changed. */
	std::string switch_path;
	bool        switch_read;
	bool        switch_on;
	struct stat switch_stat;

	State() : clock(0), switch_read(false), switch_on(false)
	{
		std::memset(&switch_stat, 0, sizeof(switch_stat));
	}
};

// Built on first use: nothing in this directory may need a constructor before main.
State &state()
{
	static State s;
	return s;
}

OpenThreads::Mutex &lock()
{
	static OpenThreads::Mutex m;
	return m;
}

// Called with the lock held.
time_t nowLocked()
{
	if (state().clock != 0)
		return state().clock;
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return t.tv_sec;
}

// A clock that went back counts as run out as well.
bool ranOut(const Handed &h, time_t now)
{
	return now < h.at || now - h.at >= kAskSeconds;
}

bool sameFile(const struct stat &a, const struct stat &b)
{
	return a.st_ino == b.st_ino && a.st_dev == b.st_dev && a.st_size == b.st_size &&
	       a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec;
}

std::string handOut(const std::string &target, const std::string &method,
                    const std::map<std::string, std::string> &params)
{
	Handed h;
	h.token = randomToken(16);
	if (h.token.empty())
		return std::string();
	h.target = target;
	h.method = method;
	h.params = params;

	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	State &s = state();
	h.at = nowLocked();
	while (!s.handed.empty() && ranOut(s.handed.front(), h.at))
		s.handed.pop_front();
	s.handed.push_back(h);
	while (s.handed.size() > kHandedMax)
		s.handed.pop_front();
	return h.token;
}

enum Taken
{
	NotHanded,
	RanOut,
	Found
};

// The token's entry, taken out of the table whatever it is found to be.
Taken take(const std::string &token, Handed *out)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	State &s = state();
	const time_t now = nowLocked();
	for (std::deque<Handed>::iterator it = s.handed.begin(); it != s.handed.end(); ++it)
	{
		if (token.empty() || it->token.size() != token.size() ||
		    CRYPTO_memcmp(it->token.data(), token.data(), token.size()) != 0)
			continue;
		const bool late = ranOut(*it, now);
		*out = *it;
		s.handed.erase(it);
		return late ? RanOut : Found;
	}
	return NotHanded;
}

std::string lowered(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char) (out[i] - 'A' + 'a');
	return out;
}

int hexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Whether a character shows nothing, or changes the direction of the text around
   it: the control characters (C0, DEL, C1), the soft hyphen, the Arabic letter
   mark, the zero-width ones, the marks, embeddings, overrides and isolates of
   bidirectional text, the invisible operators and the byte order mark. */
bool unshown(unsigned long cp)
{
	return cp < 0x20 || cp == 0x7f || (cp >= 0x80 && cp <= 0x9f) || cp == 0xad || cp == 0x61c ||
	       (cp >= 0x200b && cp <= 0x200f) || (cp >= 0x202a && cp <= 0x202e) ||
	       (cp >= 0x2060 && cp <= 0x2069) || cp == 0xfeff;
}

/* s as the page may show it: a character unshown() names, and a byte that is no
   part of a well-formed UTF-8 character, shown as a question mark. So a value can
   neither turn the words around it the other way round nor hide what it holds. */
std::string printable(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	size_t i = 0;
	while (i < s.size())
	{
		const unsigned char c = (unsigned char) s[i];
		size_t n = 0;
		unsigned long cp = 0;
		if (c < 0x80)
		{
			n = 1;
			cp = c;
		}
		else if ((c & 0xe0) == 0xc0)
		{
			n = 2;
			cp = c & 0x1f;
		}
		else if ((c & 0xf0) == 0xe0)
		{
			n = 3;
			cp = c & 0x0f;
		}
		else if ((c & 0xf8) == 0xf0)
		{
			n = 4;
			cp = c & 0x07;
		}
		bool whole = n > 0 && i + n <= s.size();
		for (size_t k = 1; whole && k < n; ++k)
		{
			const unsigned char d = (unsigned char) s[i + k];
			whole = (d & 0xc0) == 0x80;
			cp = (cp << 6) | (d & 0x3f);
		}
		// an overlong form or a surrogate is no character either
		if (whole && ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && (cp < 0x10000 || cp > 0x10ffff)) ||
		              (cp >= 0xd800 && cp <= 0xdfff)))
			whole = false;
		if (!whole)
		{
			out += '?';
			++i;
			continue;
		}
		if (unshown(cp))
			out += '?';
		else
			out.append(s, i, n);
		i += n;
	}
	return out;
}

/* A piece of a query as it reads: a plus is a space and an escape is the byte it
   stands for, as nhttpd's reader decodes them. For the page only: what runs is what
   that reader made of the request. */
std::string readable(const std::string &s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); ++i)
	{
		char c = s[i];
		if (c == '+')
			c = ' ';
		else if (c == '%' && i + 2 < s.size() && hexValue(s[i + 1]) >= 0 && hexValue(s[i + 2]) >= 0)
		{
			c = (char) (hexValue(s[i + 1]) * 16 + hexValue(s[i + 2]));
			i += 2;
		}
		out += c;
	}
	return printable(out);
}


std::string html(const std::string &s)
{
	std::string out;
	out.reserve(s.size() + 16);
	for (size_t i = 0; i < s.size(); ++i)
	{
		switch (s[i])
		{
			case '&':  out += "&amp;"; break;
			case '<':  out += "&lt;"; break;
			case '>':  out += "&gt;"; break;
			case '"':  out += "&quot;"; break;
			case '\'': out += "&#39;"; break;
			default:   out += s[i];
		}
	}
	return out;
}

/* The words of the page, in the two languages it is written in. */
struct Words
{
	const char *lang;
	const char *title;
	const char *heading;
	const char *lead;
	const char *address;
	const char *only_if;
	const char *remember;
	const char *risk;
	const char *where;
	const char *run;
	const char *cancel;
	const char *answered;
	const char *to_ui;
	const char *ran;
	const char *not_ran;
	const char *no_answer;
	const char *picture;
	const char *no_script;
	const char *hidden;
	const char *refused;
};

const Words kEnglish = {
	"en",
	"Neutrino-Web-UI - Run this command?",
	"Run this command?",
	"This address runs a command on the box:",
	"Address",
	"Run it only if you opened this address yourself.",
	"Run without asking from now on",
	"A page on another site could then run such commands on the box too.",
	"You can change this again in the web interface under Settings \xE2\x86\x92"
	" Webserver \xE2\x86\x92" " Access (Run commands without asking).",
	"Run",
	"Cancel",
	"The box answered",
	"To the web interface",
	"The box ran the command",
	"The box did not run the command",
	"The box did not answer",
	"The box's answer as a picture",
	"Without JavaScript, Run shows the box's answer as the box sends it.",
	"[hidden]",
	"The box did not run the command. This question was answered already, is older than "
	"5 minutes or belongs to another address, or the answer did not come from the box's own "
	"page. Open the address again to be asked anew.\n",
};

const Words kGerman = {
	"de",
	"Neutrino-Web-UI - Befehl ausf\xC3\xBC" "hren?",
	"Diesen Befehl ausf\xC3\xBC" "hren?",
	"Diese Adresse f\xC3\xBC" "hrt einen Befehl auf der Box aus:",
	"Adresse",
	"Nur ausf\xC3\xBC" "hren, wenn diese Adresse absichtlich ge\xC3\xB6" "ffnet wurde.",
	"K\xC3\xBC" "nftig ohne R\xC3\xBC" "ckfrage ausf\xC3\xBC" "hren",
	"Dann k\xC3\xB6" "nnte auch eine Seite einer anderen Website solche Befehle auf der Box "
	"ausf\xC3\xBC" "hren.",
	"\xC3\x84" "ndern l\xC3\xA4" "sst sich das jederzeit in der Weboberfl\xC3\xA4" "che unter "
	"Einstellungen \xE2\x86\x92" " Webserver \xE2\x86\x92" " Zugriff (\xE2\x80\x9E" "Befehle ohne "
	"R\xC3\xBC" "ckfrage ausf\xC3\xBC" "hren\xE2\x80\x9C" ").",
	"Ausf\xC3\xBC" "hren",
	"Abbrechen",
	"Antwort der Box",
	"Zur Weboberfl\xC3\xA4" "che",
	"Die Box hat den Befehl ausgef\xC3\xBC" "hrt",
	"Die Box hat den Befehl nicht ausgef\xC3\xBC" "hrt",
	"Die Box hat nicht geantwortet",
	"Die Antwort der Box als Bild",
	"Ohne JavaScript zeigt \xE2\x80\x9E" "Ausf\xC3\xBC" "hren\xE2\x80\x9C" " die Antwort der Box so, "
	"wie die Box sie schickt.",
	"[verborgen]",
	"Die Box hat den Befehl nicht ausgef\xC3\xBC" "hrt. Diese R\xC3\xBC" "ckfrage wurde schon "
	"beantwortet, ist \xC3\xA4" "lter als 5 Minuten oder geh\xC3\xB6" "rt zu einer anderen Adresse, "
	"oder die Antwort kam nicht von der Seite der Box. Um neu gefragt zu werden, die Adresse noch "
	"einmal \xC3\xB6" "ffnen.\n",
};

const Words &wordsInEffect()
{
	return webLanguage() == "de" ? kGerman : kEnglish;
}

std::string param(const std::map<std::string, std::string> &p, const char *key)
{
	const std::map<std::string, std::string>::const_iterator it = p.find(key);
	return it == p.end() ? std::string() : it->second;
}

// pattern with its %s replaced by value, a parameter as nhttpd's reader decoded it, whole.
std::string with(const char *pattern, const std::string &value)
{
	const std::string v = printable(value);
	std::string out(pattern);
	const size_t at = out.find("%s");
	if (at != std::string::npos)
		out.replace(at, 2, v);
	return out;
}

bool digits(const std::string &s)
{
	return !s.empty() && s.size() <= 3 && s.find_first_not_of("0123456789") == std::string::npos;
}

/* The command a request runs, in plain words, and false for one this does not
   know. The names of the old interface's dispatch table (controlapi.cpp), under the
   three prefixes that dispatch to it, and the first parameter read as it does. */
bool commandWords(const std::string &path, const std::map<std::string, std::string> &params,
                  bool de, std::string *out)
{
	const size_t slash = path.rfind('/');
	if (slash == std::string::npos)
		return false;
	const std::string dir = path.substr(0, slash + 1);
	if (dir != "/control/" && dir != "/cgi-bin/" && dir != "/api/v4/")
		return false;
	const std::string c = lowered(path.substr(slash + 1));
	const std::string first = param(params, "1");

#define ASK_SAY(en, ger) do { *out = de ? (ger) : (en); return true; } while (0)
#define ASK_SAY_WITH(en, ger, value) do { *out = with(de ? (ger) : (en), value); return true; } while (0)
	if (c == "standby")
	{
		if (first == "on")
			ASK_SAY("Switch the box to standby", "Die Box in den Standby schalten");
		if (first == "off")
			ASK_SAY("Wake the box from standby", "Die Box aus dem Standby holen");
		ASK_SAY("Tell whether the box is in standby", "Melden, ob die Box im Standby ist");
	}
	if (c == "shutdown")
		ASK_SAY("Shut the box down", "Die Box ausschalten");
	if (c == "reboot")
		ASK_SAY("Restart the box", "Die Box neu starten");
	if (c == "restart")
		ASK_SAY("Restart Neutrino", "Neutrino neu starten");
	if (c == "zapto")
	{
		if (first.empty())
			ASK_SAY("Tell which channel is on", "Melden, welcher Kanal l\xC3\xA4" "uft");
		if (first == "getpids" || first == "getallpids" || first == "statusplayback")
			ASK_SAY("Tell about the channel that is on", "Auskunft \xC3\xBC" "ber den laufenden Kanal geben");
		if (first == "stopplayback")
			ASK_SAY("Stop the picture", "Die Wiedergabe anhalten");
		if (first == "startplayback")
			ASK_SAY("Start the picture again", "Die Wiedergabe wieder starten");
		if (first == "stopsectionsd" || first == "startsectionsd")
			return false;
		ASK_SAY("Switch to another channel", "Auf einen anderen Kanal umschalten");
	}
	if (c == "rcem" && !first.empty())
		ASK_SAY_WITH("Press a key of the remote control: %s",
			     "Eine Taste der Fernbedienung dr\xC3\xBC" "cken: %s", first);
	if (c == "rc")
	{
		if (first == "lock")
			ASK_SAY("Lock the remote control", "Die Fernbedienung sperren");
		if (first == "unlock")
			ASK_SAY("Unlock the remote control", "Die Fernbedienung entsperren");
		ASK_SAY("Tell whether the remote control is locked", "Melden, ob die Fernbedienung gesperrt ist");
	}
	if (c == "volume")
	{
		if (first == "mute")
			ASK_SAY("Mute the sound", "Den Ton stumm schalten");
		if (first == "unmute")
			ASK_SAY("Turn the sound back on", "Den Ton wieder einschalten");
		if (first == "status")
			ASK_SAY("Tell whether the sound is muted", "Melden, ob der Ton stumm ist");
		if (digits(first))
			ASK_SAY_WITH("Set the volume to %s", "Die Lautst\xC3\xA4" "rke auf %s stellen", first);
		if (first.empty())
			ASK_SAY("Tell the volume", "Die Lautst\xC3\xA4" "rke melden");
		return false;
	}
	if (c == "message")
		ASK_SAY("Show a message on the TV", "Eine Meldung auf dem Fernseher zeigen");
	if (c == "setmode")
	{
		if (first == "tv")
			ASK_SAY("Switch to TV", "Auf Fernsehen umschalten");
		if (first == "radio")
			ASK_SAY("Switch to radio", "Auf Radio umschalten");
		return false;
	}
	if (c == "exec" && !first.empty())
		ASK_SAY_WITH("Run the script %s on the box", "Das Skript %s auf der Box ausf\xC3\xBC" "hren", first);
	if (c == "startplugin" && !param(params, "name").empty())
		ASK_SAY_WITH("Start the plugin %s", "Das Plugin %s starten", param(params, "name"));
	if (c == "screenshot")
		ASK_SAY("Take a screenshot", "Ein Bildschirmfoto aufnehmen");
	if (c == "reloadsetup")
		ASK_SAY("Read the settings again", "Die Einstellungen neu einlesen");
	if (c == "reloadchannels")
		ASK_SAY("Read the channel lists again", "Die Kanallisten neu einlesen");
	if (c == "reloadplugins")
		ASK_SAY("Look for plugins again", "Die Plugins neu einlesen");
	if (c == "timer")
	{
		const std::string action = param(params, "action");
		if (action == "new")
			ASK_SAY("Add a timer", "Einen Timer anlegen");
		if (action == "modify")
			ASK_SAY("Change a timer", "Einen Timer \xC3\xA4" "ndern");
		if (action == "remove")
			ASK_SAY("Delete a timer", "Einen Timer l\xC3\xB6" "schen");
		return false;
	}
	if (c == "config" && param(params, "action") == "submit")
	{
		if (param(params, "config") == "nhttpd")
			ASK_SAY("Change the settings of the web server",
				"Die Einstellungen des Webservers \xC3\xA4" "ndern");
		ASK_SAY("Change settings of the box", "Einstellungen der Box \xC3\xA4" "ndern");
	}
	if (c == "setbouquet" || c == "savebouquet" || c == "movebouquet" || c == "deletebouquet" ||
	    c == "addbouquet" || c == "renamebouquet" || c == "changebouquet" || c == "updatebouquet")
		ASK_SAY("Change the bouquets", "Die Bouquets \xC3\xA4" "ndern");
#undef ASK_SAY
#undef ASK_SAY_WITH
	return false;
}

/* An address inside a value, as the web log writes one (ylog::url): a user and a
   password in front of the host shown as hidden, and of its query the names only. */
std::string shownUrl(const std::string &v, const Words &w)
{
	const size_t mark = v.find('?');
	std::string head = v.substr(0, mark);
	const size_t scheme = head.find("://");
	if (scheme != std::string::npos)
	{
		const size_t host = scheme + 3;
		const size_t end = head.find_first_of("/#", host);
		const size_t at = head.rfind('@', end);
		if (at != std::string::npos && at >= host)
			head.replace(host, at - host, w.hidden);
	}
	if (mark == std::string::npos)
		return head;
	std::string out = head + "?";
	size_t at = mark + 1;
	for (;;)
	{
		const size_t amp = v.find('&', at);
		const size_t end = (amp == std::string::npos) ? v.size() : amp;
		const size_t eq = v.find('=', at);
		if (eq != std::string::npos && eq < end)
			out += v.substr(at, eq - at) + "=";
		if (amp == std::string::npos)
			break;
		out += '&';
		at = amp + 1;
	}
	return out;
}

/* The path with its query as it reads, whole: the page lets it wrap. The value of a
   parameter whose name says it is a secret is left out, by the rule the web log reads
   names by (ylog::secretParam), and of a value that is an address the parts the web
   log leaves out (shownUrl). The path is the one nhttpd's reader decoded; only the
   query is decoded here. */
std::string shownAddress(const std::string &path, const std::string &query, const Words &w)
{
	std::string out = printable(path);
	if (!query.empty())
	{
		out += '?';
		size_t at = 0;
		bool first = true;
		while (at <= query.size())
		{
			size_t end = query.find('&', at);
			if (end == std::string::npos)
				end = query.size();
			const std::string piece = query.substr(at, end - at);
			at = end + 1;
			if (!first)
				out += '&';
			first = false;
			const size_t eq = piece.find('=');
			if (eq == std::string::npos)
			{
				out += readable(piece);
				continue;
			}
			const std::string name = readable(piece.substr(0, eq));
			const std::string value = readable(piece.substr(eq + 1));
			out += name + "=";
			if (ylog::secretParam(name))
				out += w.hidden;
			else if (value.find("://") != std::string::npos)
				out += shownUrl(value, w);
			else
				out += value;
		}
	}
	return out;
}

/* The page's style: the sign-in page's look, its colours those of the web interface
   (css/webui.css), dark and light by data-theme. */
const char kAskStyle[] =
	":root{color-scheme:dark;--bg:#0e1116;--card:#161b22;--sect:#1c232c;--line:#2a333f;"
	"--text:#e6ebf1;--muted:#9aa7b5;--accent:#74a6f3;--accent-hover:#90b8f5;--on-accent:#0b1220;"
	"--glow:rgba(116,166,243,.15);--shadow:0 2px 8px rgba(0,0,0,.3)}"
	"[data-theme=light]{color-scheme:light;--bg:#eef2f6;--card:#fff;--sect:#f6f8fb;--line:#d3dbe4;"
	"--text:#17202b;--muted:#556372;--accent:#1258c6;--accent-hover:#0f49a2;--on-accent:#fff;"
	"--glow:rgba(18,88,198,.1);--shadow:0 1px 2px rgba(23,32,43,.06),0 2px 8px rgba(23,32,43,.05)}"
	"*{box-sizing:border-box}"
	"html{background:var(--bg)}"
	"body{margin:0;min-height:100vh;min-height:100dvh;display:flex;flex-direction:column;padding:24px;"
	"background:var(--bg) radial-gradient(circle at 12% -10%,var(--glow),transparent 55%);color:var(--text);"
	"font:400 1rem/1.5 system-ui,-apple-system,\"Segoe UI\",Roboto,\"Helvetica Neue\",Arial,sans-serif}"
	"main{margin:auto;width:100%;max-width:30rem;padding:2rem;background:var(--card);border-radius:.6rem;"
	"box-shadow:var(--shadow)}"
	"[data-theme=light] main{border:1px solid var(--line)}"
	".brand{margin:0 0 .25rem;color:var(--muted);font-size:.85rem;font-weight:500}"
	"h1{margin:0 0 1rem;font-size:1.3rem;line-height:1.3}"
	"h2{margin:0 0 .5rem;font-size:1.05rem}"
	"p{margin:0 0 .75rem}"
	".what{font-size:1.1rem;font-weight:600;overflow-wrap:anywhere}"
	".where{color:var(--muted);font-size:.9rem;overflow-wrap:anywhere}"
	"code,pre{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:.85rem}"
	"code{color:var(--text);background:var(--sect);border:1px solid var(--line);border-radius:.3rem;"
	"padding:.05rem .3rem}"
	".note{color:var(--muted);font-size:.9rem}"
	".remember{margin:1.25rem 0;padding:.75rem .9rem;background:var(--sect);border:1px solid var(--line);"
	"border-radius:.4rem}"
	".remember label{display:flex;gap:.6rem;align-items:flex-start;font-weight:500;cursor:pointer}"
	".remember input{flex:none;width:1.15rem;height:1.15rem;margin:.2rem 0 0;accent-color:var(--accent)}"
	".remember p{margin:.4rem 0 0 1.75rem;color:var(--muted);font-size:.85rem}"
	".actions{display:flex;flex-wrap:wrap;gap:.6rem}"
	".actions>*{flex:1 1 8rem}"
	"button,.button{display:inline-flex;align-items:center;justify-content:center;min-height:2.75rem;"
	"margin:0;padding:.55rem 1.1rem;border:1px solid var(--line);border-radius:.4rem;background:var(--sect);"
	"color:var(--text);font:inherit;font-weight:500;text-decoration:none;cursor:pointer}"
	"button.primary{background:var(--accent);border-color:var(--accent);color:var(--on-accent)}"
	"@media (hover:hover){button.primary:hover{background:var(--accent-hover);border-color:var(--accent-hover)}"
	".button:hover{border-color:var(--accent);color:var(--accent)}}"
	"a{color:var(--accent)}"
	"a:focus-visible,button:focus-visible,input:focus-visible,h1:focus-visible{outline:2px solid var(--accent);"
	"outline-offset:2px}"
	"[aria-disabled=true]{opacity:.6;cursor:progress}"
	"pre{margin:0 0 .75rem;padding:.75rem;max-height:50vh;overflow:auto;white-space:pre-wrap;"
	"overflow-wrap:anywhere;background:var(--sect);border:1px solid var(--line);border-radius:.4rem}"
	"img{max-width:100%;height:auto}"
	"[hidden]{display:none!important}"
	"@media (max-width:480px){body{padding:16px}main{padding:1.25rem}}";

/* The page's script: the theme of the web interface before the first paint, the
   line its pages carry; Escape for Cancel; and Run as a fetch of the form, so a
   command that answers nothing worth reading goes on to the start page and any
   other answer stands on the page, under a heading that says what happened. Run is
   not taken in the first 600 ms after the page came into view, so a click or a key
   meant for the page before it does not land on it. */
const char kAskScript[] =
	"try { var m = localStorage.getItem('webui-mode'); if (m !== 'light' && m !== 'dark') m = "
	"(localStorage.getItem('webui-theme') || localStorage.getItem('yweb-theme')) === 'light' || "
	"matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark'; "
	"document.documentElement.setAttribute('data-theme', m); } catch (e) {}\n"
	"document.addEventListener('DOMContentLoaded', function () {\n"
	"\tvar form = document.getElementById('ask-form');\n"
	"\tif (!form) return;\n"
	"\tvar run = document.getElementById('ask-run');\n"
	"\tvar out = document.getElementById('ask-answer');\n"
	"\tvar pre = document.getElementById('ask-answer-text');\n"
	"\tvar title = document.getElementById('ask-title');\n"
	"\tvar note = document.getElementById('ask-note');\n"
	"\tvar after = document.getElementById('ask-after');\n"
	"\tvar busy = false;\n"
	"\tvar since = Date.now();\n"
	"\tvar fresh = function () { since = Date.now(); };\n"
	"\twindow.addEventListener('focus', fresh);\n"
	"\twindow.addEventListener('pageshow', fresh);\n"
	"\tdocument.addEventListener('visibilitychange', function () { if (!document.hidden) fresh(); });\n"
	"\tdocument.addEventListener('keydown', function (e) {\n"
	"\t\tif (e.key === 'Escape' && !busy) { e.preventDefault(); location.href = '/'; }\n"
	"\t});\n"
	"\tvar show = function (said, text, picture) {\n"
	"\t\tform.hidden = true;\n"
	"\t\tnote.hidden = true;\n"
	"\t\ttitle.textContent = form.getAttribute('data-' + said);\n"
	"\t\tdocument.title = 'Neutrino-Web-UI - ' + title.textContent;\n"
	"\t\tif (picture) {\n"
	"\t\t\tvar img = document.createElement('img');\n"
	"\t\t\timg.alt = form.getAttribute('data-picture');\n"
	"\t\t\timg.src = picture;\n"
	"\t\t\tpre.replaceWith(img);\n"
	"\t\t\tout.hidden = false;\n"
	"\t\t} else if (text !== null) {\n"
	"\t\t\tpre.textContent = text;\n"
	"\t\t\tout.hidden = false;\n"
	"\t\t}\n"
	"\t\tafter.hidden = false;\n"
	"\t\ttitle.focus();\n"
	"\t};\n"
	"\tform.addEventListener('submit', function (e) {\n"
	"\t\te.preventDefault();\n"
	"\t\tif (busy || Date.now() - since < 600) return;\n"
	"\t\tbusy = true;\n"
	"\t\trun.setAttribute('aria-disabled', 'true');\n"
	"\t\tfetch(form.action, {method: 'POST', credentials: 'same-origin', cache: 'no-store', redirect: 'manual',\n"
	"\t\t\theaders: {'Content-Type': 'application/x-www-form-urlencoded'},\n"
	"\t\t\tbody: new URLSearchParams(new FormData(form)).toString()})\n"
	"\t\t.then(function (r) {\n"
	"\t\t\tif (r.type === 'opaqueredirect') { location.replace('/'); return; }\n"
	"\t\t\tif ((r.headers.get('Content-Type') || '').toLowerCase().indexOf('image/') === 0)\n"
	"\t\t\t\treturn r.blob().then(function (b) { show(r.ok ? 'ran' : 'notran', null, URL.createObjectURL(b)); });\n"
	"\t\t\treturn r.text().then(function (t) {\n"
	"\t\t\t\tvar plain = t.split(' ').join('').trim().toLowerCase();\n"
	"\t\t\t\tif (r.ok && (plain === '' || plain === 'ok' || plain === '{\"success\":\"true\"}' ||\n"
	"\t\t\t\t\t\tplain === '<success>true</success>')) {\n"
	"\t\t\t\t\tlocation.replace('/');\n"
	"\t\t\t\t\treturn;\n"
	"\t\t\t\t}\n"
	"\t\t\t\tshow(r.ok ? 'ran' : 'notran', t.trim() === '' ? 'HTTP ' + r.status : t, null);\n"
	"\t\t\t});\n"
	"\t\t})\n"
	"\t\t.catch(function () { show('noanswer', null, null); });\n"
	"\t});\n"
	"});\n";

// "sha256-" and the digest of text in base64, the form a policy names an inline element by.
std::string sha256Source(const char *text)
{
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int length = 0;
	if (EVP_Digest(text, std::strlen(text), digest, &length, EVP_sha256(), NULL) != 1)
		return std::string();
	unsigned char coded[4 * ((EVP_MAX_MD_SIZE + 2) / 3) + 1];
	const int n = EVP_EncodeBlock(coded, digest, (int) length);
	if (n <= 0)
		return std::string();
	return "'sha256-" + std::string((const char *) coded, (size_t) n) + "'";
}

} // namespace

bool asksBeforeRunning(const login_bridge::Ask &a)
{
	try
	{
		return login_bridge::needsOwnPage(a) && login_bridge::fromBrowser(a) &&
		       !login_bridge::fromThisBox(a) && !controlWithoutAsking();
	}
	catch (...)
	{
		return true;
	}
}

std::string askingPage(const std::string &target, const std::string &method, const std::string &path,
                       const std::string &query, const std::map<std::string, std::string> &params)
{
	try
	{
		// an address on this box, which the form posts back to
		if (target.compare(0, 1, "/") != 0 || target.compare(0, 2, "//") == 0)
			return std::string();

		const Words &w = wordsInEffect();
		const bool de = (&w == &kGerman);
		std::string name;
		const bool known = commandWords(path, params, de, &name);
		const std::string address = shownAddress(path, query, w);

		const std::string token = handOut(target, method, params);
		if (token.empty())
			return std::string();

		std::string page;
		page.reserve(8192);
		page += "<!DOCTYPE html>\n<html lang=\"";
		page += w.lang;
		page += "\" data-theme=\"dark\">\n<head>\n<meta charset=\"utf-8\">\n"
			"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>";
		page += w.title;
		page += "</title>\n<style>";
		page += kAskStyle;
		page += "</style>\n<script>";
		page += kAskScript;
		page += "</script>\n</head>\n<body>\n<main aria-labelledby=\"ask-title\">\n"
			"<p class=\"brand\">Neutrino-Web-UI</p>\n<h1 id=\"ask-title\" tabindex=\"-1\">";
		page += w.heading;
		page += "</h1>\n<p>";
		page += w.lead;
		page += "</p>\n";
		if (known)
		{
			page += "<p class=\"what\" id=\"ask-what\">" + html(name) + "</p>\n<p class=\"where\">";
			page += w.address;
			page += " <code>" + html(address) + "</code></p>\n";
		}
		else
			page += "<p class=\"what\" id=\"ask-what\"><code>" + html(address) + "</code></p>\n";
		page += "<p class=\"note\" id=\"ask-note\">";
		page += w.only_if;
		/* Without an action: the form goes back to the address the browser opened.
		   Nothing of it is restored from an earlier visit. */
		page += "</p>\n<form id=\"ask-form\" method=\"post\" autocomplete=\"off\" data-ran=\"";
		page += w.ran;
		page += "\" data-notran=\"";
		page += w.not_ran;
		page += "\" data-noanswer=\"";
		page += w.no_answer;
		page += "\" data-picture=\"";
		page += w.picture;
		page += "\">\n<input type=\"hidden\" name=\"";
		page += kAskTokenField;
		page += "\" value=\"" + token + "\">\n<div class=\"remember\">\n<label><input type=\"checkbox\" "
			"id=\"ask-remember\" name=\"";
		page += kAskRememberField;
		page += "\" value=\"true\" aria-describedby=\"ask-risk ask-where\"> <span>";
		page += w.remember;
		page += "</span></label>\n<p id=\"ask-risk\">";
		page += w.risk;
		page += "</p>\n<p id=\"ask-where\">";
		page += w.where;
		page += "</p>\n</div>\n<div class=\"actions\">\n<button type=\"submit\" id=\"ask-run\" class=\"primary\">";
		page += w.run;
		page += "</button>\n<a id=\"ask-cancel\" class=\"button\" href=\"/\">";
		page += w.cancel;
		page += "</a>\n</div>\n</form>\n<section id=\"ask-answer\" aria-labelledby=\"ask-answer-head\" hidden>\n"
			"<h2 id=\"ask-answer-head\">";
		page += w.answered;
		page += "</h2>\n<pre id=\"ask-answer-text\"></pre>\n</section>\n<p id=\"ask-after\" hidden><a href=\"/\">";
		page += w.to_ui;
		page += "</a></p>\n<noscript><p class=\"note\">";
		page += w.no_script;
		page += "</p></noscript>\n</main>\n</body>\n</html>\n";
		return page;
	}
	catch (...)
	{
		return std::string();
	}
}

std::string askingPagePolicy()
{
	try
	{
		return "default-src 'none'; style-src " + sha256Source(kAskStyle) + "; script-src " +
		       sha256Source(kAskScript) + "; connect-src 'self'; img-src 'self' blob:; form-action 'self'; "
		       "base-uri 'none'; frame-ancestors 'none'";
	}
	catch (...)
	{
		return "default-src 'none'; frame-ancestors 'none'";
	}
}

bool confirmedRun(const login_bridge::Ask &a, const std::string &target, const std::string &token,
                  bool remember, std::map<std::string, std::string> *params, std::string *method,
                  std::string *why)
{
	try
	{
		if (why != NULL)
			why->clear();
		// taken first, so a token is spent wherever it was sent from
		Handed h;
		const Taken taken = take(token, &h);
		const char *refused = NULL;
		if (!login_bridge::fromThisBox(a))
			refused = "the confirmation did not come from a page of this box";
		else if (taken == NotHanded)
			refused = "the token was not handed out by this box, or was used already";
		else if (taken == RanOut)
			refused = "the token ran out";
		else if (h.target != target)
			refused = "the token was handed out for another command";
		if (refused != NULL)
		{
			if (why != NULL)
				*why = refused;
			return false;
		}

		if (params != NULL)
			*params = h.params;
		if (method != NULL)
			*method = h.method;
		// a tick from the page's form, and never from the address the page was asked for
		if (remember && h.params.find(kAskRememberField) == h.params.end())
		{
			std::string not_kept;
			setControlWithoutAsking(true, &not_kept);
		}
		return true;
	}
	catch (...)
	{
		if (why != NULL)
			*why = "the confirmation could not be checked";
		return false;
	}
}

std::string askRefusedText()
{
	try
	{
		return wordsInEffect().refused;
	}
	catch (...)
	{
		return kEnglish.refused;
	}
}

bool controlWithoutAsking()
{
	try
	{
		// the file the configuration in effect was read from
		const std::string path = configPath();

		OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
		State &s = state();
		if (path != s.switch_path)
		{
			s.switch_path = path;
			s.switch_read = false;
		}
		if (path.empty())
			return config().control_without_asking;
		/* A file that is gone or cannot be read says nothing, and the least the
		   switch can mean is off: the box asks. Opened every time, so a file that
		   can no longer be read counts as that, which neither its size nor its
		   times say. Its own writes put a whole new file in place in one step, so
		   none of them is ever read half done. */
		struct stat st;
		const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
		if (fd < 0 || ::fstat(fd, &st) != 0)
		{
			if (fd >= 0)
				::close(fd);
			s.switch_read = false;
			return false;
		}
		::close(fd);
		if (!(s.switch_read && sameFile(st, s.switch_stat)))
		{
			bool on = false;
			if (!readControlWithoutAsking(path, on))
			{
				s.switch_read = false;
				return false;
			}
			s.switch_on = on;
			s.switch_stat = st;
			s.switch_read = true;
		}
		return s.switch_on;
	}
	catch (...)
	{
		return false;
	}
}

bool setControlWithoutAsking(bool on, std::string *why)
{
	if (why != NULL)
		why->clear();
	try
	{
		AccountLock account;
		const std::string path = configPath();
		if (path.empty())
		{
			if (why != NULL)
				*why = "no configuration file is in use";
			return false;
		}
		if (writeControlWithoutAsking(path, on))
			return true;
		if (why != NULL)
		{
			const std::vector<std::string> said = configProblems();
			*why = said.empty() ? std::string("the configuration could not be written") : said[said.size() - 1];
		}
		return false;
	}
	catch (...)
	{
		if (why != NULL)
			*why = "the switch could not be written";
		return false;
	}
}

void setAskClockForTest(time_t now)
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	state().clock = now;
}

void forgetAskTokensForTest()
{
	OpenThreads::ScopedLock<OpenThreads::Mutex> held(lock());
	state().handed.clear();
}

} // namespace httpd
