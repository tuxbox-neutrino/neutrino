//=============================================================================
// YHTTPD
// TestHook
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

	// dont check local calls or calls from NoAuthClient
	if (authenticate) {
		if ((hh->UrlData["clientaddr"]).find(IADDR_LOCAL) > 0
			&& (no_auth_client == "" || (hh->UrlData["clientaddr"]).compare(no_auth_client) != 0))
		{
			if (!CheckAuth(hh)) {
				hh->SetError(HTTP_UNAUTHORIZED);
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
	if (hh->HeaderList["Authorization"] == "")
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

