//=============================================================================
// YHTTPD
// CacheManager
//-----------------------------------------------------------------------------
//=============================================================================
#ifndef __yhttpd_mod_cache_h__
#define __yhttpd_mod_cache_h__

// system
#include <pthread.h>
// c++
#include <string>
// yhttpd
#include <yconfig.h>
#include <ytypes_globals.h>
#include <yhook.h>

//-----------------------------------------------------------------------------
typedef struct {
	std::string filename;
	std::string mime_type;
	std::string category;
	time_t created;
} TCache;

typedef std::map<std::string, TCache> TCacheList;

//-----------------------------------------------------------------------------
class CmodCache: public Cyhook {
private:
	static TCacheList 	CacheList;
	std::string 		cache_directory;
	void 				yshowCacheInfo(CyhookHandler *hh);
	void 				yCacheClear(CyhookHandler *hh);
public:
	static 				pthread_mutex_t mutex;

	CmodCache() {
	}
	;
	~CmodCache(void) {
	}
	;

	void 		AddToCache(CyhookHandler *hh, std::string url, std::string content,
					std::string mime_type, std::string cartegory = "none");
	static void RemoveURLFromCache(std::string url);
	static void RemoveCategoryFromCache(std::string category);
	static void DeleteCache(void);

	// virtual functions for HookHandler/Hook
	virtual std::string 	getHookName(void) {return std::string("mod_cache");}
	virtual std::string 	getHookVersion(void) {return std::string("$Revision$");}
	virtual THandleStatus 	Hook_PrepareResponse(CyhookHandler *hh);
	virtual THandleStatus 	Hook_SendResponse(CyhookHandler *hh);
	virtual THandleStatus 	Hook_ReadConfig(CConfigFile *Config, CStringList &ConfigList);
};

#endif /* __yhttpd_mod_cache_h__ */
