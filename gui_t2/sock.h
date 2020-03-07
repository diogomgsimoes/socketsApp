/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * sock.h
 *
 * Header file of functions that handle network programming using sockets and Gtk+3.0
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo
\*****************************************************************************/
#ifndef _INCL_SOCK_H_
#define _INCL_SOCK_H_

#include <netinet/in.h>
#include <gtk/gtk.h>
#include <glib.h>


// Variables with local IP addresses
extern struct in_addr local_ipv4; // Local IPv4 address
extern gboolean valid_local_ipv4; // TRUE if the local IPv4 address is valid
extern struct in6_addr local_ipv6;// Local IPv6 address
extern gboolean valid_local_ipv6; // TRUE if the local IPv6 address is valid


/* Macro to read from a buffer to a variable */
/* pt - read pointer */
/* var - pointer to the variable */
/* n - number of bytes to read */
#define READ_BUF(pt, var, n)  bcopy(pt, var, n); pt+= n

/* Macro to write from a variable to a buffer */
/* pt - write pointer */
/* var - pointer to the variable */
/* n - number of bytes to read */
#define WRITE_BUF(pt, var, n)  bcopy(var, pt, n); pt+= n


void set_local_IP(); // Set the contents of the variables with the local IP addresses
					 // BUGS: It only works for 1 address per host
gboolean init_local_ipv4(struct in_addr *ip);  //  Get local IPv4 address
gboolean init_local_ipv6(struct in6_addr *ip);  //  Get local IPv6 address
gboolean is_local_ip(const char *ip_str); // Return TRUE if 'ip_str' is a local address
void translate_local_ip(struct in6_addr *ip); // Convert "::1" to the local global address

gboolean get_IPv6(const gchar *textIP, struct in6_addr *addrv6); // Read an IPv6 Multicast address
gboolean get_IPv4(const gchar *textIP, struct in_addr *addrv4); // Read an IPv4 Multicast address
// Convert IPv4 address to the IPv6 equivalent address ::ffff:IPv4
gboolean translate_ipv4_to_ipv6(const char *ipv4_str, struct in6_addr *ipv6);

char *addr_ipv4(struct in_addr *addr);	// Return a static string with an IPv4 address contents
char *addr_ipv6(struct in6_addr *addr);	// Return a static string with an IPv6 address contents

int init_socket_ipv4(int dom, int port, gboolean shared); // Initialize an IPv4 socket
int init_socket_ipv6(int dom, int port, gboolean shared); // Initialize an IPv6 socket
// dom = SOCK_DGRAM or SOCK_STREAM
// Return: -1 - error;  >0 - socket number

int get_portnumber(int s); // Return the port number associated to a socket

// Read data from an IPv4 socket
// Returns the number of byte read (<0 in case of error) and the sender's address and port
int read_data_ipv4(int sock, char *buf, int n, struct in_addr *ip,
		    short unsigned int *port);

// Read data from an IPv6 socket
// Returns the number of byte read (<0 in case of error) and the sender's address and port
int read_data_ipv6(int sock, char *buf, int n, struct in6_addr *ip,
		    short unsigned int *port);

// Create a GIOchannel object and regist a callback function in the GIO main loop
// event = G_IO_IN ; G_IO_OUT; G_IO_IN | G_IO_OUT
gboolean put_socket_in_mainloop(int sock, void *ptr, guint *chan_id, GIOChannel **chan,
		guint event, gboolean(*callback)(GIOChannel *, GIOCondition, gpointer));

// Regist a callback function in the GIO main loop
// event = G_IO_IN or G_IO_OUT or G_IO_IN | G_IO_OUT
gboolean restore_socket_in_mainloop(int sock, void *ptr, guint *chan_id, GIOChannel *chan,
		guint event, gboolean(*callback)(GIOChannel *, GIOCondition, gpointer));

// Cancel the callback registration in the GIO main loop
void suspend_socket_from_mainloop(int sock, int chan_id);

// Free GIO channel resources
void free_gio_channel(GIOChannel *chan);

// Cancel callback registration in the GIO main loop and free the resources allocated
void remove_socket_from_mainloop(int sock, guint chan_id, GIOChannel *chan);

// Close the socket
void close_socket(int sock);


#endif

