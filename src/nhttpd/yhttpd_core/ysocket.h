//=============================================================================
// YHTTPD
// Socket Class : Basic Socket Operations 
//-----------------------------------------------------------------------------
// Socket-Handler
//=============================================================================
#ifndef __yhttpd_ysocket_h__
#define __yhttpd_ysocket_h__

// system
#include <netinet/in.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>

#include <yconfig.h>
#include "ytypes_globals.h"

#ifdef Y_CONFIG_USE_OPEN_SSL
#include <openssl/ssl.h>
#endif
//-----------------------------------------------------------------------------
// Some common socket definitions
#ifndef SOCKET
#define SOCKET int
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR	(-1)
#endif

//-----------------------------------------------------------------------------
#define NON_BLOCKING_MAX_RETRIES 10	// Retries, if receive buffer is empty
#define NON_BLOCKING_TRY_BYTES 	512	// try to catch min bytes .. else sleep
#define RECEIVE_BLOCK_LEN 	1024	// receiving block length
#define MAX_LINE_BUFFER 	1024*2	// Max length of one line

//-----------------------------------------------------------------------------
class CySocket
{
public:
	// constructor & destructor
			CySocket();
			CySocket(SOCKET s):sock(s){init();};
	virtual 	~CySocket(void);
	void 		init(void);					// some extra initialization
#ifdef Y_CONFIG_USE_OPEN_SSL
	bool 			initAsSSL(void);			// initialize this socket as a SSL-socket
	static bool 		initSSL(void);				// global initialize of SSL-Library, CTX and cerificate/key-file
	static std::string	SSL_pemfile;				// "server.pem" file with SSL-Certificate (now: only one certificate at all)
	static std::string	SSL_CA_file;				// CA Certificate (optional)
#endif
	// Socket handling
	bool		handling;					// true: Socket yet is handled by a Connection Thread
	bool		isOpened;					// is this socket open?
	bool		isValid;					// false on Socket Errors. Must close.
	struct timeval 	tv_start_waiting;				// Put keep-alive Socket to Wait-Queue
	
	void 		close(void);					// Close Socket
	void 		shutdown(void);					// Shutdown Socket
	bool 		listen(int port, int max_connections, const std::string &bindAddress = std::string());		// Listen on Port for max Slave Connections
	CySocket* 	accept();					// Wait for Connection. Returns "slave" Socket
	void 		setAddr(sockaddr_in _clientaddr);
	std::string 	get_client_ip(void);				// Get IP from Client
	std::string 	get_local_authority(void);			// "address:port" this end answers on, empty if unknown
	SOCKET 		get_socket(){return sock;}			// Return "C" Socket-ID
	int		get_accept_port(void);				// Get Port for accepted connection

	// send & receive (basic)
	int 		Read(char *buffer, unsigned int length);	// Read a buffer (normal or SSL)
	int 		Send(char const *buffer, unsigned int length);	// Send a buffer (normal or SSL)
#if 0 //#endif
	bool 		CheckSocketOpen();				// check if socket was closed by client
#endif
	// send & receive
	bool 		SendFile(int filed, off_t start = 0, off_t size = -1); // Send a File
	std::string 	ReceiveBlock();					// receive a Block. Look at length
	unsigned int	ReceiveFileGivenLength(int filed, unsigned int _length); // Receive File of given length
	std::string 	ReceiveLine();					// receive until "\n"
	// Exactly length bytes into out, or false: the peer closed, an error, or
	// timeout_ms passed without them. Bytes beyond length stay on the socket.
	bool		ReceiveExact(std::string &out, size_t length, int timeout_ms);
	// Up to and including the first terminator into out and not one byte
	// more (the socket is peeked, then read exactly), or false: EOF, an
	// error, more than limit bytes without it, or timeout_ms passed.
	bool		ReceiveUntil(const char *terminator, std::string &out, size_t limit, int timeout_ms);
	// length bytes read and dropped, or false: the peer closed, an error, or
	// timeout_ms passed first. A refusal that closes on unread bytes turns
	// into a reset on the way to the peer; this is what makes it readable.
	bool		Discard(size_t length, int timeout_ms);
	// Every byte of data, or false after the peer went away. Send() alone
	// reports a partial write as success.
	bool		SendAll(const char *data, size_t length);
	// The close a server owes a caller whose request it refused without
	// reading the body: the answer leaves behind a FIN, and whatever the
	// caller still sends is read and dropped until it closes, timeout_ms
	// pass or cap bytes went. A plain close on unread bytes becomes a reset,
	// and a reset can take the answer with it.
	void		LingeringClose(int timeout_ms, size_t cap);
	// Set where a request was refused unread; the connection's end then
	// closes by LingeringClose instead of close.
	bool		linger_on_close;
	void		set_stream_flush(void);			// uncork + NODELAY for streamed answers

protected:
	long 		BytesSend;					// Bytes send over Socket
	bool 		set_option(int typ, int option);		// Set Socket Options
	void 		set_reuse_port();				// Set Reuse Port Option for Socket
	void 		set_reuse_addr();				// Set Reuse Address Option for Socket
	void		set_keep_alive();				// Set Keep-Alive Option for Socket
	void		set_tcp_nodelay();

#ifdef Y_CONFIG_USE_OPEN_SSL
	bool		isSSLSocket;					// This is a SSL based Socket
	static SSL_CTX 	*SSL_ctx;					// Global SSL ctx object
	SSL		*ssl;						// ssl habdler for this socket
#endif
private:
	std::string	receive_buffer;					// buffer for receive from socket 
	sockaddr_in	addr;						// "slave" Client Socket Data
	socklen_t	addr_len;					// Length of addr struct
	SOCKET		sock;						// "C" Socket-ID
};
#endif // __yhttpd_ysocket_h__
