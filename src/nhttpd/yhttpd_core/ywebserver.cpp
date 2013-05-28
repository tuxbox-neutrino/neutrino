//=============================================================================
// YHTTPD
// Webserver Class : Until now: exact one instance
//=============================================================================
// c++
#include <cerrno>
#include <csignal>

// system
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
// tuxbox
#include <configfile.h>

// yhttpd
#include <yhttpd.h>
#include "ytypes_globals.h"
#include "ywebserver.h"
#include "ylogging.h"
#include "helper.h"
#include "ysocket.h"
#include "yconnection.h"
#include "yrequest.h"

//=============================================================================
// Initialization of static variables
//=============================================================================
bool CWebserver::is_threading = true;
pthread_mutex_t CWebserver::mutex = PTHREAD_MUTEX_INITIALIZER;
;

//=============================================================================
// Constructor & Destructor & Initialization
//=============================================================================
CWebserver::CWebserver() {
	terminate = false;
	for (int i = 0; i < HTTPD_MAX_CONNECTIONS; i++) {
		Connection_Thread_List[i] = (pthread_t) NULL;
		SocketList[i] = NULL;
	}
	FD_ZERO(&master); // initialize FD_SETs
	FD_ZERO(&read_fds);
	fdmax = 0;
	open_connections = 0;
#ifdef Y_CONFIG_BUILD_AS_DAEMON
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
#endif
	port = 80;

}
//-----------------------------------------------------------------------------
CWebserver::~CWebserver() {
	listenSocket.close();
}
//=============================================================================
// Start Webserver. Main-Loop.
//-----------------------------------------------------------------------------
// Wait for Connection and schedule ist to handle_connection()
// HTTP/1.1 should can handle "keep-alive" connections to reduce socket
// creation and handling. This is handled using FD_SET and select Socket
// mechanism.
// select wait for socket-activity. Cases:
//	1) get a new connection
//	2) re-use a socket
//	3) timeout: close unused sockets
//-----------------------------------------------------------------------------
//	from RFC 2616:
//	8 Connections
//	8.1 Persistent Connections
//	8.1.1 Purpose
//
//	   Prior to persistent connections, a separate TCP connection was
//	   established to fetch each URL, increasing the load on HTTP servers
//	   and causing congestion on the Internet. The use of inline images and
//	   other associated data often require a client to make multiple
//	   requests of the same server in a short amount of time. Analysis of
//	   these performance problems and results from a prototype
//	   implementation are available [26] [30]. Implementation experience and
//	   measurements of actual HTTP/1.1 (RFC 2068) implementations show good
//	   results [39]. Alternatives have also been explored, for example,
//	   T/TCP [27].
//
//	   Persistent HTTP connections have a number of advantages:
//
//	      - By opening and closing fewer TCP connections, CPU time is saved
//	        in routers and hosts (clients, servers, proxies, gateways,
//	        tunnels, or caches), and memory used for TCP protocol control
//	        blocks can be saved in hosts.
//
//	      - HTTP requests and responses can be pipelined on a connection.
//	        Pipelining allows a client to make multiple requests without
//	        waiting for each response, allowing a single TCP connection to
//	        be used much more efficiently, with much lower elapsed time.
//
//	      - Network congestion is reduced by reducing the number of packets
//	        caused by TCP opens, and by allowing TCP sufficient time to
//	        determine the congestion state of the network.
//
//	      - Latency on subsequent requests is reduced since there is no time
//	        spent in TCP's connection opening handshake.
//
//	      - HTTP can evolve more gracefully, since errors can be reported
//	        without the penalty of closing the TCP connection. Clients using
//	        future versions of HTTP might optimistically try a new feature,
//	        but if communicating with an older server, retry with old
//	        semantics after an error is reported.
//
//	   HTTP implementations SHOULD implement persistent connections.
//=============================================================================
#define MAX_TIMEOUTS_TO_CLOSE 10
#define MAX_TIMEOUTS_TO_TEST 100
bool CWebserver::run(void) {
	if (!listenSocket.listen(port, HTTPD_MAX_CONNECTIONS)) {
		if (port != 80) {
			fprintf(stderr, "[yhttpd] Socket cannot bind and listen on port %d Abort.\n", port);
			return false;
		}
		fprintf(stderr, "[yhttpd] cannot bind and listen on port 80, retrying on port 8080.\n");
		port = 8080;
		if (!listenSocket.listen(port, HTTPD_MAX_CONNECTIONS)) {
			fprintf(stderr, "[yhttpd] Socket cannot bind and listen on port %d Abort.\n", port);
			return false;
		}
	}
#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE

	// initialize values for select
	int listener = listenSocket.get_socket();// Open Listener
	struct timeval tv; // timeout struct
	FD_SET(listener, &master); // add the listener to the master set
	fdmax = listener; // init max fd
	fcntl(listener, F_SETFD , O_NONBLOCK); // listener master socket non-blocking
	int timeout_counter = 0; // Counter for Connection Timeout checking
	int test_counter = 0; // Counter for Testing long running Connections

	// main Webserver Loop
	while(!terminate)
	{
		// select : init vars
		read_fds = master; // copy it
		tv.tv_usec = 10000; // microsec: Timeout for select ! for re-use / keep-alive socket
		tv.tv_sec = 0; // seconds
		int fd = -1;

		// select : wait for socket activity
		if(open_connections <= 0) // No open Connection. Wait in select.
		fd = select(fdmax+1,&read_fds, NULL, NULL, NULL);// wait for socket activity
		else
		fd = select(fdmax+1,&read_fds, NULL, NULL, &tv);// wait for socket activity or timeout

		// too much to do : sleep
		if(open_connections >= HTTPD_MAX_CONNECTIONS-1)
		sleep(1);

		// Socket Error?
		if(fd == -1 && errno != EINTR)
		{
			perror("select");
			return false;
		}

		// Socket Timeout?
		if(fd == 0)
		{
			// Testoutput for long living threads
			if(++test_counter >= MAX_TIMEOUTS_TO_TEST)
			{
				for(int j=0;j < HTTPD_MAX_CONNECTIONS;j++)
				if(SocketList[j] != NULL) // here is a socket
				log_level_printf(2,"FD-TEST sock:%d handle:%d open:%d\n",SocketList[j]->get_socket(),
						SocketList[j]->handling,SocketList[j]->isOpened);
				test_counter=0;
			}
			// some connection closing previous missed?
			if(++timeout_counter >= MAX_TIMEOUTS_TO_CLOSE)
			{
				CloseConnectionSocketsByTimeout();
				timeout_counter=0;
			}
			continue; // main loop again
		}
		//----------------------------------------------------------------------------------------
		// Check all observed descriptors & check new or re-use Connections
		//----------------------------------------------------------------------------------------
		for(int i = listener; i <= fdmax; i++)
		{
			int slot = -1;
			if(FD_ISSET(i, &read_fds)) // Socket observed?
			{ // we got one!!
				if (i == listener) // handle new connections
				slot = AcceptNewConnectionSocket();
				else // Connection on an existing open Socket = reuse (keep-alive)
				{
					slot = SL_GetExistingSocket(i);
					if(slot>=0)
					log_level_printf(2,"FD: reuse con fd:%d\n",SocketList[slot]->get_socket());
				}
				// prepare Connection handling
				if(slot>=0)
				if(SocketList[slot] != NULL && !SocketList[slot]->handling && SocketList[slot]->isValid)
				{
					log_level_printf(2,"FD: START CON HANDLING con fd:%d\n",SocketList[slot]->get_socket());
					FD_CLR(SocketList[slot]->get_socket(), &master); // remove from master set
					SocketList[slot]->handling = true; // prepares for thread-handling
					if(!handle_connection(SocketList[slot]))// handle this activity
					{ // Can not handle more threads
						char httpstr[]=HTTP_PROTOCOL " 503 Service Unavailable\r\n\r\n";
						SocketList[slot]->Send(httpstr, strlen(httpstr));
						SL_CloseSocketBySlot(slot);
					}
				}
			}
		}// for
		CloseConnectionSocketsByTimeout(); // Check connections to close

	}//while
#else
	while (!terminate) {
		CySocket *newConnectionSock;
		if (!(newConnectionSock = listenSocket.accept())) //Now: Blocking wait
		{
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			pthread_testcancel();
			dperror("Socket accept error. Continue.\n");
			continue;
		}
		log_level_printf(3, "Socket connect from %s\n",
				(listenSocket.get_client_ip()).c_str());
#ifdef Y_CONFIG_USE_OPEN_SSL
		if(Cyhttpd::ConfigList["SSL"]=="true")
		newConnectionSock->initAsSSL(); // make it a SSL-socket
#endif
		handle_connection(newConnectionSock);
	}
#endif
	return true;
}
//=============================================================================
// SocketList Handler
//=============================================================================
//-----------------------------------------------------------------------------
// Accept new Connection
//-----------------------------------------------------------------------------
int CWebserver::AcceptNewConnectionSocket() {
	int slot = -1;
	CySocket *connectionSock = NULL;

	if (!(connectionSock = listenSocket.accept())) // Blocking wait
	{
		dperror("Socket accept error. Continue.\n");
		delete connectionSock;
		return -1;
	}
#ifdef Y_CONFIG_USE_OPEN_SSL
	if(Cyhttpd::ConfigList["SSL"]=="true")
	connectionSock->initAsSSL(); // make it a SSL-socket
#endif
	log_level_printf(2, "FD: new con fd:%d on port:%d\n",
			connectionSock->get_socket(), connectionSock->get_accept_port());

	// Add Socket to List
	slot = SL_GetFreeSlot();
	if (slot < 0) {
		connectionSock->close();
		aprintf("No free Slot in SocketList found. Open:%d\n", open_connections);
	} else {
		SocketList[slot] = connectionSock; // put it to list
		fcntl(connectionSock->get_socket(), F_SETFD, O_NONBLOCK); // set non-blocking
		open_connections++; // count open connectins
		int newfd = connectionSock->get_socket();
		if (newfd > fdmax) // keep track of the maximum fd
			fdmax = newfd;
	}
	return slot;
}

//-----------------------------------------------------------------------------
// Get Index for Socket from SocketList
//-----------------------------------------------------------------------------
int CWebserver::SL_GetExistingSocket(SOCKET sock) {
	int slot = -1;
	for (int j = 0; j < HTTPD_MAX_CONNECTIONS; j++)
		if (SocketList[j] != NULL // here is a socket
				&& SocketList[j]->get_socket() == sock) // we know that socket
		{
			slot = j;
			break;
		}
	return slot;
}
//-----------------------------------------------------------------------------
// Get Index for free Slot in SocketList
//-----------------------------------------------------------------------------
int CWebserver::SL_GetFreeSlot() {
	int slot = -1;
	for (int j = 0; j < HTTPD_MAX_CONNECTIONS; j++)
		if (SocketList[j] == NULL) // here is a free slot
		{
			slot = j;
			break;
		}
	return slot;
}

//-----------------------------------------------------------------------------
// Look for Sockets to close
//-----------------------------------------------------------------------------
void CWebserver::CloseConnectionSocketsByTimeout() {
	CySocket *connectionSock = NULL;
	for (int j = 0; j < HTTPD_MAX_CONNECTIONS; j++)
		if (SocketList[j] != NULL // here is a socket
				&& !SocketList[j]->handling) // it is not handled
		{
			connectionSock = SocketList[j];
			SOCKET thisSocket = connectionSock->get_socket();
			bool shouldClose = true;

			if (!connectionSock->isValid) // If not valid -> close
				; // close
			else if (connectionSock->tv_start_waiting.tv_sec != 0
					|| SocketList[j]->tv_start_waiting.tv_usec != 0) { // calculate keep-alive timeout
				struct timeval tv_now;
				struct timezone tz_now;
				gettimeofday(&tv_now, &tz_now);
				int64_t tdiff = ((tv_now.tv_sec
						- connectionSock->tv_start_waiting.tv_sec) * 1000000
						+ (tv_now.tv_usec
								- connectionSock->tv_start_waiting.tv_usec));
				if (tdiff < HTTPD_KEEPALIVE_TIMEOUT || tdiff < 0)
					shouldClose = false;
			}
			if (shouldClose) {
				log_level_printf(2, "FD: close con Timeout fd:%d\n", thisSocket);
				SL_CloseSocketBySlot(j);
			}
		}
}
//-----------------------------------------------------------------------------
// Add Socket fd to FD_SET again (for select-handling)
// Add start-time for waiting for connection re-use / keep-alive
//-----------------------------------------------------------------------------
void CWebserver::addSocketToMasterSet(SOCKET fd) {
	int slot = SL_GetExistingSocket(fd); // get slot/index for fd
	if (slot < 0)
		return;
	log_level_printf(2, "FD: add to master fd:%d\n", fd);
	struct timeval tv_now;
	struct timezone tz_now;
	gettimeofday(&tv_now, &tz_now);
	SocketList[slot]->tv_start_waiting = tv_now; // add keep-alive wait time
	FD_SET(fd, &master); // add fd to select-master-set
}

//-----------------------------------------------------------------------------
// Close (FD_SET handled) Socket
// Clear it from SocketList
//-----------------------------------------------------------------------------
void CWebserver::SL_CloseSocketBySlot(int slot) {
	open_connections--; // count open connections
	if (SocketList[slot] == NULL)
		return;
	SocketList[slot]->handling = false; // no handling anymore
	FD_CLR(SocketList[slot]->get_socket(), &master);// remove from master set
	SocketList[slot]->close(); // close the socket
	delete SocketList[slot]; // destroy ySocket
	SocketList[slot] = NULL; // free in list
}

//=============================================================================
// Thread Handling
//=============================================================================
//-----------------------------------------------------------------------------
// Check if IP is allowed for keep-alive
//-----------------------------------------------------------------------------
bool CWebserver::CheckKeepAliveAllowedByIP(std::string client_ip) {
	pthread_mutex_lock(&mutex);
	bool do_keep_alive = true;
	CStringVector::const_iterator it = conf_no_keep_alive_ips.begin();
	while (it != conf_no_keep_alive_ips.end()) {
		if (trim(*it) == client_ip)
			do_keep_alive = false;
		++it;
	}
	pthread_mutex_unlock(&mutex);
	return do_keep_alive;
}
//-----------------------------------------------------------------------------
// Set Entry(number)to NULL in Threadlist
//-----------------------------------------------------------------------------
void CWebserver::clear_Thread_List_Number(int number) {
	pthread_mutex_lock(&mutex);
	if (number < HTTPD_MAX_CONNECTIONS)
		Connection_Thread_List[number] = (pthread_t) NULL;
	CloseConnectionSocketsByTimeout();
	pthread_mutex_unlock(&mutex);
}

//-----------------------------------------------------------------------------
// A new Connection is established to newSock. Create a (threaded) Connection
// and handle the Request.
//-----------------------------------------------------------------------------
bool CWebserver::handle_connection(CySocket *newSock) {
	void *WebThread(void *args); //forward declaration

	// create arguments
	TWebserverConnectionArgs *newConn = new TWebserverConnectionArgs;
	newConn->ySock = newSock;
	newConn->ySock->handling = true;
	newConn->WebserverBackref = this;
#ifdef Y_CONFIG_BUILD_AS_DAEMON
	newConn->is_treaded = is_threading;
#else
	newConn->is_treaded = false;
#endif
	int index = -1;
#ifdef Y_CONFIG_BUILD_AS_DAEMON
	if(is_threading)
	{
		pthread_mutex_lock( &mutex );
		// look for free Thread slot
		for(int i=0;i<HTTPD_MAX_CONNECTIONS;i++)
		if(Connection_Thread_List[i] == (pthread_t)NULL)
		{
			index = i;
			break;
		}
		if(index == -1)
		{
			dperror("Maximum Connection-Threads reached\n");
			pthread_mutex_unlock( &mutex );
			return false;
		}
		newConn->thread_number = index; //remember Index of Thread slot (for clean up)

		// Create an orphan Thread. It is not joinable anymore
		pthread_mutex_unlock( &mutex );

		// start connection Thread
		if(pthread_create(&Connection_Thread_List[index], &attr, WebThread, (void *)newConn) != 0)
		dperror("Could not create Connection-Thread\n");
	}
	else // non threaded
#endif
	WebThread((void *) newConn);
	return ((index != -1) || !is_threading);
}
//-------------------------------------------------------------------------
// Webserver-Thread for each connection
//-------------------------------------------------------------------------
void *WebThread(void *args) {
	CWebserverConnection *con;
	CWebserver *ws;
	TWebserverConnectionArgs *newConn = (TWebserverConnectionArgs *) args;
	ws = newConn->WebserverBackref;

	bool is_threaded = newConn->is_treaded;
	if (is_threaded)
		log_level_printf(1, "++ Thread 0x06%X gestartet\n",
				(int) pthread_self());

	if (!newConn) {
		dperror("WebThread called without arguments!\n");
		if (newConn->is_treaded)
			pthread_exit( NULL);
	}

	// (1) create & init Connection
	con = new CWebserverConnection(ws);
	con->Request.UrlData["clientaddr"] = newConn->ySock->get_client_ip(); // TODO:here?
	con->sock = newConn->ySock; // give socket reference
	newConn->ySock->handling = true; // dont handle this socket now be webserver main loop

	// (2) handle the connection
	con->HandleConnection();

	// (3) end connection handling
#ifdef Y_CONFIG_FEATURE_KEEP_ALIVE
	if(!con->keep_alive)
	log_level_printf(2,"FD SHOULD CLOSE sock:%d!!!\n",con->sock->get_socket());
	else
	ws->addSocketToMasterSet(con->sock->get_socket()); // add to master set
#else
	delete newConn->ySock;
#endif
	if (!con->keep_alive)
		con->sock->isValid = false;
	con->sock->handling = false; // socket can be handled by webserver main loop (select) again

	// (4) end thread
	delete con;
	int thread_number = newConn->thread_number;
	delete newConn;
	if (is_threaded) {
		log_level_printf(1, "-- Thread 0x06%X beendet\n", (int) pthread_self());
		ws->clear_Thread_List_Number(thread_number);
		pthread_exit( NULL);
	}
	return NULL;
}
