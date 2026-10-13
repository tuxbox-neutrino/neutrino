//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================

#include <cctype>
#include <cstring>
#include <cstdlib>
#include "mod_auth.h"
#include <helper.h>

//-----------------------------------------------------------------------------
// HOOK: response_hook
//-----------------------------------------------------------------------------
THandleStatus CmAuth::Hook_PrepareResponse(CyhookHandler *hh) {
	THandleStatus status = HANDLED_CONTINUE;

	// The API surface carries its own gate (src/httpd: the one account in
	// nhttpd-access.conf, its sessions and tokens, and the lan-read rule); this
	// module asks the same account through login_bridge. Basic in front of
	// the API would put a
	// second password prompt before /api/v1/login and keep every browser in
	// a popup no page can style. /control and the pages stay behind this
	// module as they always were. The same predicate the request reader
	// and the bridge classify with, so the three cannot disagree about
	// what is an API target.
	if (CyhookHandler::isApiTarget(hh->UrlData["url"]))
		return HANDLED_CONTINUE;

	// With the page login switched off the pages are open, yet a change
	// through the REST layer still asks for a sign-in and opens a session.
	// /logout ends that one too, and then has nothing else to do.
	if (!authenticate) {
		if (hh->UrlData["url"] == "/logout") {
			login_bridge::closeSessionOf(GetHeader(hh, "Cookie"));
			hh->HookVarList["Set-Cookie"] = login_bridge::forgetSessionCookie();
			hh->SendRedirect("/");
			return HANDLED_REDIRECTION;
		}
		return status;
	}

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

	// /login runs for every client, exempt included, so an old bookmark
	// or a form cached before the login moved to POST /api/v1/login
	// lands on the page instead of a 404
	if (url == "/login") {
		hh->SendRedirect("/login.yhtm");
		return HANDLED_REDIRECTION;
	}

	if (url == "/logout") {
		// close the session (if one is presented) and always tell the
		// browser to forget the cookie, whatever else happens below
		login_bridge::closeSessionOf(GetHeader(hh, "Cookie"));
		hh->HookVarList["Set-Cookie"] = login_bridge::forgetSessionCookie();

		// a pure cookie session in a browser logs out deterministically;
		// anything carrying Basic credentials still needs the 401 below —
		// answering 401 to a top-level navigation is the only way to make
		// browsers drop their cached credentials, otherwise the Basic check
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
		const login_bridge::Ask ask = AskOf(hh);
		if (!login_bridge::sessionAdmits(ask) && !login_bridge::basicAdmits(ask)) {
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
	no_auth_client = Config->getString("mod_auth.no_auth_client", "");
	authenticate = Config->getBool("mod_auth.authenticate", false);
	ConfigList["mod_auth.no_auth_client"] = no_auth_client;
	ConfigList["mod_auth.authenticate"] = Config->getString(
			"mod_auth.authenticate", "false");

	// The account lives in nhttpd-access.conf, hashed, and was carried there from
	// this file at the first start. A plain copy left here is one nothing
	// reads and anybody who can read the file can, so it goes once the
	// account is there; the cookie lifetime went with the old sessions.
	if (login_bridge::passwordSet()) {
		bool dropped = Config->deleteKey("mod_auth.password");
		dropped = Config->deleteKey("mod_auth.username") || dropped;
		dropped = Config->deleteKey("mod_auth.session_timeout") || dropped;
		if (dropped)
			Config->saveConfig(HTTPD_CONFIGFILE);
	}
	return HANDLED_CONTINUE;
}

//-----------------------------------------------------------------------------
// what the REST layer's login needs to know about this request
//-----------------------------------------------------------------------------
login_bridge::Ask CmAuth::AskOf(CyhookHandler *hh) {
	login_bridge::Ask a;
	a.path = hh->UrlData["path"];
	a.peer = hh->UrlData["clientaddr"];
	a.cookie = GetHeader(hh, "Cookie");
	a.authorization = GetHeader(hh, "Authorization");
	a.sec_fetch_site = GetHeader(hh, "Sec-Fetch-Site");
	a.origin = GetHeader(hh, "Origin");
	a.referer = GetHeader(hh, "Referer");
	a.host = GetHeader(hh, "Host");
	CStringList::const_iterator execute = hh->ParamList.find("execute");
	a.runs_commands = execute != hh->ParamList.end() && !execute->second.empty();
	return a;
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
