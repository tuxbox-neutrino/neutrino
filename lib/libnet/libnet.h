#ifndef __libnet__
#define __libnet__

#include <string>

int 	netSetIP(std::string &dev, std::string &ip, std::string &mask, std::string &brdcast );
void	netGetIP(std::string &dev, std::string &ip, std::string &mask, std::string &brdcast );
void	netSetDefaultRoute( std::string &gw );
void	netGetDefaultRoute( std::string &ip );
// The interface and gateway of the first IPv4 default route, false for none.
bool	netDefaultRoute(std::string &dev, std::string &gw);
// A default route over IPv4 or IPv6, which is what being online needs.
bool	netHaveDefaultRoute();
// The same over route tables other than the system's.
bool	netDefaultRouteIn(const char *table, std::string &dev, std::string &gw);
bool	netHaveDefaultRouteIn(const char *table, const char *table6);
void	netGetDomainname( std::string &dom );
void	netSetDomainname( std::string &dom );
void	netGetHostname( std::string &host );
void	netSetHostname( std::string &host );
void	netSetNameserver(std::string &ip);
void	netGetNameserver( std::string &ip );
void 	netGetMacAddr(std::string & ifname, unsigned char *);

#endif
