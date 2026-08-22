//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================

#include <cstring>
#include <cstdlib>
#include "mod_auth.h"
#include <helper.h>

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

	if (authenticate) {
		// basic auth has no logout; answering 401 to a top-level
		// navigation is the only way to make browsers drop their
		// cached credentials, so /logout does that unconditionally
		if (hh->UrlData["url"] == "/logout") {
			hh->SetHeader(HTTP_UNAUTHORIZED, "text/html; charset=UTF-8");
			hh->Write(
				"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
				"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
				"<title>yWeb - Logout</title>"
				"<style>body{margin:0;display:flex;min-height:100vh;align-items:center;"
				"justify-content:center;font-family:system-ui,sans-serif;"
				"background:#11191f;color:#dce3e8}main{max-width:24rem;padding:2rem;"
				"text-align:center}h1{font-size:1.3rem}p{color:#8891a4}"
				"a{color:#fd6d13}</style></head><body><main>"
				"<h1>Logged out &mdash; Abgemeldet</h1>"
				"<p>The browser will ask for the login again on the next access.<br>"
				"Der Browser fragt beim n&auml;chsten Zugriff erneut nach der Anmeldung.</p>"
				"<p><a href=\"/\">Log in again &mdash; Erneut anmelden</a></p>"
				"</main></body></html>");
			return HANDLED_ERROR;
		}

		// dont check local calls or calls from NoAuthClient
		// (a leading address prefix, e.g. "192.168.1." — the UIs have
		// always advertised it as a prefix)
		if ((hh->UrlData["clientaddr"]).find(IADDR_LOCAL) > 0
			&& (no_auth_client.empty() || (hh->UrlData["clientaddr"]).compare(0, no_auth_client.length(), no_auth_client) != 0))
		{
			if (!CheckAuth(hh)) {
				hh->SetHeader(HTTP_UNAUTHORIZED, "text/html; charset=UTF-8");
				hh->Write(
					"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
					"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
					"<title>yWeb - Login</title>"
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
	}
	return status;
}

//-----------------------------------------------------------------------------
// HOOK: webserver_readconfig_hook Handler
// This hook ist called from ReadConfig
//-----------------------------------------------------------------------------
THandleStatus CmAuth::Hook_ReadConfig(CConfigFile *Config,
		CStringList &ConfigList) {
	username = Config->getString("mod_auth.username", AUTHUSER);
	password = Config->getString("mod_auth.password", AUTHPASSWORD);
	no_auth_client = Config->getString("mod_auth.no_auth_client", "");
	authenticate = Config->getBool("mod_auth.authenticate", false);
	ConfigList["mod_auth.username"] = username;
	ConfigList["mod_auth.password"] = password;
	ConfigList["mod_auth.no_auth_client"] = no_auth_client;
	ConfigList["mod_auth.authenticate"] = Config->getString(
			"mod_auth.authenticate", "false");
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

