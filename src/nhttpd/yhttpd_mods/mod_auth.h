//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================
#ifndef __yhttpd_mod_auth_h__
#define __yhttpd_mod_auth_h__

#include <yhook.h>
#include <httpd/login_bridge.h>
class CmAuth: public Cyhook {
public:
	bool authenticate;
	CmAuth() {
		authenticate = false;
	}
	;
	~CmAuth() {
	}
	;

	// virtual functions for HookHandler/Hook
	virtual std::string	getHookName(void) {return std::string("mod_auth");}
	virtual std::string	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus	Hook_PrepareResponse(CyhookHandler *hh);
	virtual THandleStatus	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);

	// The account, its sessions and the Basic check live in the REST
	// layer (httpd/login_bridge.h); this hook asks there, and so do the
	// old routes that change the account.
	static login_bridge::Ask AskOf(CyhookHandler *hh);

protected:
	std::string no_auth_client;

	static std::string GetHeader(CyhookHandler *hh, const std::string &name);
	static bool IsBrowserNavigation(CyhookHandler *hh);
	static std::string UrlEncode(const std::string &s);
};
#endif // __yhttpd_mod_auth_h__
