#ifndef __E_STRING__
#define __E_STRING__

const std::string convertLatin1UTF8(const std::string &string);
int isUTF8(const std::string &string);
std::string convertDVBUTF8(const char *data, int len, int table, int tsidonid = 0);
int readEncodingFile();

inline std::string stringDVBUTF8(const std::string &string, int table=0, int tsidonid=0)
{
	return convertDVBUTF8((const char*)string.c_str(), (int)string.length(), table, tsidonid);
}

int getCountryCodeDefaultMapping( const std::string &lang );

#endif // __E_STRING__
