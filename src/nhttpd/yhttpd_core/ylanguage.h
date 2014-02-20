//=============================================================================
// YHTTPD
// Language
//=============================================================================
#ifndef __yhttpd_language_h__
#define __yhttpd_language_h__

#include <stdlib.h>
#include <configfile.h>
// yhttpd
#include <yconfig.h>
#include "ytypes_globals.h"
#include "ywebserver.h"

// forward declaration
class CWebserverConnection;

class CLanguage
{
	protected:
		static CLanguage 	*instance;
		CLanguage(void);
		~CLanguage(void);

		static CConfigFile 	*DefaultLanguage;
		static CConfigFile 	*ConfigLanguage;
		static CConfigFile 	*NeutrinoLanguage;

	public:
		// Instance Handling
		static CLanguage *getInstance(void);
		static void deleteInstance(void);

		// Language
		static std::string 	language;
		static std::string	language_dir;

		void setLanguage(std::string _language);
		std::string getLanguage(void) {return language;};
		std::string 		getLanguageDir(void);

		std::string getTranslation(std::string id);
};

#endif /* __yttpd_language_h__ */
