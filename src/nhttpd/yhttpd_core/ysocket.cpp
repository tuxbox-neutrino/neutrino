//=============================================================================
// YHTTPD
// Socket Class : Basic Socket Operations
//=============================================================================

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <atomic>

// system
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

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
	linger_on_close = false;
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
	if((SSL_set_fd(ssl, sock)) != 1) // associate socket descriptor
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
	if (RAND_status() == 0) // set Random
	{
		aprintf("ySocket:SSL got no rand\n");
		return false;
	}
	if((SSL_ctx = SSL_CTX_new(SSLv23_server_method())) == NULL) // create master ctx
	{
		aprintf("ySocket:SSL Error: Create SSL_CTX_new : %s\n", ERR_error_string(ERR_get_error(), NULL) );
		return false;
	}
	if(SSL_pemfile.empty())
	{
		aprintf("ySocket:SSL Error: no pemfile given\n");
		return false;
	}
	if(!SSL_CA_file.empty()) // have a CA?
	if(SSL_CTX_load_verify_locations(SSL_ctx, SSL_CA_file.c_str(), NULL) != 1)
	{
		aprintf("ySocket:SSL Error: %s CA-File:%s\n",ERR_error_string(ERR_get_error(), NULL), ylog::text(SSL_CA_file).c_str());
		return false;
	}
	if(SSL_CTX_use_certificate_file(SSL_ctx, SSL_pemfile.c_str(), SSL_FILETYPE_PEM) < 0)
	{
		aprintf("ySocket:SSL Error: %s PEM-File:%s\n",ERR_error_string(ERR_get_error(), NULL), ylog::text(SSL_pemfile).c_str());
		return false;
	}

	if(SSL_CTX_use_PrivateKey_file(SSL_ctx, SSL_pemfile.c_str(), SSL_FILETYPE_PEM) < 0)
	{
		aprintf("ySocket:SSL Error: Private Keys: %s PEM-File:%s\n",ERR_error_string(ERR_get_error(), NULL), ylog::text(SSL_pemfile).c_str());
		return false;
	}
	if(SSL_CTX_check_private_key(SSL_ctx) != 1)
	{
		aprintf("ySocket:SSL Error: Private Keys not compatile to public keys: %s PEM-File:%s\n",ERR_error_string(ERR_get_error(), NULL), ylog::text(SSL_pemfile).c_str());
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
bool CySocket::listen(int port, int max_connections, const std::string &bindAddress) {
	if (sock == INVALID_SOCKET)
		return false;

	// set sockaddr for listening
	sockaddr_in sai;
	memset(&sai, 0, sizeof(sai));
	sai.sin_family = AF_INET; // Protocol
	bool use_any = true;
	if (!bindAddress.empty() && bindAddress != "0.0.0.0" && bindAddress != "*") {
		in_addr custom_addr {};
		if (inet_pton(AF_INET, bindAddress.c_str(), &custom_addr) == 1) {
			sai.sin_addr = custom_addr;
			use_any = false;
		} else {
			hostent *he = gethostbyname(bindAddress.c_str());
			if (he && he->h_addrtype == AF_INET && he->h_length == sizeof(custom_addr)) {
				memcpy(&sai.sin_addr, he->h_addr_list[0], sizeof(custom_addr));
				use_any = false;
			} else {
				fprintf(stderr, "[yhttpd] Invalid bind host '%s'. Falling back to 0.0.0.0.\n", ylog::text(bindAddress).c_str());
			}
		}
	}
	if (use_any)
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
// The address and port the connection came in on, as a client would write them
// to reach this box again: the listening socket may be bound to every address.
std::string CySocket::get_local_authority(void) {
	sockaddr_in local;
	socklen_t len = sizeof(local);
	if (getsockname(sock, (sockaddr *) &local, &len) != 0 || local.sin_family != AF_INET)
		return "";
	char ip[INET_ADDRSTRLEN];
	if (!inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip)))
		return "";
	return std::string(ip) + ":" + std::to_string((int) ntohs(local.sin_port));
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
// A streamed answer must reach the peer the moment it is written: lift the
// cork accept() put on (clearing TCP_CORK also flushes what it held back)
// and stop Nagle from batching the small frames that follow.
//-----------------------------------------------------------------------------
void CySocket::set_stream_flush(void) {
#ifdef TCP_CORK
	int off = 0;
	setsockopt(sock, IPPROTO_TCP, TCP_CORK, (char *) &off, sizeof(off));
#endif
	set_option(IPPROTO_TCP, TCP_NODELAY);
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
#if 0 //never used
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
#endif
//-----------------------------------------------------------------------------
// A clock a stepped system time cannot move: deadlines are measured on it.
//-----------------------------------------------------------------------------
static long long monotonicMillis() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
//-----------------------------------------------------------------------------
// How long a write may wait for a peer that takes nothing, counted from the
// last byte it took, so a download or a stream that keeps moving is never cut,
// however long it runs. The 30 seconds the carried answers give such a peer:
// a player that pauses or a phone on a weak link takes something within them,
// and without connection threads the server waits no longer for one that left.
//-----------------------------------------------------------------------------
static const unsigned kSendIdleDefaultMs = 30000;
static std::atomic<unsigned> send_idle_ms(kSendIdleDefaultMs);
// How often a wait looks at what the peer took.
static const unsigned kSendSliceDefaultMs = 200;
static std::atomic<unsigned> send_slice_ms(kSendSliceDefaultMs);

unsigned CySocket::sendIdleMs() {
	return send_idle_ms.load();
}

void CySocket::setSendIdleMsForTest(unsigned ms) {
	send_idle_ms = (ms == 0) ? kSendIdleDefaultMs : ms;
}

void CySocket::setSendSliceMsForTest(unsigned ms) {
	send_slice_ms = (ms == 0) ? kSendSliceDefaultMs : ms;
}

// A write that moved bytes: the wait for the next one counts from here.
static void sentNow(long long &moved_at, int &queued_was) {
	moved_at = monotonicMillis();
	queued_was = -1;
}

/* The socket says it takes more only once a good part of its buffer is free
   again, which a slow reader takes long to free; what the peer takes shows
   sooner in how much the kernel still holds for it, looked at between slices
   of the wait. Either counts as moving. What it holds is looked at before the
   first slice too, so what the peer takes during that slice counts. */
bool CySocket::waitToSend(long long &moved_at, int &queued_was) {
	if (queued_was < 0) {
		int queued = 0;
		if (ioctl(sock, TIOCOUTQ, &queued) == 0)
			queued_was = queued;
	}
	for (;;) {
		const long long left = (long long) sendIdleMs() - (monotonicMillis() - moved_at);
		if (left <= 0)
			return false;
		struct pollfd p;
		p.fd = sock;
		p.events = POLLOUT;
		p.revents = 0;
		const int ready = poll(&p, 1, (int) std::min(left, (long long) send_slice_ms.load()));
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0)
			return false;
		// writable, or a fault the write then reports
		if (ready > 0)
			return true;
		int queued = 0;
		if (ioctl(sock, TIOCOUTQ, &queued) == 0) {
			if (queued_was >= 0 && queued < queued_was)
				moved_at = monotonicMillis();
			queued_was = queued;
		}
	}
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
	if (fstat(filed, &st) != 0)
		return false;
	off_t left = st.st_size - start;
	off_t written = 0;
	if (size > -1 && size < left)
		left = size;
	/* The socket does not block while the file goes out, as sendfile() has no
	   flag for that: a peer that takes nothing is waited for the way SendAll()
	   waits for one, and the socket blocks again afterwards. */
	struct NonBlocking {
		int fd, was;
		explicit NonBlocking(int f) : fd(f), was(fcntl(f, F_GETFL)) {
			if (was >= 0)
				fcntl(fd, F_SETFL, was | O_NONBLOCK);
		}
		~NonBlocking() {
			if (was >= 0)
				fcntl(fd, F_SETFL, was);
		}
	} nonblocking(sock);
	long long moved_at = 0;
	int queued_was = -1;
	sentNow(moved_at, queued_was);
#ifdef Y_CONFIG_HAVE_SENDFILE
#ifdef Y_CONFIG_USE_OPEN_SSL
	if(!isSSLSocket)
#endif
	while (left > 0) {
		// split sendfile() transfer to smaller chunks to reduce memory-mapping requirements
		if((written = ::sendfile(sock, filed, &start, std::min((off_t) 0x8000000LL, left))) == -1) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (waitToSend(moved_at, queued_was))
					continue;
				isValid = false;
				return false;
			}
			if (errno != EPIPE)
				perror("sendfile failed");
			if (errno != EINVAL)
				return false;
			break;
		} else {
			// EOF before the length the fstat above promised: the file was
			// truncated while it was being sent. Zero forever, not progress;
			// without this the loop spins on it.
			if (written == 0)
				return false;
			BytesSend += written;
			left -= written;
			sentNow(moved_at, queued_was);
		}
	}
#endif // Y_CONFIG_HAVE_SENDFILE
	if (left > 0) {
		::lseek(filed, start, SEEK_SET);

		char sbuf[65536];
		for (;;) {
			written = read(filed, sbuf, std::min((off_t) sizeof(sbuf), left));
			if (written < 0 && errno == EINTR)
				continue;
			if (written < 0)
				return false;
			if (written == 0 || left == 0)
				break;
			// all of what was read, not whatever one send took; Send()
			// inside SendAll() already counts the bytes
			if (!SendAll(sbuf, (size_t) written)) {
				if (errno != EPIPE)
					perror("send failed");
				return false;
			}
			left -= written;
		}
	}

	log_level_printf(9, "<Sock:SendFile>: Bytes:%ld\n", BytesSend);
	return true;
}
//-----------------------------------------------------------------------------
// Exactly length bytes, with a deadline. Read() rather than recv() so an SSL
// socket takes the same path; poll() on the descriptor is what bounds the
// wait, and a body that stops short fails at the deadline instead of holding
// the connection thread for good.
//-----------------------------------------------------------------------------
bool CySocket::ReceiveExact(std::string &out, size_t length, int timeout_ms) {
	out.clear();
	if (length == 0)
		return true;
	if (!isValid || !isOpened)
		return false;
	out.reserve(length);

	const long long started = monotonicMillis();
	char buffer[16384];
	while (out.size() < length) {
		long long remaining = (long long) timeout_ms - (monotonicMillis() - started);
		if (remaining <= 0)
			return false;

		struct pollfd p;
		p.fd = sock;
		p.events = POLLIN;
		p.revents = 0;
		int ready = poll(&p, 1, (int) remaining);
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready <= 0)
			return false;

		size_t want = length - out.size();
		if (want > sizeof(buffer))
			want = sizeof(buffer);
		int got = Read(buffer, (unsigned int) want);
		if (got < 0 && errno == EINTR)
			continue;
		if (got <= 0) {
			isValid = false;
			return false;
		}
		out.append(buffer, (size_t) got);
	}
	return true;
}
//-----------------------------------------------------------------------------
// Every byte, or the reason it could not be: send(2) may take part of a
// buffer and the old callers took that for the whole.
//-----------------------------------------------------------------------------
bool CySocket::SendAll(const char *data, size_t length) {
	size_t done = 0;
	long long moved_at = 0;
	int queued_was = -1;
	sentNow(moved_at, queued_was);
	while (done < length) {
		size_t chunk = length - done;
		if (chunk > (1u << 20))
			chunk = (1u << 20);
		int sent;
#ifdef Y_CONFIG_USE_OPEN_SSL
		if (isSSLSocket)
			sent = Send(data + done, (unsigned int) chunk);
		else
#endif
		{
			// what the socket takes now, the wait for more bounded above
			if (!waitToSend(moved_at, queued_was)) {
				isValid = false;
				return false;
			}
			sent = (int) ::send(sock, data + done, chunk, MSG_NOSIGNAL | MSG_DONTWAIT);
			if (sent > 0)
				BytesSend += sent;
		}
		if (sent < 0) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			isValid = false;
			return false;
		}
		if (sent == 0) {
			isValid = false;
			return false;
		}
		done += (size_t) sent;
		sentNow(moved_at, queued_was);
	}
	return true;
}
//-----------------------------------------------------------------------------
// Up to the terminator and not past it. The socket is peeked and only what
// belongs to this read is taken, so what follows (a body) stays where the
// next reader expects it. Not for an SSL socket, where a peek sees the
// record and not the bytes; SSL is compiled out of this build.
//-----------------------------------------------------------------------------
bool CySocket::ReceiveUntil(const char *terminator, std::string &out, size_t limit, int timeout_ms) {
	out.clear();
	if (!isValid || !isOpened)
		return false;
	const size_t tlen = strlen(terminator);
	if (tlen == 0)
		return true;

	const long long started = monotonicMillis();
	char buffer[4096];
	for (;;) {
		long long remaining = (long long) timeout_ms - (monotonicMillis() - started);
		if (remaining <= 0)
			return false;
		struct pollfd p;
		p.fd = sock;
		p.events = POLLIN;
		p.revents = 0;
		int ready = poll(&p, 1, (int) remaining);
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready <= 0)
			return false;

		ssize_t peeked = recv(sock, buffer, sizeof(buffer), MSG_PEEK);
		if (peeked < 0 && errno == EINTR)
			continue;
		if (peeked <= 0) {
			isValid = false;
			return false;
		}

		// the terminator may straddle what was kept and what was peeked
		const size_t kept = out.size();
		const size_t back = (kept >= tlen - 1) ? tlen - 1 : kept;
		std::string window = out.substr(kept - back) + std::string(buffer, (size_t) peeked);
		const size_t hit = window.find(terminator);
		size_t take;
		if (hit != std::string::npos)
			take = hit + tlen - back;
		else
			take = (size_t) peeked;
		if (kept + take > limit)
			return false;

		size_t done = 0;
		while (done < take) {
			ssize_t n = recv(sock, buffer, take - done, 0);
			if (n < 0 && errno == EINTR)
				continue;
			if (n <= 0) {
				isValid = false;
				return false;
			}
			out.append(buffer, (size_t) n);
			done += (size_t) n;
		}
		if (hit != std::string::npos)
			return true;
	}
}
//-----------------------------------------------------------------------------
// Read and drop length bytes under a deadline.
//-----------------------------------------------------------------------------
void CySocket::LingeringClose(int timeout_ms, size_t cap) {
	if (isValid && isOpened && sock != 0 && sock != INVALID_SOCKET) {
		::shutdown(sock, SHUT_WR);
		const long long started = monotonicMillis();
		char buffer[16384];
		size_t dropped = 0;
		while (dropped < cap) {
			long long remaining = (long long) timeout_ms - (monotonicMillis() - started);
			if (remaining <= 0)
				break;
			struct pollfd p;
			p.fd = sock;
			p.events = POLLIN;
			p.revents = 0;
			int ready = poll(&p, 1, (int) remaining);
			if (ready < 0 && errno == EINTR)
				continue;
			if (ready <= 0)
				break;
			int got = Read(buffer, sizeof(buffer));
			if (got < 0 && errno == EINTR)
				continue;
			if (got <= 0)
				break; // the caller closed its side, or the connection broke
			dropped += (size_t) got;
		}
	}
	close();
}
//-----------------------------------------------------------------------------
bool CySocket::Discard(size_t length, int timeout_ms) {
	if (length == 0)
		return true;
	if (!isValid || !isOpened)
		return false;
	const long long started = monotonicMillis();
	char buffer[16384];
	size_t done = 0;
	while (done < length) {
		long long remaining = (long long) timeout_ms - (monotonicMillis() - started);
		if (remaining <= 0)
			return false;
		struct pollfd p;
		p.fd = sock;
		p.events = POLLIN;
		p.revents = 0;
		int ready = poll(&p, 1, (int) remaining);
		if (ready < 0 && errno == EINTR)
			continue;
		if (ready <= 0)
			return false;
		size_t want = length - done;
		if (want > sizeof(buffer))
			want = sizeof(buffer);
		int got = Read(buffer, (unsigned int) want);
		if (got < 0 && errno == EINTR)
			continue;
		if (got <= 0) {
			isValid = false;
			return false;
		}
		done += (size_t) got;
	}
	return true;
}
