//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include "mod_auth.h"
#include <helper.h>

#define SESSION_COOKIE_NAME	"ysid"
#define SESSION_MAX		64

static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

//-----------------------------------------------------------------------------
// HOOK: response_hook
//-----------------------------------------------------------------------------
THandleStatus CmAuth::Hook_PrepareResponse(CyhookHandler *hh) {
	THandleStatus status = HANDLED_CONTINUE;

	// The API surface carries its own gate (src/httpd: sessions, tokens and
	// the lan-read rule out of ni-web.conf, whose first write is migrated
	// from this module's own credentials). Basic in front of it would put a
	// second password prompt before /api/v1/login and keep every browser in
	// a popup no page can style. /control and the pages stay behind this
	// module as they always were. The same predicate the request reader
	// and the bridge classify with, so the three cannot disagree about
	// what is an API target.
	if (CyhookHandler::isApiTarget(hh->UrlData["url"]))
		return HANDLED_CONTINUE;

	if (!authenticate)
		return status;

	// local calls and NoAuthClient matches (a leading address
	// prefix, e.g. "192.168.1." — the UIs have always advertised
	// it as a prefix) never see the login
	const std::string &clientaddr = hh->UrlData["clientaddr"];
	bool exempt = (clientaddr.find(IADDR_LOCAL) == 0)
		|| (!no_auth_client.empty() && clientaddr.compare(0, no_auth_client.length(), no_auth_client) == 0);

	const std::string &url = hh->UrlData["url"];

	// the login flow's own resources must work without credentials,
	// but never through a traversal: analyzeURL matches the
	// percent-decoded URL and mod_sendfile does not sanitize "..",
	// so "/css/../…" must not become an unauthenticated file read.
	// The template parser runs an `execute` parameter (and prints its
	// trace for `debug`) before it renders a page, and before it even
	// looks whether the file exists: a request carrying either is
	// never open, and under the asset prefixes no template is.
	const std::string &ext = hh->UrlData["fileext"];
	const bool runs_template = hh->ParamList.find("execute") != hh->ParamList.end()
		|| hh->ParamList.find("debug") != hh->ParamList.end();
	const bool template_file = ext == "yhtm" || ext == "yjs" || ext == "ysh";
	bool authfree = false;
	if (url.find("..") == std::string::npos && !runs_template)
		authfree = url == "/login.yhtm"
			|| url == "/favicon.ico"
			|| (!template_file && url.compare(0, 5, "/css/") == 0)
			|| (!template_file && url.compare(0, 9, "/favicon/") == 0);

	// the /login handlers run for every client, exempt included —
	// otherwise localhost could not submit the form and GET /login
	// would run into a 404
	if (url == "/login") {
		if (hh->Method == M_POST) {
			std::string next = SanitizeNext(hh->ParamList["next"]);
			if (hh->ParamList["username"] == username
					&& hh->ParamList["password"] == password) {
				std::string token = CreateSession();
				// an empty token means /dev/urandom failed; deny the
				// login rather than fall back to weak randomness
				if (!token.empty()) {
					hh->HookVarList["Set-Cookie"] = SESSION_COOKIE_NAME "="
						+ token + "; Path=/; HttpOnly; SameSite=Lax; Max-Age="
						+ string_printf("%d", session_timeout);
					hh->SendRedirect(next);
					return HANDLED_REDIRECTION;
				}
			}
			hh->SendRedirect("/login.yhtm?err=1&next=" + UrlEncode(next));
			return HANDLED_REDIRECTION;
		}
		hh->SendRedirect("/login.yhtm");
		return HANDLED_REDIRECTION;
	}

	if (url == "/logout") {
		// drop the session (if one is presented) and always tell the
		// browser to forget the cookie, whatever else happens below
		DropSession(GetSessionCookie(hh));
		hh->HookVarList["Set-Cookie"] = SESSION_COOKIE_NAME "=; Path=/; Max-Age=0";

		// a pure cookie session in a browser logs out deterministically;
		// anything carrying Basic credentials still needs the 401 below —
		// answering 401 to a top-level navigation is the only way to make
		// browsers drop their cached credentials, otherwise CheckAuth
		// would log them right back in.
		// An exempt client gets told why logging out changes nothing
		// for it; the concrete exception list is never disclosed here
		// (this page is reachable without credentials).
		if (!exempt && GetHeader(hh, "Authorization").empty()
				&& IsBrowserNavigation(hh)) {
			hh->SendRedirect("/login.yhtm?out=1");
			return HANDLED_REDIRECTION;
		}
		// the exempt explanation page is purely informational — there
		// are no credentials to drop, and a 401 would make the browser
		// pop its native login dialog first (BuildHeader attaches the
		// Basic challenge to every 401)
		hh->SetHeader(exempt ? HTTP_OK : HTTP_UNAUTHORIZED,
				"text/html; charset=UTF-8");
		hh->Write(
			"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
			"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
			"<title>Neutrino-Web-UI - Logout</title>"
			"<style>body{margin:0;display:flex;min-height:100vh;align-items:center;"
			"justify-content:center;font-family:system-ui,sans-serif;"
			"background:#11191f;color:#dce3e8}main{max-width:26rem;padding:2rem;"
			"text-align:center}h1{font-size:1.3rem}p{color:#8891a4}"
			"a{color:#fd6d13}small{color:#667}</style></head><body><main>");
		if (exempt) {
			hh->Write(
				"<h1>No login required &mdash; Kein Login erforderlich</h1>"
				"<p>This device is exempt (local access or configured address"
				" range), the web interface opens without a login &mdash;"
				" logging out has no effect here. Change this under Settings"
				" &rarr; Webserver.<br>"
				"Dieses Ger&auml;t ist ausgenommen (lokaler Zugriff oder"
				" konfigurierter Adressbereich); das Webinterface &ouml;ffnet"
				" ohne Anmeldung &mdash; Abmelden hat hier keine Wirkung."
				" &Auml;nderbar unter Einstellungen &rarr; Webserver.</p>"
				"<p><a href=\"/\">Continue &mdash; Weiter</a></p>");
		} else {
			hh->Write(
				"<h1>Logged out &mdash; Abgemeldet</h1>"
				"<p>The browser will ask for the login again on the next access.<br>"
				"Der Browser fragt beim n&auml;chsten Zugriff erneut nach der Anmeldung.</p>"
				// must navigate away — reloading /logout would 401 forever;
				// the unique query defeats the cached /
				"<p><a href=\"/\" onclick=\"this.href='/?r='+Date.now()\">"
				"Log in again &mdash; Erneut anmelden</a></p>"
				"<p><small>Devices on the exception list keep access without a login"
				" (Settings &rarr; Webserver).<br>Ger&auml;te auf der Ausnahmeliste"
				" behalten Zugriff ohne Anmeldung (Einstellungen &rarr; Webserver)."
				"</small></p>");
		}
		hh->Write("</main></body></html>");
		return HANDLED_ERROR;
	}

	if (!exempt && !authfree)
	{
		if (!CheckSession(hh) && !CheckAuth(hh)) {
			// browsers get the styled login page; scripts and API
			// clients keep the unchanged Basic-auth 401 path
			if (IsBrowserNavigation(hh)) {
				hh->SendRedirect("/login.yhtm?next="
					+ UrlEncode(hh->UrlData["fullurl"]));
				return HANDLED_REDIRECTION;
			}
			// minimal body against the blank-page dead end for the
			// remaining Basic/script corner cases
			hh->SetHeader(HTTP_UNAUTHORIZED, "text/html; charset=UTF-8");
			hh->Write(
				"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
				"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
				"<title>Neutrino-Web-UI - Login</title>"
				"<style>body{margin:0;display:flex;min-height:100vh;align-items:center;"
				"justify-content:center;font-family:system-ui,sans-serif;"
				"background:#11191f;color:#dce3e8}main{max-width:24rem;padding:2rem;"
				"text-align:center}h1{font-size:1.3rem}p{color:#8891a4}"
				"a{color:#fd6d13}</style></head><body><main>"
				"<h1>Login required &mdash; Anmeldung erforderlich</h1>"
				"<p><a href=\"/\">Try again &mdash; Erneut versuchen</a></p>"
				"</main></body></html>");
			status = HANDLED_ERROR;
		}
	}
	return status;
}

//-----------------------------------------------------------------------------
// HOOK: webserver_readconfig_hook Handler
// This hook ist called from ReadConfig
//-----------------------------------------------------------------------------
THandleStatus CmAuth::Hook_ReadConfig(CConfigFile *Config,
		CStringList &ConfigList) {
	std::string old_username = username;
	std::string old_password = password;
	username = Config->getString("mod_auth.username", AUTHUSER);
	password = Config->getString("mod_auth.password", AUTHPASSWORD);
	no_auth_client = Config->getString("mod_auth.no_auth_client", "");
	authenticate = Config->getBool("mod_auth.authenticate", false);
	session_timeout = Config->getInt32("mod_auth.session_timeout",
			AUTH_SESSION_TIMEOUT);
	if (session_timeout <= 0)
		session_timeout = AUTH_SESSION_TIMEOUT;
	// this hook fires on every config save and set-lang reload; only a
	// real credential change may log everybody out
	if (old_username != username || old_password != password)
		ClearSessions();
	ConfigList["mod_auth.username"] = username;
	ConfigList["mod_auth.password"] = password;
	ConfigList["mod_auth.no_auth_client"] = no_auth_client;
	ConfigList["mod_auth.authenticate"] = Config->getString(
			"mod_auth.authenticate", "false");
	ConfigList["mod_auth.session_timeout"] = string_printf("%d",
			session_timeout);
	return HANDLED_CONTINUE;
}

//-----------------------------------------------------------------------------
// check if given username an pssword are valid
//-----------------------------------------------------------------------------
bool CmAuth::CheckAuth(CyhookHandler *hh) {
	if (hh->HeaderList["Authorization"].empty())
		return false;
	std::string encodet = hh->HeaderList["Authorization"].substr(6,
			hh->HeaderList["Authorization"].length() - 6);
	std::string decodet = decodeBase64(encodet.c_str());
	int pos = decodet.find_first_of(':');
	std::string user = decodet.substr(0, pos);
	std::string passwd = decodet.substr(pos + 1, decodet.length() - pos - 1);
	return (user.compare(username) == 0 && passwd.compare(password) == 0);
}

//-----------------------------------------------------------------------------
// case-insensitive request-header lookup: HeaderList keeps the client's
// capitalization and proxys may deliver lowercase names
//-----------------------------------------------------------------------------
std::string CmAuth::GetHeader(CyhookHandler *hh, const std::string &name) {
	for (CStringList::const_iterator it = hh->HeaderList.begin();
			it != hh->HeaderList.end(); ++it)
		if (strcasecmp(it->first.c_str(), name.c_str()) == 0)
			return it->second;
	return "";
}

//-----------------------------------------------------------------------------
// extract the session token from the Cookie request header
//-----------------------------------------------------------------------------
std::string CmAuth::GetSessionCookie(CyhookHandler *hh) {
	std::string cookies = GetHeader(hh, "Cookie");
	static const std::string prefix = SESSION_COOKIE_NAME "=";
	std::string::size_type pos = 0;
	while (pos < cookies.length()) {
		std::string::size_type end = cookies.find(';', pos);
		if (end == std::string::npos)
			end = cookies.length();
		std::string part = trim(cookies.substr(pos, end - pos));
		if (part.compare(0, prefix.length(), prefix) == 0)
			return part.substr(prefix.length());
		pos = end + 1;
	}
	return "";
}

//-----------------------------------------------------------------------------
// a browser navigation accepts text/html — but so do the legacy
// Prototype XHRs, which must keep getting 401 instead of login HTML;
// they mark themselves with X-Requested-With
//-----------------------------------------------------------------------------
bool CmAuth::IsBrowserNavigation(CyhookHandler *hh) {
	if (GetHeader(hh, "Accept").find("text/html") == std::string::npos)
		return false;
	return GetHeader(hh, "X-Requested-With") != "XMLHttpRequest";
}

//-----------------------------------------------------------------------------
// open-redirect guard for the post-login target: only a local absolute
// path survives — exactly one leading '/', no scheme, no backslashes
//-----------------------------------------------------------------------------
std::string CmAuth::SanitizeNext(std::string next) {
	if (next.empty() || next[0] != '/')
		return "/";
	if (next.length() > 1 && (next[1] == '/' || next[1] == '\\'))
		return "/";
	if (next.find('\\') != std::string::npos)
		return "/";
	return next;
}

//-----------------------------------------------------------------------------
// 32 hex characters from /dev/urandom; empty on failure — the caller
// denies the login then, a weak fallback would defeat the session login
// (the in-repo randomString() is unseeded rand() and not usable here)
//-----------------------------------------------------------------------------
std::string CmAuth::MintToken(void) {
	unsigned char raw[16];
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return "";
	size_t got = 0;
	while (got < sizeof(raw)) {
		ssize_t r = read(fd, raw + got, sizeof(raw) - got);
		if (r <= 0) {
			close(fd);
			return "";
		}
		got += r;
	}
	close(fd);
	static const char hexdigit[] = "0123456789abcdef";
	std::string token(2 * sizeof(raw), '0');
	for (size_t i = 0; i < sizeof(raw); i++) {
		token[2 * i] = hexdigit[raw[i] >> 4];
		token[2 * i + 1] = hexdigit[raw[i] & 0x0f];
	}
	return token;
}

//-----------------------------------------------------------------------------
// percent-encode a string for use as a query-parameter value
// (helper.h's encodeString is an HTML encoder, not a URL encoder)
//-----------------------------------------------------------------------------
std::string CmAuth::UrlEncode(const std::string &s) {
	static const char hexdigit[] = "0123456789ABCDEF";
	std::string result;
	for (std::string::size_type i = 0; i < s.length(); i++) {
		unsigned char c = s[i];
		if (isalnum(c) || c == '/' || c == '.' || c == '-' || c == '_' || c == '~')
			result += (char) c;
		else {
			result += '%';
			result += hexdigit[c >> 4];
			result += hexdigit[c & 0x0f];
		}
	}
	return result;
}

//-----------------------------------------------------------------------------
// look up the presented session token; a hit renews the expiry
// (sliding timeout). Expired entries are swept while the lock is held.
//-----------------------------------------------------------------------------
bool CmAuth::CheckSession(CyhookHandler *hh) {
	std::string token = GetSessionCookie(hh);
	if (token.empty())
		return false;
	bool valid = false;
	time_t now = time(NULL);
	pthread_mutex_lock(&session_mutex);
	for (std::map<std::string, time_t>::iterator it = sessions.begin();
			it != sessions.end();)
		if (it->second <= now)
			sessions.erase(it++);
		else
			++it;
	std::map<std::string, time_t>::iterator hit = sessions.find(token);
	if (hit != sessions.end()) {
		hit->second = now + session_timeout;
		valid = true;
	}
	pthread_mutex_unlock(&session_mutex);
	return valid;
}

//-----------------------------------------------------------------------------
// mint and store a new session token; caps the table so a client
// hammering the login cannot grow it without bound
//-----------------------------------------------------------------------------
std::string CmAuth::CreateSession(void) {
	std::string token = MintToken();
	if (token.empty())
		return "";
	time_t now = time(NULL);
	pthread_mutex_lock(&session_mutex);
	if (sessions.size() >= SESSION_MAX) {
		std::map<std::string, time_t>::iterator oldest = sessions.begin();
		for (std::map<std::string, time_t>::iterator it = sessions.begin();
				it != sessions.end(); ++it)
			if (it->second < oldest->second)
				oldest = it;
		sessions.erase(oldest);
	}
	sessions[token] = now + session_timeout;
	pthread_mutex_unlock(&session_mutex);
	return token;
}

//-----------------------------------------------------------------------------
void CmAuth::DropSession(const std::string &token) {
	if (token.empty())
		return;
	pthread_mutex_lock(&session_mutex);
	sessions.erase(token);
	pthread_mutex_unlock(&session_mutex);
}

//-----------------------------------------------------------------------------
void CmAuth::ClearSessions(void) {
	pthread_mutex_lock(&session_mutex);
	sessions.clear();
	pthread_mutex_unlock(&session_mutex);
}

//-----------------------------------------------------------------------------
// decode Base64 buffer to String
//-----------------------------------------------------------------------------
std::string CmAuth::decodeBase64(const char *b64buffer) {
	if(b64buffer==NULL)
		return "";
	char *newString; //shorter then b64buffer
	std::string result;
	if ((newString = (char *) malloc(sizeof(char) * strlen(b64buffer) + 1))
			!= NULL) {
		char *org_newString = newString;
		int i = 0;
		unsigned long c = 0;

		while (*b64buffer) {
			int oneChar = *b64buffer++;
			if (oneChar >= '0' && oneChar <= '9')
				oneChar = oneChar - '0' + 52;
			else if (oneChar >= 'A' && oneChar <= 'Z')
				oneChar = oneChar - 'A';
			else if (oneChar >= 'a' && oneChar <= 'z')
				oneChar = oneChar - 'a' + 26;
			else if (oneChar == '+')
				oneChar = 62;
			else if (oneChar == '/')
				oneChar = 63;
			else if (oneChar == '=')
				oneChar = 0;
			else
				continue;

			c = (c << 6) | oneChar;
			if (++i == 4) {
				*newString++ = (char) (c >> 16);
				*newString++ = (char) (c >> 8);
				*newString++ = (char) c;
				i = 0;
			}
		}
		*newString++ = '\0';
		result = std::string(org_newString);
		free(org_newString);
		return result;
	} else
		return "";
}
