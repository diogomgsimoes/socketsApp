/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * callbacks.h
 *
 * Header file of functions that handle main application logic for UDP communication
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/

#include <gtk/gtk.h>
#include <glib.h>
#include <netinet/in.h>
#include <inttypes.h>
#include "gui.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif


/**********************\
|*  Global variables  *|
\**********************/

// TRUE if server is active
extern gboolean active;
// Directory pathname to write output files
extern char *out_dir;
// User name
extern char *user_name;
// List with active TCP connections/threads
extern GList *tcp_conn;
// TRUE if IPv4 is on and IPv6 if off
extern gboolean active4;
// TRUE if IPv6 is on and IPv4 is off
extern gboolean active6;
// Timer event
extern guint query_timer_id;



#ifndef CALLBACKS_INC_
#define CALLBACKS_INC_


// File thread (TCP connection) information
typedef struct Thread_Data {
    gboolean sending;	// TRUE: transmitting ; FALSE: receiving
    char fname[256];   	// if (!sending) has the filename being recorded
    int len;		   	// if (!sending) has the block size being received

    pthread_t tid;	   	// Thread ID
    char name_str[80]; 	// Thread name
    int s;			   	// Descriptor of the TCP socket
    FILE *f;		   	// In/out file descriptor
    long long total; 	// Bytes handled in the subprocess
    long long flen;		// File length
    struct in6_addr ip; // IP address of remote node
    u_short port;		// port number of remote node
    char nome[80];		// User name
	gboolean slow;		// Using slow configuration

    gboolean finished;	// If it finished the transference
    struct Thread_Data *self;	// Self testing pointer, to detected freed memory blocks
} Thread_Data ;



//#define DEBUG
#define MESSAGE_MAX_LENGTH	9000

/* Packet types */
#define REGISTRATION_NAME		21
#define CANCELLATION_NAME		20

/* Clock period durations */
#define NAME_TIMER_PERIOD	10000



/****************************************\
|* Functions to handle list of users    *|
 \****************************************/
// Handle REGISTRATION/CANCELLATION packets
gboolean process_registration(const char *name, int n, const char *ip_str,
		u_short port, gboolean registration);
// Sends a message using the IPv6 multicast socket
gboolean send_multicast(const char *buf, int n);
// Create a REGISTRATION/CANCELLATION message with the name and sends it
void multicast_name(gboolean registration);
// Test the timer for all neighbors
void test_all_name_timer(void);
// Timer callback for periodical registration of the server's name
gboolean callback_name_timer(gpointer data);
// Callback to receive data from UDP socket
gboolean callback_UDP_data(GIOChannel *source, GIOCondition condition,
		gpointer data);

/***********************************************************\
|* Functions to handle the list of file transfer threads   *|
 \**********************************************************/
// Add thread information to process list
Thread_Data *new_file_thread_desc(gboolean sending, struct in6_addr *ip, u_short port,
		const char *filename, gboolean optimal);
// Locate descriptor in subprocess list
Thread_Data *locate_file_thead_in_list(unsigned tid);
// Delete descriptor in subprocess list
gboolean free_file_thread_desc(unsigned tid, Thread_Data *pt);
// Stop the transmission of all files
void stop_all_file_threads();
// Callback to receive connections at TCP socket
gboolean callback_connections_TCP(GIOChannel *source, GIOCondition condition,
		gpointer data);


// Start sending a file to the selected user - handle button "SendFile"
void
on_buttonSendFile_clicked                (GtkButton       *button,
        								 gpointer         user_data);
// Stop the selected file transmission - handle button "Stop"
void
on_buttonStop_clicked                 	(GtkButton       *button,
		                                 gpointer         user_data);

/*********************************\
|* Functions to control sockets  *|
\*********************************/
// Close the UDP sockets
void close_sockUDP(void);
// Close TCP socket
void close_sockTCP(void);
// Create IPv4 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp4(const char *addr_multicast);
// Create IPv6 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp6(const char *addr_multicast);
// Initialize all sockets. Receives configuration from global variables:
// str_addr_MCast4/6 - IP multicast address
// imr_MCast4/6 - struct for association to to IP Multicast address
// port_MCast4/6 - multicast port
// addr_MCast4/6 - struct with UDP socket data for sending packets to the group
gboolean init_sockets(gboolean is_ipv6, const char *addr_multicast);


/*******************************************************\
|* Functions to control the state of the application   *|
\*******************************************************/
// Closes everything
void close_all(void);
// Button that starts and stops the application
void
on_togglebuttonActive_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
// IPv4 type button modified; it redefines the multicast address and the socket type
void
on_checkbuttonIPv4_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
// IPv6 type button modified; it redefines the multicast address and the socket type
void
on_checkbuttonIPv6_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
// The user closed the main window
gboolean
on_window1_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);
#endif
