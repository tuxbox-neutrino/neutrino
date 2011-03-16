#ifndef __libnet__
#define __libnet__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


extern  int 	netSetIP( char *dev, char *ip, char *mask, char *brdcast );
extern  void	netGetIP( char *dev, char *ip, char *mask, char *brdcast );
extern  void	netSetDefaultRoute( char *gw );
extern  void	netGetDefaultRoute( char *ip );
extern	char	*netGetDomainname( void );
extern	void	netSetDomainname( char *dom );
extern	char	*netGetHostname( void );
extern	void	netSetHostname( char *host );
extern	void	netSetNameserver(const char *ip);
extern  void	netGetNameserver( char *ip );
extern  void 	netGetMacAddr(char * ifname, unsigned char * mac);


#ifdef __cplusplus
}
#endif


#endif
