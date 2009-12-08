/**
 * PING module
 *
 * Copyright (C) 2001 Jeffrey Fulmer <jdfulmer@armstrong.com>
 * This file is part of LIBPING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include "ping.h"


#ifndef  EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif /*EXIT_SUCCESS*/
#ifndef  EXIT_FAILURE
# define EXIT_FAILURE 1
#endif /*EXIT_FAILURE*/

#define  MAXPACKET   65535
#define  PKTSIZE     64 
#define  HDRLEN      ICMP_MINLEN
#define  DATALEN     (PKTSIZE-HDRLEN)
#define  MAXDATA     (MAXPKT-HDRLEN-TIMLEN)
#define  DEF_TIMEOUT 5

int   ident = 0;
int   timo  = 2;
int   rrt;
int   sock;

int 
in_checksum( u_short *buf, int len )
{
  register long sum = 0;
  u_short  answer = 0;

  while( len > 1 ){
    sum += *buf++;
    len -= 2;
  }

  if( len == 1 ){
    *( u_char* )( &answer ) = *( u_char* )buf;
    sum += answer;
  }
  sum = ( sum >> 16 ) + ( sum & 0xffff );
  sum += ( sum >> 16 );     
  answer = ~sum;     

  return ( answer );

} 

int
send_ping( const char *host, struct sockaddr_in *taddr )
{
  int len;
  int ss;
  unsigned char buf[ HDRLEN + DATALEN ];
  struct protoent *proto;
  struct hostent  *hp;
  struct icmp     *icp;
  unsigned short  last;

  len = HDRLEN + DATALEN;

  if(( proto = getprotobyname( "icmp" )) == NULL ){
    return -1;
  }
  
  if(( hp = gethostbyname( host )) != NULL ){
    memcpy( &taddr->sin_addr, hp->h_addr_list[0], sizeof( taddr->sin_addr ));
    taddr->sin_port = 0;
    taddr->sin_family = AF_INET;
  }
  else if( inet_aton( host, &taddr->sin_addr ) == 0 ){
    return -1;
  }

  last = ntohl( taddr->sin_addr.s_addr ) & 0xFF;
  if(( last == 0x00 ) || ( last == 0xFF )){
    return -1;
  }

  if(( sock = socket( AF_INET, SOCK_RAW, proto->p_proto )) < 0 ){
#ifdef  DEBUG
  perror( "sock" );
#endif/*DEBUG*/
    return -2;
  }

  icp = (struct icmp *)buf;
  icp->icmp_type  = ICMP_ECHO;
  icp->icmp_code  = 0;
  icp->icmp_cksum = 0;
  icp->icmp_id    = getpid() & 0xFFFF;
  icp->icmp_cksum = in_checksum((u_short *)icp, len );

  if(( ss = sendto( sock, buf, sizeof( buf ), 0, 
     (struct sockaddr*)taddr, sizeof( *taddr ))) < 0 ){
#ifdef  DEBUG
  perror( "sock" );
#endif/*DEBUG*/
    close( sock );
    return -2;
  }
  if( ss != len ){
#ifdef  DEBUG
  perror( "malformed packet" );
#endif/*DEBUG*/
    close( sock );
    return -2;
  }

  return 0;
}

int 
recv_ping( struct sockaddr_in *taddr )
{
  int len;
  socklen_t from;
  int nf, cc;
  unsigned char buf[ HDRLEN + DATALEN ];
  struct icmp        *icp;
  struct sockaddr_in faddr;
  struct timeval to;
  fd_set readset, writeset;

  to.tv_sec = timo / 100000;
  to.tv_usec = ( timo - ( to.tv_sec * 100000 ) ) * 10;

  FD_ZERO( &readset );
  FD_ZERO( &writeset );
  FD_SET( sock, &readset );
  /* we use select to see if there is any activity
     on the socket.  If not, then we've requested an
     unreachable network and we'll time out here. */
  if(( nf = select( sock + 1, &readset, &writeset, NULL, &to )) < 0 ){
#ifdef  DEBUG
  perror( "select" );
#endif/*DEBUG*/
    exit( EXIT_FAILURE );
  }
  if( nf == 0 ){ 
    return -1; 
  }

  len = HDRLEN + DATALEN;
  from = sizeof( faddr ); 

  cc = recvfrom( sock, buf, len, 0, (struct sockaddr*)&faddr, &from );
  if( cc < 0 ){ 
    exit( EXIT_FAILURE );
  }

  icp = (struct icmp *)(buf + HDRLEN + DATALEN );
  if( faddr.sin_addr.s_addr != taddr->sin_addr.s_addr ){
    return 1;
  }
  /*****
  if( icp->icmp_id   != ( getpid() & 0xFFFF )){
    printf( "id: %d\n",  icp->icmp_id );
    return 1; 
  }
  *****/

  return 0;
}

/**
 * elapsed_time
 * returns an int value for the difference
 * between now and starttime in milliseconds.
 */
int
elapsed_time( struct timeval *starttime ){
  struct timeval *newtime;
  int elapsed;
  newtime = (struct timeval*)malloc( sizeof(struct timeval));
  gettimeofday(newtime,NULL);
  elapsed = 0;
  if(( newtime->tv_usec - starttime->tv_usec) > 0 ){
    elapsed += (newtime->tv_usec - starttime->tv_usec)/1000 ;
  }
  else{
    elapsed += ( 1000000 + newtime->tv_usec - starttime->tv_usec ) /1000;
    newtime->tv_sec--;
  }
  if(( newtime->tv_sec - starttime->tv_sec ) > 0 ){
    elapsed += 1000 * ( newtime->tv_sec - starttime->tv_sec );
  }
  free(newtime);
  return( elapsed );
} 

int 
myping( const char *hostname, int t )
{
  int err;
  struct sockaddr_in sa;
  struct timeval mytime;
 
  ident = getpid() & 0xFFFF;

  if( t == 0 ) timo = 2; 
  else         timo = t;

  (void) gettimeofday( &mytime, (struct timezone *)NULL); 
  if(( err = send_ping( hostname, &sa )) < 0 ){
    return err;
  }
  do{
    if(( rrt = elapsed_time( &mytime )) > timo * 1000 ){
      close( sock );
      return 0;
    }
  } while( recv_ping( &sa ));
  close( sock ); 

  return 1;
}

int
pinghost( const char *hostname )
{
  return myping( hostname, 0 );
}

int
pingthost( const char *hostname, int t )
{
  return myping( hostname, t );
}

int
tpinghost( const char *hostname )
{
  int ret;

  if(( ret = myping( hostname, 0 )) > 0 )
    return rrt;
  else
    return ret;
} 

int
tpingthost( const char *hostname, int t )
{
  int ret;

  if(( ret = myping( hostname, t )) > 0 )
    return rrt; 
  else
    return ret;
}
