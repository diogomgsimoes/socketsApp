/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * callbacks.c
 *
 * Functions that handle main application logic for UDP communication
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#include <gtk/gtk.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include "sock.h"
#include "file.h"
#include "gui.h"
#include "thread.h"
#include "callbacks.h"

#ifdef DEBUG
#define debugstr(x)     g_print(x)
#else
#define debugstr(x)
#endif

/**********************\
|*  Global variables  *|
\**********************/

gboolean active = FALSE; 	// TRUE if server is active
char *user_name = NULL; // User name
gboolean active4 = FALSE; // TRUE if IPv4 is on and IPv6 if off
gboolean active6 = FALSE; // TRUE if IPv6 is on and IPv4 is off

guint query_timer_id = 0; // Timer event

gboolean changing = FALSE; // If it is changing the network

GList *tcp_conn = NULL; // List with active TCP connections/threads

// Mutex to synchronize changes to threads list
pthread_mutex_t tmutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG
#define LOCK_MUTEX(mutex,str) { \
			fprintf(stderr,str); \
			pthread_mutex_lock( mutex ); \
		}
#else
#define LOCK_MUTEX(mutex,str) { \
			pthread_mutex_lock( mutex ); \
		}
#endif

#ifdef DEBUG
#define UNLOCK_MUTEX(mutex,str) { \
			fprintf(stderr,str); \
			pthread_mutex_unlock( mutex ); \
		}
#else
#define UNLOCK_MUTEX(mutex,str) { \
			pthread_mutex_unlock( mutex ); \
		}
#endif


/******************************\
|*  Network Global variables  *|
\******************************/
u_short port_MCast = 0; // IPv4/IPv6 Multicast port

int sockUDP4 = -1; // IPv4 UDP socket descriptor
const char *str_addr_MCast4 = NULL;// IPv4 multicast address
struct sockaddr_in addr_MCast4; // struct with data of IPv4 socket
struct ip_mreq imr_MCast4; // struct with IPv4 multicast data

int sockUDP6 = -1; // IPv6 UDP socket descriptor
const char *str_addr_MCast6 = NULL;// IPv6 multicast address
struct sockaddr_in6 addr_MCast6; // struct with data of IPv6 socket
struct ipv6_mreq imr_MCast6; // struct with IPv6 multicast data

GIOChannel *chanUDP = NULL; // GIO channel descriptor of socket UDPv4 or UDPv6
guint chanUDP_id = 0; // channel number of socket UDPv4 or UDPv6

guint nome_timer_id = 0; // Timer event id

u_short port_TCP = 0; // TCP port
int sockTCP = -1; // IPv6 TCP socket descriptor
GIOChannel *chanTCP = NULL; // GIO channel descriptor of TCPv6 socket
guint chanTCP_id = 0; // Channel number of socket TCPv6


/*********************\
|*  Local variables  *|
 \*********************/
// Used to define unique numbers for incoming files
static int counter = 0;
// Temporary buffer
static char tmp_buf[8000];



/****************************************\
|* Functions to handle list of users    *|
 \****************************************/


// Handle REGISTRATION/CANCELLATION packets
gboolean process_registration(const char *name, int n, const char *ip_str,
		u_short port, gboolean registration) {

	if (strlen(name) != n - 1) {
		Log("Packet with string not terminated with '\\0' - ignored\n");
		return FALSE;
	}
	if (registration) {
		if (GUI_regist_name(name, ip_str, port)) {
			// New registration
			if (!strcmp(user_name, name)) {
				// Same name as the local name
				if (is_local_ip(ip_str) && (port == port_TCP)) {
					// Same socket - ignored
					return FALSE;
				}
			}
		} else
			return FALSE;
	} else {
		// Cancellation
		if (!GUI_cancel_name(name, ip_str, port)) {
			sprintf(tmp_buf, "Cancellation of unknown user '%s'\n", name);
			Log(tmp_buf);
			return FALSE;
		}
	}
	return TRUE;
}


// Sends a message using the IPv6 multicast socket
gboolean send_multicast(const char *buf, int n) {
	if (!active) {
		debugstr("active is false in send_multicast\n");
		return FALSE;
	}
	assert(active4 ^ active6);
	assert((sockUDP4>0) ^ (sockUDP6>0));
	// Sends message.
	if (active4) {
		if (sendto(sockUDP4, buf, n, 0, (struct sockaddr *) &addr_MCast4,
				sizeof(addr_MCast4)) < 0) {
			perror("Error while sending multicast datagram");
			return FALSE;
		}
	} else {
		if (sendto(sockUDP6, buf, n, 0, (struct sockaddr *) &addr_MCast6,
				sizeof(addr_MCast6)) < 0) {
			perror("Error while sending multicast datagram");
			return FALSE;
		}
	}
	return TRUE;
}

// Create a REGISTRATION/CANCELLATION message with the name and sends it
void multicast_name(gboolean registration) {

	// TASK 1
	// write the REGISTRATION / CANCELLATIOM message to a temporary buffer and
	//    send it using the function 'send_multicast(buf, len);

	unsigned char cod;
	short unsigned int len;
	char *pt = &tmp_buf[0];
	char *ptr = pt;

	assert(user_name != NULL);											// user_name
	assert(port_TCP > 0);												// port_TCP
	cod = (registration ? REGISTRATION_NAME : CANCELLATION_NAME);		// cod

	WRITE_BUF(ptr, &cod, 1);            								// Adds cod
	WRITE_BUF(ptr, &port_TCP, sizeof(port_TCP));        				// Adds port_TCP
	WRITE_BUF(ptr, user_name, strlen(user_name) + 1);   				// Adds user_name
	len = ptr - pt;														// Length in network format

	send_multicast(tmp_buf, len);										// send_multicast
}


// Test the timer for all neighbors
void remove_overdue(void) {

	GList *overdue= GUI_test_all_name_timer();
	UserInfo *pt;
	if (overdue != NULL) {
		GList *list= overdue;
		while (list != NULL) {
			pt= (UserInfo *) list->data;
			if (!GUI_cancel_name(pt->name, pt->ip, pt->port)) {
#ifdef DEBUG
				g_print("Cancellation of unknown user '%s'\n", pt->name);
#endif
			}
			list = list->next;
		}
		GUI_free_UserInfo_list (overdue);
	}
}


// Timer callback for periodical registration of the server's name
gboolean callback_name_timer(gpointer data) {
	// data - entrymSec with number
	if (!active)
		return FALSE;
	if (changing) {
		debugstr("Callback_name_timer while changing\n");
		return TRUE;
	}
	debugstr("Callback_name_timer sent NAME\n");
	multicast_name(TRUE);
	remove_overdue();
	return TRUE; // periodic timer
}


// Callback to receive data from UDP socket
gboolean callback_UDP_data(GIOChannel *source, GIOCondition condition,
		gpointer data) {
	static char buf[MESSAGE_MAX_LENGTH]; // buffer for reading data
	struct in6_addr ipv6;
	struct in_addr ipv4;
	char ip_str[81];
	u_short port;
	int n;

	assert(main_window != NULL);
	if (!active) {
		debugstr("callback_UDP_data with active FALSE\n");
		return FALSE;
	}
	if (condition == G_IO_IN) {
		// Receive packet //
		if (active6) {
			n = read_data_ipv6(sockUDP6, buf, MESSAGE_MAX_LENGTH, &ipv6, &port);
			strncpy(ip_str, addr_ipv6(&ipv6), sizeof(ip_str));
		} else if (active4) {
			n = read_data_ipv4(sockUDP4, buf, MESSAGE_MAX_LENGTH, &ipv4, &port);
			strncpy(ip_str, addr_ipv4(&ipv4), sizeof(ip_str));
		} else {
			if (changing)
				return TRUE;
			assert(active6 || active4);
		}
		if (n <= 0) {
			Log("Failed reading packet from multicast socket\n");
			return TRUE; // Continue waiting for more events
		} else {
			time_t tbuf;
			unsigned char m;
			char *pt;

			// Read data //
			pt = buf;
			READ_BUF(pt, &m, 1); // Reads type and advances pointer
			READ_BUF(pt, &port, 2); // Reads port and advances pointer
			// Writes date and sender's data //
			time(&tbuf);
			sprintf(tmp_buf, "%sReceived %d bytes from %s#%hu - type %hhd\n",
					ctime(&tbuf), n, ip_str, port, m);
			g_print("%s", tmp_buf);
			switch (m) {
			case REGISTRATION_NAME:
				sprintf(tmp_buf, "Registration of '%s' - %s#%hu\n", pt, ip_str,
						port);
				if (process_registration(pt, n - 3, ip_str, port, TRUE))
					Log(tmp_buf);
				else
					g_print("%s", tmp_buf);
				break;
			case CANCELLATION_NAME:
				sprintf(tmp_buf, "Cancellation of '%s' - %s#%hu\n", pt, ip_str,
						port);
				if (process_registration(pt, n - 3, ip_str, port, FALSE))
					Log(tmp_buf);
				else
					g_print("%s", tmp_buf);
				break;
			default:
				sprintf(tmp_buf, "Invalid packet type (%d) - ignored\n",
						(int) m);
				Log(tmp_buf);
				break;
			}
			return TRUE; // Keeps receiving more packets
		}
	} else if ((condition == G_IO_NVAL) || (condition == G_IO_ERR)) {
		Log("Error detected in UDP socket\n");
		// Turns sockets off
		close_all();
		// Closes the application
		gtk_main_quit();
		return FALSE; // Stops callback for receiving packets from socket
	} else {
		assert(0); // Should never reach this line
		return FALSE; // Stops callback for receiving packets from socket
	}
}

/***********************************************************\
|* Functions to handle the list of file transfer threads   *|
 \**********************************************************/

// Add thread information to file thread list
Thread_Data *new_file_thread_desc(gboolean sending, struct in6_addr *ip, u_short port,
		const char *filename, gboolean slow) {
	assert(ip != NULL);
	assert(filename != NULL);

	Thread_Data *pt;

	//  g_print("new_desc(pid=%d ; pipe=%d ; %s)\n", pid, pipe, sending?"SND":"RCV");
	pt = (Thread_Data *) malloc(sizeof(Thread_Data));
	pt->sending= sending;
	pt->slow= slow;
	memcpy(&pt->ip, ip, sizeof(struct in6_addr));
	pt->port= port;
	strncpy(pt->fname, filename, sizeof(pt->fname));
	// Default initialization
	pt->len= 0;
	pt->s= 0;
	pt->f= NULL;
	pt->flen= 0;
	pt->total= 0;
	pt->nome[0]= '\0';
	pt->name_str[0]='\0';
    pt->finished= FALSE;
    pt->self= pt;

    // Add thread descriptor to list
	LOCK_MUTEX(&tmutex, "lock_t1\n");
    tcp_conn = g_list_append(tcp_conn, pt);
	UNLOCK_MUTEX(&tmutex, "unlock_t1\n");
	return pt;
}

// Locate descriptor in file thread list
Thread_Data *locate_file_thead_desc(unsigned tid) {
	GList *list = tcp_conn;
	while (list != NULL) {
		if (((unsigned)((Thread_Data *) list->data)->tid) == tid)
			return (Thread_Data *) list->data;
		list = list->next;
	}
	return NULL;
}

// Remove descriptor from file thread list, stop it and free memory
gboolean free_file_thread_desc(unsigned tid, Thread_Data *pt) {
	LOCK_MUTEX(&tmutex, "lock_t2\n");
	if (pt == NULL)
		pt = locate_file_thead_desc(tid);
	if (pt != NULL) {
		if (tcp_conn != NULL) {
			tcp_conn = g_list_remove(tcp_conn, pt);
		}
		pt->finished= TRUE;  // Mark the thread as ending
		UNLOCK_MUTEX(&tmutex, "unlock_t2\n");
		// Remove the thread from the GUI
		GUI_remove_thread((unsigned)pt->tid);
		// Close the socket
		if (pt->s>0) {
			close (pt->s);
			pt->s= 0;
		}
		// Close the file
		if (pt->f != NULL) {
			fclose(pt->f);
			pt->f= NULL;
		}
		// Mark block as freed
		pt->self= NULL;
		// Free memory
		free(pt);
		return TRUE;
	}
	UNLOCK_MUTEX(&tmutex, "unlock_t2\n");
	return FALSE;
}

// Stop the transmission of all files
void stop_all_file_threads() {
	Thread_Data *pt;

	LOCK_MUTEX(&tmutex, "lock_t3\n");
	GList *list = tcp_conn;
	while (list != NULL) {
		pt= (Thread_Data *) list->data;
		pt->finished= TRUE;  // Mark thread as finishing - memory is freed when it stops
		list = list->next;
	}
	g_list_free(tcp_conn);
	tcp_conn= NULL;
	UNLOCK_MUTEX(&tmutex, "unlock_t3\n");
}

// Callback to receive connections at TCP socket
gboolean callback_connections_TCP(GIOChannel *source, GIOCondition condition,
		gpointer data) {
	char buf[180]; // buffer for receiving data

	assert(main_window != NULL);
	assert(active);
	if (condition == G_IO_IN) {
		// Received a new connection
		struct sockaddr_in6 server;
		int msgsock;
		unsigned int length = sizeof(server);

		msgsock = accept(sockTCP, (struct sockaddr *) &server, &length);
		if (msgsock == -1) {
			perror("accept");
			Log("accept failed - aborting\nPlease turn off the application!\n");
			return FALSE; // Turns callback off
		} else {
			sprintf(tmp_buf, "Received connection from %s - %d\n", addr_ipv6(
					&server.sin6_addr), ntohs(server.sin6_port));
			Log(tmp_buf);
			// Sets the filename where the received data will be created
			sprintf(buf, "%s/file%d.out", out_dir, counter++);
			// Starts a thread to read the data from the socket
			Thread_Data *pt= start_rcv_file_thread(msgsock, &server.sin6_addr,
					ntohs(server.sin6_port), buf, get_slow());
			return (pt != NULL);
		}

	} else if ((condition == G_IO_NVAL) || (condition == G_IO_ERR)) {
		Log("Detected error in TCP socket\n");
		// Closes the sockets
		close_all();
		// Quits the application
		gtk_main_quit();
		return FALSE; // Stops callback for receiving connections in the socket
	} else {
		assert(0); // Should never reach this line
		return FALSE; // Stops callback for receiving connections in the socket
	}
}

// Start sending a file to the selected user - handle button "SendFile"
void on_buttonSendFile_clicked(GtkButton *button, gpointer user_data) {
	char *ip, *name;
	int port;
	struct in6_addr ip_file;
	GtkTreeIter iter;

	if (!active) {
		Log("This program is not active\n");
		return;
	}
	if (!GUI_get_selected_User(&ip, &port, &name, &iter)) {
		Log("No user is selected\n");
		return;
	}

	// TASK 6:
	// Convert the string 'ip' to equivalent IPv6 address and store it in
	//      >>>>> 'ip_file' variable <<<<<
	// Suggestion: read section 2.1.5.1 of the introduction document and look to the functions
	//           in sock.h/sock.c

	// if ipv4 is active, translate to ipv6; if not, store it in ip_file using inet_pton
	if(active4)
		g_print("result = %d", translate_ipv4_to_ipv6(ip, &ip_file));
	else
		g_print("result = %d", inet_pton(AF_INET6, ip, &ip_file));

	const char *filename = gtk_entry_get_text(main_window->FileName);
	FILE *f = fopen(filename, "r");
	if (f == NULL) {
		Log("Select a valid file to transmit and try again\n");
		// Open window
		on_buttonFilename_clicked(NULL, NULL);
		return;
	}
	fclose(f);

	// Start sending the file
	start_snd_file_thread(&ip_file, port, name, filename, get_slow());
}

// Stop the selected file transmission - handle button "Stop"
void on_buttonStop_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeIter iter;
	unsigned tid;

	if (!GUI_get_selected_Filetx(&tid, &iter)) {
		Log("No file transfer is selected\n");
		return;
	}
	fprintf(stderr, "stopping tid %u\n", (unsigned)tid);
	Thread_Data *th= locate_file_thead_desc(tid);
	if (th != NULL) {
		if (!th->finished && (th->self == th))
			th->finished= TRUE;
	} else {
		Log("Stop did not locate thread\n");
	}
}

/*********************************\
|* Functions to control sockets  *|
\*********************************/

// Close the UDP sockets
void close_sockUDP(void) {
	gboolean old_changing = changing;
	debugstr("close_sockUDP\n");
	changing = TRUE;

	if (chanUDP != NULL) {
		if ((sockUDP4 > 0) ^ (sockUDP6 > 0)) {
			remove_socket_from_mainloop((sockUDP4 > 0)?sockUDP4:sockUDP6, chanUDP_id, chanUDP);
			chanUDP= NULL;
		}
		// It closed all sockets!
	} else {
		if (sockUDP4 > 0) {
			// TASK 4:
			// Leave the group and close the UDP IPv4 socket
			// Look at the IPv6 code and translate to IPv4 ...
			if (str_addr_MCast4 != NULL) {
				// Leaves the group
				if (setsockopt(sockUDP4, IPPROTO_IP, MCAST_LEAVE_GROUP,
						(char *) &imr_MCast4, sizeof(imr_MCast4)) == -1) {
					perror("Failed de-association to IPv4 multicast group");
					sprintf(
							tmp_buf,
							"Failed de-association to IPv4 multicast group (%hu)\n",
							sockUDP4);
					Log(tmp_buf);
				}
			}
			// Close socket
			if (close(sockUDP4))
				perror("Error during close of IPv4 multicast socket");
		}

		if (sockUDP6 > 0) {
			if (str_addr_MCast6 != NULL) {
				// Leaves the group
				if (setsockopt(sockUDP6, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
						(char *) &imr_MCast6, sizeof(imr_MCast6)) == -1) {
					perror("Failed de-association to IPv6 multicast group");
					sprintf(
							tmp_buf,
							"Failed de-association to IPv6 multicast group (%hu)\n",
							sockUDP6);
					Log(tmp_buf);
					/* NOTE: Kernel 2.4 has a bug - it does not support de-association of IPv6 groups! */
				}
			}
			if (close(sockUDP6))
				perror("Error during close of IPv6 multicast socket");
		}
	}
	sockUDP4 = -1;
	str_addr_MCast4 = NULL;
	active4 = FALSE;
	sockUDP6 = -1;
	str_addr_MCast6 = NULL;
	active6 = FALSE;
	changing = old_changing;
}

// Close TCP socket
void close_sockTCP(void) {
	if (chanTCP != NULL) {
		remove_socket_from_mainloop(sockTCP, chanTCP_id, chanTCP);
		chanTCP= NULL;
	}
	if (sockTCP > 0) {
		close(sockTCP);
		sockTCP = -1;
	}
	port_TCP = 0;
}

// Create IPv4 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp4(const char *addr_multicast) {
	gboolean old_changing = changing;
	char loop = 1;

	if (active4)
		return TRUE;
	changing = TRUE;

	if ((sockUDP6 > 0) || (sockUDP4 > 0)) {
		debugstr("WARNING: 'init_sockets_udp4' closed UDP socket\n");
		close_sockUDP();
	}

	// TASK 4:
	// Create the IPv4 UDP socket
	// Look at the IPv6 version code and translate to IPv4 ...

	// Prepares the data structures
	if (!get_IPv4(addr_multicast, &addr_MCast4.sin_addr)) {
		return FALSE;
	}

	addr_MCast4.sin_port = htons(port_MCast);
	addr_MCast4.sin_family = AF_INET;
	bcopy(&addr_MCast4.sin_addr, &imr_MCast4.imr_multiaddr, sizeof(addr_MCast4.sin_addr));
	imr_MCast4.imr_interface.s_addr = htonl(INADDR_ANY);

	// Create the IPV4 UDP socket
	sockUDP4 = init_socket_ipv4(SOCK_DGRAM, port_MCast, TRUE);
	fprintf(stderr, "UDP4 = %d\n", sockUDP4);
	if (sockUDP4 < 0) {
		Log("Failed opening IPv4 UDP socket\n");
		return FALSE;
	}

	// Join the group
	if (setsockopt(sockUDP4, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &imr_MCast4, sizeof(imr_MCast4)) == -1) {
			perror("Failed association to IPv4 multicast group");
			Log("Failed association to IPv4 multicast group\n");
			return FALSE;
	}
	str_addr_MCast4 = addr_multicast; // Memorizes it is associated to a group

	// Configures the socket to receive an echo of the multicast packets sent by this application
	setsockopt(sockUDP4, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

	// Regist the socket in the main loop of Gtk+
	// ...
	//      Use the callback function: callback_UDP_data

	if (!put_socket_in_mainloop(sockUDP4, (void *) 0, &chanUDP_id, &chanUDP, G_IO_IN, callback_UDP_data)) {
			Log("Failed registration of UDPv4 socket at Gnome\n");
			close_sockUDP();
			return FALSE;
	}
	active4 = TRUE;
	changing = old_changing;
	return TRUE;
}

// Create IPv6 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp6(const char *addr_multicast) {
	gboolean old_changing = changing;
	char loop = 1;

	if (active6)
		return TRUE;

	changing = TRUE;
	if ((sockUDP6 > 0) || (sockUDP4 > 0)) {
		debugstr("WARNING: 'init_sockets_udp6' closed UDP socket\n");
		close_sockUDP();
	}

	// Prepare the data structures
	if (!get_IPv6(addr_multicast, &addr_MCast6.sin6_addr)) {
		return FALSE;
	}
	addr_MCast6.sin6_port = htons(port_MCast);
	addr_MCast6.sin6_family = AF_INET6;
	addr_MCast6.sin6_flowinfo = 0;
	bcopy(&addr_MCast6.sin6_addr, &imr_MCast6.ipv6mr_multiaddr,
			sizeof(addr_MCast6.sin6_addr));
	imr_MCast6.ipv6mr_interface = 0;

	// Create the IPV6 UDP socket
	sockUDP6 = init_socket_ipv6(SOCK_DGRAM, port_MCast, TRUE);
	fprintf(stderr, "UDP6 = %d\n", sockUDP6);
	if (sockUDP6 < 0) {
		Log("Failed opening IPv6 UDP socket\n");
		return FALSE;
	}
	// Join the multicast group
	if (setsockopt(sockUDP6, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			(char *) &imr_MCast6, sizeof(imr_MCast6)) == -1) {
		perror("Failed association to IPv6 multicast group");
		Log("Failed association to IPv6 multicast group\n");
		return FALSE;
	}
	str_addr_MCast6 = addr_multicast; // Memorizes it is associated to a group

	// Configure the socket to receive an echo of the multicast packets sent by this application
	setsockopt(sockUDP6, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));

	// Regist the socket in the main loop of Gtk+
	if (!put_socket_in_mainloop(sockUDP6, (void *) 0, &chanUDP_id, &chanUDP, G_IO_IN,
			callback_UDP_data)) {
		Log("Failed registration of UDPv6 socket at Gnome\n");
		close_sockUDP();
		return FALSE;
	}
	active6 = TRUE;
	changing = old_changing;
	return TRUE;
}

// Initialize all sockets. Receives configuration from global variables:
// str_addr_MCast4/6 - IP multicast address
// imr_MCast4/6 - struct for association to to IP Multicast address
// port_MCast4/6 - multicast port
// addr_MCast4/6 - struct with UDP socket data for sending packets to the group
gboolean init_sockets(gboolean is_ipv6, const char *addr_multicast) {
	if (is_ipv6) {
		if (!init_socket_udp6(addr_multicast))
			return FALSE;
	} else {
		if (!init_socket_udp4(addr_multicast))
			return FALSE;
	}

	// Socket TCP   //////////////////////////////////////////////////////////

	if (sockTCP > 0) {
		debugstr("WARNING: 'init_sockets' closed TCP socket\n");
		close_sockTCP();
	}
	// Creates TCP socket
	sockTCP = init_socket_ipv6(SOCK_STREAM, 0, FALSE);
	if (sockTCP < 0) {
		Log("Failed opening IPv6 TCP socket\n");
		close_sockTCP();
		return FALSE;
	}
	port_TCP = get_portnumber(sockTCP);
	if (port_TCP == 0) {
		Log("Failed to get the TCP port number\n");
		close_sockUDP();
		close_sockTCP();
		return FALSE;
	}
	// Prepares the socket to receive connections
	if (listen(sockTCP, 1) < 0) {
		perror("Listen failed\n");
		Log("Listen failed\n");
		close_sockUDP();
		close_sockTCP();
		return FALSE;
	}

	// Regists the TCP socket in Gtk+ main loop
	if (!put_socket_in_mainloop(sockTCP, main_window, &chanTCP_id, &chanTCP, G_IO_IN,
			callback_connections_TCP)) {
		Log("Failed registration of TCPv6 socket at Gnome\n");
		close_sockUDP();
		close_sockTCP();
		return FALSE;
	}
	return TRUE;
}

/*******************************************************\
|* Functions to control the state of the application   *|
\*******************************************************/

// Closes everything
void close_all(void) {
	gboolean old_changing = changing;
	changing = TRUE;

	if(nome_timer_id > 0){
			g_source_remove(nome_timer_id);
			nome_timer_id = 0;
		}

	if (user_name != NULL)
		multicast_name(FALSE);  // send a CANCELLATION message
	close_sockUDP();
	close_sockTCP();
	set_portT_number(0);
	stop_all_file_threads();
	if (user_name != NULL) {
		free(user_name);
		user_name = NULL;
	}

	GUI_clear_names();
	changing = old_changing;
}

// Button that starts and stops the application
void on_togglebuttonActive_toggled(GtkToggleButton *togglebutton, gpointer user_data) {

	if (gtk_toggle_button_get_active(main_window->active)) {

		// *** Starts the server ***
		const gchar *addr_str, *textNome;
		gboolean is_ipv6;
		gboolean b6 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip6));
		gboolean b4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip4));
		assert(b4 ^ b6);
		changing = TRUE;

		// Get local IP
		set_local_IP();
		gtk_entry_set_text(main_window->entryIPv6loc, addr_ipv6(&local_ipv6));
		gtk_entry_set_text(main_window->entryIPv4loc, addr_ipv4(&local_ipv4));
		// Read parameters
		if ((textNome = gtk_entry_get_text(main_window->entryName)) == NULL) {
			Log("Undefined user name\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		int n = get_portM_number();
		if (n < 0) {
			Log("Invalid multicast port number\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		port_MCast = (unsigned short) n;
		if (!(addr_str = get_IPmult(&is_ipv6, &addr_MCast4.sin_addr,
				&addr_MCast6.sin6_addr))) {
			Log("Invalid IP multicast address\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		if (b6 != is_ipv6) {
			Log("Invalid IP version of multicast address\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		if (b6) {
			addr_MCast6.sin6_port = htons(port_MCast);
			addr_MCast6.sin6_family = AF_INET6;
			addr_MCast6.sin6_flowinfo = 0;
			bcopy(&addr_MCast6.sin6_addr, &imr_MCast6.ipv6mr_multiaddr,
					sizeof(addr_MCast6.sin6_addr));
			imr_MCast6.ipv6mr_interface = 0;
		} else {
			assert(b4); // IPv4
			addr_MCast4.sin_port = htons(port_MCast);
			addr_MCast4.sin_family = AF_INET;
			bcopy(&addr_MCast4.sin_addr, &imr_MCast4.imr_multiaddr,
					sizeof(addr_MCast4.sin_addr));
			imr_MCast4.imr_interface.s_addr = htonl(INADDR_ANY);
		}
		if (!init_sockets(b6, addr_str)) {
			Log("Failed configuration of server\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		set_portT_number(port_TCP);

		// ****
		// Starts periodical sending of the NAME
		user_name = strdup(textNome);

		nome_timer_id = g_timeout_add(NAME_TIMER_PERIOD, callback_name_timer, NULL);

		// ****
		block_entrys(FALSE);
		changing = FALSE;
		active = TRUE;
		// Sends the local name
		multicast_name(TRUE);
		Log("fileexchange active\n");

	} else {

		// *** Stops the server ***
		close_all();
		block_entrys(TRUE);
		active = FALSE;
		Log("fileexchange stopped\n");
	}

}

// IPv4 type button modified; it redefines the multicast address and the socket type
void on_checkbuttonIPv4_toggled(GtkToggleButton *togglebutton,
		gpointer user_data) {
	changing = TRUE;
	// Set IP to IPv4 multicast address
	gboolean b4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip4));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_window->check_ip6), !b4);
	gtk_entry_set_text(main_window->entryMIP, g_strdup(!b4 ? "ff18:10:33::1"
			: "225.0.0.1"));
	if (active) {
		if ((!b4 ? !init_socket_udp6("ff18:10:33::1") : !init_socket_udp4(
				"225.0.0.1"))) {
			// Creation of socket failed
			close_all();
			gtk_toggle_button_set_active(main_window->active, FALSE);
		}
	}
	changing = FALSE;
	if (active) {
		// Sends its local name
		multicast_name(TRUE);
	}
}

// IPv6 type button modified; it redefines the multicast address and the socket type
void on_checkbuttonIPv6_toggled(GtkToggleButton *togglebutton,
		gpointer user_data) {
	changing = TRUE;
	// Set IP to IPv6 multicast address
	gboolean b6 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip6));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_window->check_ip4), !b6);
	gtk_entry_set_text(main_window->entryMIP, g_strdup(b6 ? "ff18:10:33::1"
			: "225.0.0.1"));
	if (active) {
		if ((b6 ? !init_socket_udp6("ff18:10:33::1") : !init_socket_udp4(
				"225.0.0.1"))) {
			// Creation of socket failed
			close_all();
			gtk_toggle_button_set_active(main_window->active, FALSE);
		}
	}
	changing = FALSE;
	if (active) {
		// Sends its local name
		multicast_name(TRUE);
	}
}

// The user closed the main window
gboolean on_window1_delete_event(GtkWidget *widget, GdkEvent *event,
		gpointer user_data) {
	close_all();
	gtk_main_quit();
	return FALSE;
}
