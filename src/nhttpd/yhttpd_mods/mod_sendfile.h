//=============================================================================
// YHTTPD
// SendFile
//-----------------------------------------------------------------------------
//=============================================================================
#ifndef __yhttpd_mod_sendfile_h__
#define __yhttpd_mod_sendfile_h__

// system
#include <pthread.h>
// c++
#include <string>
// yhttpd
#include <yconfig.h>
#include <ytypes_globals.h>
#include <yhook.h>

//-----------------------------------------------------------------------------
class CmodSendfile : public Cyhook
{
private:
protected:
	int 			OpenFile(CyhookHandler *hh, std::string fullfilename);
	std::string 		GetContentType(std::string ext);
	static CStringList	sendfileTypes;
public:
	CmodSendfile(){};
	~CmodSendfile(void){};

	// virtual functions for HookHandler/Hook
	virtual std::string 	getHookName(void) {return std::string("mod_sendfile");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus 	Hook_PrepareResponse(CyhookHandler *hh);
//	virtual THandleStatus 	Hook_SendResponse(CyhookHandler *hh);
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);

	// Called with the descriptor of a file once every gate let it through and
	// before its length is read off it; NULL, as the server runs, for none.
	static void setCheckedForTest(void (*checked)(int fd));
};

#endif /* __yhttpd_mod_sendfile_h__ */
