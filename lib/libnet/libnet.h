#ifndef __libnet__
#define __libnet__

#include <string>

int 	netSetIP(std::string &dev, std::string &ip, std::string &mask, std::string &brdcast );
void	netGetIP(std::string &dev, std::string &ip, std::string &mask, std::string &brdcast );
void	netSetDefaultRoute( std::string &gw );
void	netGetDefaultRoute( std::string &ip );
void	netGetDomainname( std::string &dom );
void	netSetDomainname( std::string &dom );
void	netGetHostname( std::string &host );
void	netSetHostname( std::string &host );
void	netSetNameserver(std::string &ip);
void	netGetNameserver( std::string &ip );
void 	netGetMacAddr(std::string & ifname, unsigned char *);

#endif
