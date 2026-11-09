//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include "mod_auth.h"
#include <helper.h>
#include <httpd/control_ask.h>
#include <httpd/webconfig.h>
#include <ystatic.h>

// What Confirmed leaves on a request it runs again, for AskOf (Ask::replayed).
static const char kReplayed[] = "mod_auth.replayed";

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
	// /logout ends that one too, and then has nothing else to do. A
	// browser's command of the old interface is asked about first.
	if (!authenticate) {
		if (hh->UrlData["url"] == "/logout") {
			login_bridge::closeSessionOf(GetHeader(hh, "Cookie"));
			hh->HookVarList["Set-Cookie"] = login_bridge::forgetSessionCookie();
			hh->SendRedirect("/");
			return HANDLED_REDIRECTION;
		}
		return AskFirst(hh);
	}

	// local calls and NoAuthClient matches (a leading address
	// prefix, e.g. "192.168.1." — the UIs have always advertised
	// it as a prefix) never see the login, unless a proxy is configured
	// in front of this server: every request then arrives from the
	// proxy's address, the loopback for one on the box, and the address
	// says nothing about who asks. Configured is the REST layer's own
	// reading (login_bridge::proxied): a trusted_proxies line that names
	// proxies, is empty or cannot be read.
	const std::string &clientaddr = hh->UrlData["clientaddr"];
	const bool exempt = !login_bridge::proxied()
		&& ((clientaddr.find(IADDR_LOCAL) == 0)
		|| (!no_auth_client.empty() && clientaddr.compare(0, no_auth_client.length(), no_auth_client) == 0));

	const std::string &url = hh->UrlData["url"];

	// What may be answered without credentials: the files of the web
	// directory itself, which another interface in its place loads before
	// anybody signs in, as a single-page interface must; of this
	// interface its scripts, styles, texts and pictures and the sign-in
	// page, while its other pages wait for the login (pageNeedsLogin), as
	// they did while they were templates. Never through a traversal: analyzeURL
	// matches the percent-decoded URL and mod_sendfile does not sanitize
	// "..". A request carrying an `execute` or a `debug` parameter is never
	// open either: the template parser ran the one and printed its trace
	// for the other before it even looked whether the file existed. That
	// parser is gone and nothing reads either parameter now; the rule
	// stays as a precaution. Which file a request ends up at, and whether that file may go
	// out, is ystatic's: the override and hosted directories, the raw
	// path, templates, scripts, extensions and the spaces other hooks
	// answer stay behind the login. The file checked is the file sent
	// (PublicFd).
	const bool runs_template = hh->ParamList.find("execute") != hh->ParamList.end()
		|| hh->ParamList.find("debug") != hh->ParamList.end();
	bool authfree = false;
	if (url.find("..") == std::string::npos && !runs_template
			&& (hh->Method == M_GET || hh->Method == M_HEAD) && !ystatic::inReservedSpace(url)) {
		const ystatic::Target target = ystatic::resolve(hh);
		int fd = ystatic::openPublic(hh, target);
		if (fd >= 0 && ystatic::pageNeedsLogin(hh, target, fd)) {
			close(fd);
			fd = -1;
		}
		if (fd >= 0) {
			authfree = true;
			hh->dropPublicFd();
			hh->PublicFd = fd;
			hh->HookVarList["sendfile.public"] = target.file;
			hh->HookVarList["sendfile.fallback"] = target.fallback ? "true" : "false";
		}
	}

	// /login runs for every client, exempt included, so an old bookmark
	// or a form cached before the login moved to POST /api/v1/login
	// lands on the page instead of a 404
	if (url == "/login") {
		hh->SendRedirect("/login.html");
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
			hh->SendRedirect("/login.html?out=1");
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
			// A browser held back on a command for where it came from,
			// by its Basic pair or by its session, is asked, as a
			// browser let in without the login is: a 401 would have it
			// ask for the pair and send the same request again, and the
			// sign-in page would ask one that is signed in already.
			if (login_bridge::basicHeldBack(ask) || login_bridge::sessionHeldBack(ask))
				return AskBeforeRunning(hh);
			// browsers get the styled login page; scripts and API
			// clients keep the unchanged Basic-auth 401 path, and so
			// do browsers where the web directory has no login page of
			// its own: sent to a missing or overridden one, they would
			// be sent on to it again and again.
			// Once signed in, the sign-in page opens next, and that request
			// comes from a page of this box, which is all the needsOwnPage
			// rule asks for. So next names a page and never such a target:
			// a link to /control/standby?on that sent the browser here does
			// not run when its user signs in. next is the target as the
			// browser wrote it, escapes and all, so the browser comes back
			// to the request judged here and not to one a decoded %3F or
			// %2F turns into another. The sign-in page checks next again
			// itself (signinSafeNext).
			if (IsBrowserNavigation(hh) && ystatic::ownLoginPage(hh)) {
				if (!login_bridge::needsOwnPage(ask) && ReturnsToPage(url))
					hh->SendRedirect("/login.html?next="
						+ UrlEncode(hh->UrlData["target"]));
				else
					hh->SendRedirect("/login.html");
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
			return HANDLED_ERROR;
		}
	}
	// what goes on, signed in or let in without the login: a browser's
	// command of the old interface is asked about first
	return authfree ? status : AskFirst(hh);
}

//-----------------------------------------------------------------------------
// A request the gate lets go on, before it does (httpd/control_ask.h). A POST
// of the page that asks runs the request that page was handed out for, or is
// refused; a browser's command that shows nothing saying it came from a page
// of this box gets that page; anything else goes on.
//-----------------------------------------------------------------------------
THandleStatus CmAuth::AskFirst(CyhookHandler *hh) {
	const login_bridge::Ask ask = AskOf(hh);
	if (!login_bridge::needsOwnPage(ask))
		return HANDLED_CONTINUE;
	if (hh->Method == M_POST && hh->ParamList.find(httpd::kAskTokenField) != hh->ParamList.end())
		return Confirmed(hh, ask);
	if (!httpd::asksBeforeRunning(ask))
		return HANDLED_CONTINUE;
	return AskBeforeRunning(hh);
}

//-----------------------------------------------------------------------------
// The page's confirmation: the request it asked about runs as it came, its
// parameters and its method, without the form's fields; a refused one runs
// nothing and says why the box did not run it.
//-----------------------------------------------------------------------------
THandleStatus CmAuth::Confirmed(CyhookHandler *hh, const login_bridge::Ask &ask) {
	const CStringList::const_iterator remember = hh->ParamList.find(httpd::kAskRememberField);
	const bool keep = remember != hh->ParamList.end() && remember->second == "true";
	std::map<std::string, std::string> params;
	std::string method, why;
	if (httpd::confirmedRun(ask, hh->UrlData["target"], hh->ParamList[httpd::kAskTokenField], keep,
			&params, &method, &why) && method == "GET") {
		hh->ParamList = params;
		hh->Method = M_GET;
		// it carries the box's own headers, and is no request of a page of the box
		hh->HookVarList[kReplayed] = "true";
		return HANDLED_CONTINUE;
	}
	log_level_printf(1, "mod_auth: a confirmation was refused: %s\n", why.c_str());
	AnswerAsked(hh, "text/plain; charset=UTF-8", httpd::askRefusedText(), false);
	return HANDLED_ERROR;
}

//-----------------------------------------------------------------------------
// A command the box asks about: the page that asks to a browser's navigation
// of its window, and to anything else asked the text that says where to open
// the address, with no token handed out. That is a picture, a frame, a fetch,
// a form, a script's call or a HEAD, which could only take a token nobody
// sees; a request naming a field of the page's own form, which a link could
// fill in for the form; and a change to the account, which is taken only from
// a page of the box that made it.
//-----------------------------------------------------------------------------
THandleStatus CmAuth::AskBeforeRunning(CyhookHandler *hh) {
	const std::string dest = GetHeader(hh, "Sec-Fetch-Dest");
	if (hh->Method == M_GET && IsBrowserNavigation(hh) && (dest.empty() || dest == "document")
			&& hh->ParamList.find(httpd::kAskTokenField) == hh->ParamList.end()
			&& hh->ParamList.find(httpd::kAskRememberField) == hh->ParamList.end()
			&& !login_bridge::changesAccount(hh->UrlData["url"], hh->ParamList)) {
		const std::string page = httpd::askingPage(hh->UrlData["target"], "GET", hh->UrlData["url"],
			hh->UrlData["paramstring"], hh->ParamList);
		if (!page.empty()) {
			AnswerAsked(hh, "text/html; charset=UTF-8", page, true);
			return HANDLED_ERROR;
		}
	}
	AnswerAsked(hh, "text/plain; charset=UTF-8", login_bridge::heldBackText(), false);
	return HANDLED_ERROR;
}

//-----------------------------------------------------------------------------
// The answer in place of a command that did not run: 403 with the head the
// boundary's page goes out with (nosniff, no-store), and for the page that
// asks no frame of any page around it, as its policy says too.
//-----------------------------------------------------------------------------
void CmAuth::AnswerAsked(CyhookHandler *hh, const char *type, const std::string &body, bool page) {
	hh->api_mode = true;
	hh->httpStatus = HTTP_FORBIDDEN;
	hh->ResponseMimeType = type;
	hh->ResponseHeaders.clear();
	hh->ResponseHeaders.push_back(std::make_pair(std::string("X-Content-Type-Options"),
		std::string("nosniff")));
	hh->ResponseHeaders.push_back(std::make_pair(std::string("Cache-Control"),
		std::string("no-store")));
	if (page) {
		hh->ResponseHeaders.push_back(std::make_pair(std::string("X-Frame-Options"),
			std::string("DENY")));
		hh->ResponseHeaders.push_back(std::make_pair(std::string("Content-Security-Policy"),
			httpd::askingPagePolicy()));
	}
	hh->yresult = body;
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
			httpd::saveServerFile(HTTPD_CONFIGFILE, Config->getConfigDataMap());
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
	a.user_agent = GetHeader(hh, "User-Agent");
	CStringList::const_iterator execute = hh->ParamList.find("execute");
	a.runs_commands = execute != hh->ParamList.end() && !execute->second.empty();
	CStringList::const_iterator replayed = hh->HookVarList.find(kReplayed);
	a.replayed = replayed != hh->HookVarList.end() && replayed->second == "true";
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
// Whether a browser may be sent back to url (the decoded path, without the
// query) once it has signed in: a folder, or a name ending in .html or .htm
// in any case, in parts of letters, digits and . _ ~ -, none of them empty or
// starting with a dot, the rule the sign-in page holds next to
// (signinSafeNext). So nothing in it is a step up, a second slash, a
// backslash or an escape the browser would read as another path.
//-----------------------------------------------------------------------------
bool CmAuth::ReturnsToPage(const std::string &url) {
	if (url.empty() || url[0] != '/')
		return false;
	std::string::size_type start = 1;
	while (start < url.length()) {
		const std::string::size_type end = url.find('/', start);
		const std::string part = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (part.empty() || part[0] == '.')
			return false;
		// spelled out, so a locale cannot add letters to it
		for (std::string::size_type i = 0; i < part.length(); i++) {
			const char c = part[i];
			if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')
					&& c != '.' && c != '_' && c != '~' && c != '-')
				return false;
		}
		if (end == std::string::npos) {
			std::string name = part;
			for (std::string::size_type i = 0; i < name.length(); i++)
				name[i] = (char) tolower((unsigned char) name[i]);
			const std::string::size_type dot = name.rfind('.');
			if (dot == std::string::npos)
				return false;
			const std::string ext = name.substr(dot);
			return ext == ".html" || ext == ".htm";
		}
		start = end + 1;
	}
	return true;
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
