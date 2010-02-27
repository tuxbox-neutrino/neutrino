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
#include "yconfig.h"
#include "ytypes_globals.h"
#include "yhook.h"

//-----------------------------------------------------------------------------
class CmodSendfile : public Cyhook
{
private:
protected:
	int 			OpenFile(CyhookHandler *hh, std::string fullfilename);
	std::string 		GetFileName(CyhookHandler *hh, std::string path, std::string filename);
	std::string 		GetContentType(std::string ext);
	static CStringList	sendfileTypes;
public:
	CmodSendfile(){};
	~CmodSendfile(void){};

	// Hooks
	virtual THandleStatus 	Hook_PrepareResponse(CyhookHandler *hh);
//	virtual THandleStatus 	Hook_SendResponse(CyhookHandler *hh);
	virtual std::string 	getHookName(void) {return std::string("mod_sendfile");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);
};

#endif /* __yhttpd_mod_sendfile_h__ */
