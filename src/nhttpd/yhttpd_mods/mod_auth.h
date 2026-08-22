//=============================================================================
// YHTTPD
// mod_auth : Authentication
//=============================================================================
#ifndef __yhttpd_mod_auth_h__
#define __yhttpd_mod_auth_h__

#include <ctime>
#include <map>
#include <yhook.h>
class CmAuth: public Cyhook {
public:
	bool authenticate;
	CmAuth() {
		authenticate = false;
		session_timeout = 0;
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

protected:
	bool CheckAuth(CyhookHandler *hh);
	std::string decodeBase64(const char *b64buffer);
	std::string username;
	std::string password;
	std::string no_auth_client;

	// cookie sessions (browser login). The hook object is a singleton
	// while every connection runs in its own thread, so all access to
	// "sessions" is serialized by a mutex in mod_auth.cpp.
	std::map<std::string, time_t> sessions;	// token -> expiry
	int session_timeout;

	bool CheckSession(CyhookHandler *hh);
	std::string CreateSession(void);
	void DropSession(const std::string &token);
	void ClearSessions(void);

	static std::string GetHeader(CyhookHandler *hh, const std::string &name);
	static std::string GetSessionCookie(CyhookHandler *hh);
	static bool IsBrowserNavigation(CyhookHandler *hh);
	static std::string SanitizeNext(std::string next);
	static std::string MintToken(void);
	static std::string UrlEncode(const std::string &s);
};
#endif // __yhttpd_mod_auth_h__
