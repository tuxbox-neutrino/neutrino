//=============================================================================
// YHTTPD
// Socket Class : Basic Socket Operations
//=============================================================================

#include <cstring>
#include <cstdio>
#include <algorithm>

// system
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

// yhttpd
#include <yhttpd.h>
#include "ysocket.h"
#include "ylogging.h"
// system
#ifdef Y_CONFIG_HAVE_SENDFILE
#include <sys/sendfile.h>
#endif
#ifdef Y_CONFIG_USE_OPEN_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

//=============================================================================
// Initialization of static variables
//=============================================================================
#ifdef Y_CONFIG_USE_OPEN_SSL
SSL_CTX *CySocket::SSL_ctx;
std::string CySocket::SSL_pemfile;
std::string CySocket::SSL_CA_file;
#endif

//=============================================================================
// Constructor & Destructor & Initialization
//=============================================================================
CySocket::CySocket() :
	sock(0) {
#ifdef Y_CONFIG_USE_OPEN_SSL
	ssl = NULL;
#endif
	tv_start_waiting.tv_sec = 0;
	tv_start_waiting.tv_usec = 0;
	BytesSend = 0;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	init();
}
//-----------------------------------------------------------------------------
CySocket::~CySocket() {
#ifdef Y_CONFIG_USE_OPEN_SSL
	if(isSSLSocket && ssl != NULL)
	SSL_free(ssl);
#endif
}
//-----------------------------------------------------------------------------
// initialize
//-----------------------------------------------------------------------------
void CySocket::init(void) {
	BytesSend = 0;
	handling = false;
	isOpened = false;
	isValid = true;
	addr_len = sizeof(addr);
	memset(&addr, 0, addr_len);
#ifdef Y_CONFIG_USE_OPEN_SSL
	isSSLSocket = false;
#endif
}
//-----------------------------------------------------------------------------
// Initialize this socket as a SSL-socket
//-----------------------------------------------------------------------------
#ifdef Y_CONFIG_USE_OPEN_SSL

bool CySocket::initAsSSL(void)
{
	isSSLSocket = true;
	if (NULL == (ssl = SSL_new(CySocket::SSL_ctx))) // create SSL-socket
	{
		aprintf("ySocket:SSL Error: Create SSL_new : %s\n", ERR_error_string(ERR_get_error(), NULL) );
		return false;
	}
	SSL_set_accept_state(ssl); // accept connection
	if(1 != (SSL_set_fd(ssl, sock))) // associate socket descriptor
	if (NULL == (ssl = SSL_new(CySocket::SSL_ctx)))
	{
		aprintf("ySocket:SSL Error: Create SSL_new : %s\n", ERR_error_string(ERR_get_error(), NULL) );
		return false;
	}
	return true;
}
//-----------------------------------------------------------------------------
// static: initialize the SSL-Library and the SSL ctx object.
// Read and assoziate the keyfile.
//-----------------------------------------------------------------------------
bool CySocket::initSSL(void)
{
	SSL_load_error_strings(); // Load SSL Error Strings
	SSL_library_init(); // Load SSL Library
	if (0 == RAND_status()) // set Random
	{
		aprintf("ySocket:SSL got no rand\n");
		return false;
	}
	if((SSL_ctx = SSL_CTX_new(SSLv23_server_method())) == NULL) // create master ctx
	{
		aprintf("ySocket:SSL Error: Create SSL_CTX_new : %s\n", ERR_error_string(ERR_get_error(), NULL) );
		return false;
	}
	if(SSL_pemfile == "")
	{
		aprintf("ySocket:SSL Error: no pemfile given\n");
		return false;
	}
	if(SSL_CA_file != "") // have a CA?
	if(1 != SSL_CTX_load_verify_locations(SSL_ctx, SSL_CA_file.c_str(), NULL))
	{
		aprintf("ySocket:SSL Error: %s CA-File:%s\n",ERR_error_string(ERR_get_error(), NULL), SSL_CA_file.c_str());
		return false;
	}
	if(SSL_CTX_use_certificate_file(SSL_ctx, SSL_pemfile.c_str(), SSL_FILETYPE_PEM) < 0)
	{
		aprintf("ySocket:SSL Error: %s PEM-File:%s\n",ERR_error_string(ERR_get_error(), NULL), SSL_pemfile.c_str());
		return false;
	}

	if(SSL_CTX_use_PrivateKey_file(SSL_ctx, SSL_pemfile.c_str(), SSL_FILETYPE_PEM) < 0)
	{
		aprintf("ySocket:SSL Error: Private Keys: %s PEM-File:%s\n",ERR_error_string(ERR_get_error(), NULL), SSL_pemfile.c_str());
		return false;
	}
	if(SSL_CTX_check_private_key(SSL_ctx) != 1)
	{
		aprintf("ySocket:SSL Error: Private Keys not compatile to public keys: %s PEM-File:%s\n",ERR_error_string(ERR_get_error(), NULL), SSL_pemfile.c_str());
		return false;
	}
	return true;
}
#endif
//=============================================================================
// Socket handling
//=============================================================================
void CySocket::close(void) {
	if (sock != 0 && sock != INVALID_SOCKET)
		::close( sock);
#ifndef Y_CONFIG_FEATURE_KEEP_ALIVE
	sock = 0;
#endif
	isOpened = false;
}
//-----------------------------------------------------------------------------
void CySocket::shutdown(void) {
	if (sock != 0 && sock != INVALID_SOCKET)
		::shutdown(sock, SHUT_RDWR);
}
//-----------------------------------------------------------------------------
bool CySocket::listen(int port, int max_connections) {
	if (sock == INVALID_SOCKET)
		return false;

	// set sockaddr for listening
	sockaddr_in sai;
	memset(&sai, 0, sizeof(sai));
	sai.sin_family = AF_INET; // Protocol
	sai.sin_addr.s_addr = htonl(INADDR_ANY); // No Filter
	sai.sin_port = htons(port); // Listening Port

	set_reuse_port(); // Re-Use Port
	set_reuse_addr(); // Re-Use IP
	if (bind(sock, (sockaddr *) &sai, sizeof(sockaddr_in)) != SOCKET_ERROR)
		if (::listen(sock, max_connections) == 0)
			return true;
	return false;
}
//-----------------------------------------------------------------------------
CySocket* CySocket::accept() {
	init();
	SOCKET newSock = ::accept(sock, (sockaddr *) &addr, &addr_len);
	if (newSock == INVALID_SOCKET) {
		dperror("accept: invalid socket\n");
		return NULL;
	}
	CySocket *new_ySocket = new CySocket(newSock);
	if (new_ySocket != NULL) {
		new_ySocket->setAddr(addr);
#ifdef TCP_CORK
		new_ySocket->set_option(IPPROTO_TCP, TCP_CORK);
#else
		set_tcp_nodelay();
#endif
		new_ySocket->isOpened = true;
	}
	//	handling = true;
	return new_ySocket;
}
//-----------------------------------------------------------------------------
std::string CySocket::get_client_ip(void) {
	return inet_ntoa(addr.sin_addr);
}
//-----------------------------------------------------------------------------
int CySocket::get_accept_port(void) {
	return (int) ntohs(addr.sin_port);
}
//-----------------------------------------------------------------------------
void CySocket::setAddr(sockaddr_in _addr) {
	addr = _addr;
}

//-----------------------------------------------------------------------------
// Set Socket Option (return = false = error)
//-----------------------------------------------------------------------------
bool CySocket::set_option(int typ, int option) {
	int on = 1;
	return (setsockopt(sock, typ, option, (char *) &on, sizeof(on)) >= 0);
}

//-----------------------------------------------------------------------------
// Set Re-Use Option for Port.
//-----------------------------------------------------------------------------
void CySocket::set_reuse_port() {
#ifdef SO_REUSEPORT
	if(!set_option(SOL_SOCKET, SO_REUSEPORT))
	dperror("setsockopt(SO_REUSEPORT)\n");
#endif
}

//-----------------------------------------------------------------------------
// Set Re-Use Option for Address.
//-----------------------------------------------------------------------------
void CySocket::set_reuse_addr() {
#ifdef SO_REUSEADDR
	if(!set_option(SOL_SOCKET, SO_REUSEADDR))
	dperror("setsockopt(SO_REUSEADDR)\n");
#endif
}

//-----------------------------------------------------------------------------
// Set Keep-Alive Option for Socket.
//-----------------------------------------------------------------------------
void CySocket::set_keep_alive() {
#ifdef SO_KEEPALIVE
	if(!set_option(SOL_SOCKET, SO_KEEPALIVE))
	dperror("setsockopt(SO_KEEPALIVE)\n");
#endif
}

//-----------------------------------------------------------------------------
// Set Keep-Alive Option for Socket.
//-----------------------------------------------------------------------------
void CySocket::set_tcp_nodelay() {
#ifdef TCP_NODELAY
	if(!set_option(IPPROTO_TCP, TCP_NODELAY))
	dperror("setsockopt(SO_KEEPALIVE)\n");
#endif
}
//=============================================================================
// Send and receive
//=============================================================================
//-----------------------------------------------------------------------------
// Read a buffer (normal or SSL)
//-----------------------------------------------------------------------------
int CySocket::Read(char *buffer, unsigned int length) {
#ifdef Y_CONFIG_USE_OPEN_SSL
	if(isSSLSocket)
	return SSL_read(ssl, buffer, length);
	else
#endif
	return ::read(sock, buffer, length);
}
//-----------------------------------------------------------------------------
// Send a buffer (normal or SSL)
//-----------------------------------------------------------------------------
int CySocket::Send(char const *buffer, unsigned int length) {
	unsigned int len = 0;
#ifdef Y_CONFIG_USE_OPEN_SSL
	if(isSSLSocket)
	len = SSL_write(ssl, buffer, length);
	else
#endif
	len = ::send(sock, buffer, length, MSG_NOSIGNAL);
	if (len > 0)
		BytesSend += len;
	return len;
}
//-----------------------------------------------------------------------------
// Check if Socket was closed by client
//-----------------------------------------------------------------------------
bool CySocket::CheckSocketOpen() {
	char buffer[32] = { 0 };

#ifdef CONFIG_SYSTEM_CYGWIN
	return !(recv(sock, buffer, sizeof(buffer), MSG_PEEK | MSG_NOSIGNAL) == 0);
#else
	return !(recv(sock, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0);
#endif
}

//=============================================================================
// Aggregated Send- and Receive- Operations
//=============================================================================

//-----------------------------------------------------------------------------
// BASIC Send File over Socket for FILE*
// fd is an opened FILE-Descriptor
//-----------------------------------------------------------------------------
bool CySocket::SendFile(int filed, off_t start, off_t size) {
	if (!isValid)
		return false;
	// does not work with SSL !!!
	struct stat st;
	fstat(filed, &st);
	off_t left = st.st_size - start;
	off_t written = 0;
	if (size > -1 && size < left)
		left = size;
#ifdef Y_CONFIG_HAVE_SENDFILE
	while (left > 0) {
		// split sendfile() transfer to smaller chunks to reduce memory-mapping requirements
		if((written = ::sendfile(sock, filed, &start, std::min((off_t) 0x8000000LL, left))) == -1) {
			if (errno != EPIPE)
				perror("sendfile failed");
			if (errno != EINVAL)
		return false;
			break;
		} else {
			BytesSend += written;
			left -= written;
	}
	}
#endif // Y_CONFIG_HAVE_SENDFILE
	if (left > 0) {
		::lseek(filed, start, SEEK_SET);

		char sbuf[65536];
		while (left && (written = read(filed, sbuf, std::min((off_t) sizeof(sbuf), left))) > 0) {
			if (Send(sbuf, written) < 0) {
				if (errno != EPIPE)
					perror("send failed");
				return false;
			}
			BytesSend += written;
			left -= written;
		}
	}

	log_level_printf(9, "<Sock:SendFile>: Bytes:%ld\n", BytesSend);
	return true;
}
//-----------------------------------------------------------------------------
// Receive File over Socket for FILE* filed
// read/write in small blocks (to save memory).
// usind sleep for sync input
// fd is an opened FILE-Descriptor
//-----------------------------------------------------------------------------
//TODO: Write upload Progress Informations into a file
unsigned int CySocket::ReceiveFileGivenLength(int filed, unsigned int _length) {
	unsigned int _readbytes = 0;
	char buffer[RECEIVE_BLOCK_LEN];
	int retries = 0;

	do {
		// check bytes in Socket buffer
		u_long readarg = 0;
#ifdef Y_CONFIG_USE_OPEN_SSL
		if(isSSLSocket)
		readarg = RECEIVE_BLOCK_LEN;
		else
#endif
		{
			if (ioctl(sock, FIONREAD, &readarg) != 0)// How many bytes avaiable on socket?
				break;
			if (readarg > RECEIVE_BLOCK_LEN) // enough bytes to read
				readarg = RECEIVE_BLOCK_LEN; // read only given length
		}
		if (readarg == 0) // nothing to read: sleep
		{
			retries++;
			if (retries > NON_BLOCKING_MAX_RETRIES)
				break;
			sleep(1);
		} else {
			int bytes_gotten = Read(buffer, readarg);

			if (bytes_gotten == -1 && errno == EINTR)// non-blocking
				continue;
			if (bytes_gotten <= 0) // ERROR Code gotten or Conection closed by peer
			{
				isValid = false;
				break;
			}
			_readbytes += bytes_gotten;
			if (write(filed, buffer, bytes_gotten) != bytes_gotten) {
				perror("write file failed\n");
				return 0;
			}
			retries = 0;
			if (bytes_gotten < NON_BLOCKING_TRY_BYTES) // to few bytes gotten: sleep
				sleep(1);
		}
		log_level_printf(8, "Receive Block length:%d all:%d\n", _readbytes,
				_length);
	} while (_readbytes + RECEIVE_BLOCK_LEN < _length);
	return _readbytes;
}
//-----------------------------------------------------------------------------
// read all data avaiable on Socket
//-----------------------------------------------------------------------------
std::string CySocket::ReceiveBlock() {
	std::string result = "";
	char buffer[RECEIVE_BLOCK_LEN];

	if (!isValid || !isOpened)
		return "";
	//	signal(SIGALRM, ytimeout);
	alarm(1);

	while (true) {
		// check bytes in Socket buffer
		u_long readarg = 0;
#ifdef Y_CONFIG_USE_OPEN_SSL
		if(isSSLSocket)
		readarg = RECEIVE_BLOCK_LEN;
		else
#endif
		// determine bytes that can be read
		{
			if (ioctl(sock, FIONREAD, &readarg) != 0)
				break;
			if (readarg == 0) // nothing to read
				break;
			if (readarg > RECEIVE_BLOCK_LEN) // need more loops
				readarg = RECEIVE_BLOCK_LEN;
		}
		// Read data
		int bytes_gotten = Read(buffer, readarg);

		if (bytes_gotten == -1 && errno == EINTR)// non-blocking
			continue;
		if (bytes_gotten <= 0) // ERROR Code gotten or Conection closed by peer
		{
			isValid = false;
			break;
		}
		result.append(buffer, bytes_gotten);
		if ((u_long) bytes_gotten < readarg) // no more bytes
			break;
	}
	alarm(0);
	signal(SIGALRM, SIG_IGN);
	return result;
}

//-----------------------------------------------------------------------------
// Read on line (Ends with LF) or maximum MAX_LINE_BUFFER chars
// Result Contains [CR]LF!
//-----------------------------------------------------------------------------
std::string CySocket::ReceiveLine() {
	char buffer[MAX_LINE_BUFFER];
	int bytes_gotten = 0;
	std::string result = "";

	while (true) {
		// read one char
		if (Read(buffer + bytes_gotten, 1) == 1) {
			if (buffer[bytes_gotten] == '\n')
				break;
		} else {
			isValid = false;
			break;
		}

		if (bytes_gotten < MAX_LINE_BUFFER - 1)
			bytes_gotten++;
		else
			break;
	}
	buffer[++bytes_gotten] = '\0';
	result.assign(buffer, bytes_gotten);

	return result;
}
