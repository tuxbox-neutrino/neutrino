//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================
#ifndef __yhttpd_mod_auth_h__
#define __yhttpd_mod_auth_h__

#include "yhook.h"
class CmAuth: public Cyhook {
public:
	bool authenticate;
	CmAuth() {
	}
	;
	~CmAuth() {
	}
	;

	// Hooks
	virtual THandleStatus Hook_PrepareResponse(CyhookHandler *hh);
	virtual std::string getHookName(void) {
		return std::string("mod_auth");
	}
	virtual std::string getHookVersion(void) {
		return std::string("$Revision$");
	}
	virtual THandleStatus Hook_ReadConfig(CConfigFile *Config,
			CStringList &ConfigList);
protected:
	bool CheckAuth(CyhookHandler *hh);
	std::string decodeBase64(const char *b64buffer);
	std::string username;
	std::string password;
	std::string no_auth_client;
};
#endif // __yhttpd_mod_auth_h__
