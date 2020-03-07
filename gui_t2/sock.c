/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * sock.c
 *
 * Functions that handle network programming using sockets and Gtk+3.0
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo
\*****************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <memory.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "sock.h"

// External logging function declared elsewhere
extern void Log(const gchar *str);


// Variables with local IP addresses
static const char *devicename= NULL;
static gboolean got_local_ip= FALSE; // Set local IP variables
struct in_addr local_ipv4;// Local IPv4 address
gboolean valid_local_ipv4; // TRUE if the local IPv4 address is valid
struct in6_addr local_ipv6;// Local IPv6 address
gboolean valid_local_ipv6; // TRUE if the local IPv6 address is valid



// Set the contents of the variables with the local IP addresses
void set_local_IP() {
  if (!got_local_ip) {
    valid_local_ipv4= init_local_ipv4(&local_ipv4);
    valid_local_ipv6= init_local_ipv6(&local_ipv6);
    got_local_ip= TRUE;
  }
}

// Get the local network device name
static const char *get_device_name() {
	static char buf[100];
	if (system(
		"ip link show | grep -v LOOPBACK | grep BROADCAST | awk '{ print $2 }' | sed '$ s/.$//' > /tmp/lixo0123456789.txt")<0)
		return NULL;
	FILE *fd = fopen("/tmp/lixo0123456789.txt", "r");
	char *pt= fgets(buf,100,fd);
	if (pt != NULL) {
		pt[strlen(pt)-1]='\0';	// Remove pending '\n'
	}
	fclose(fd);
	unlink("/tmp/lixo0123456789.txt");

	return pt;
}

// Get the local IPv4 address (device dev) using "ioctl" command
static gboolean get_local_ipv4name_using_ioctl(const char *dev,
		struct in_addr *addr) {
	struct ifreq req;
	int fd;
	assert((dev!=NULL) && (addr!=NULL));
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(req.ifr_name, dev);
	req.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFADDR, &req) < 0) {
		perror("getting local IP address");
		close(fd);
		return FALSE;
	}
	close(fd);
	struct sockaddr_in *pt = (struct sockaddr_in *) &req.ifr_ifru.ifru_addr;
	memcpy(addr, &(pt->sin_addr), 4);
	return TRUE;
}

// Get the local IPv4 address (device dev) using command "ifconfig"
static gboolean get_local_ipv4name_using_ifconfig(char *buf, int buf_len) {
	if (system("/sbin/ifconfig | grep -w inet | grep broadcast | head -1 | awk '{ print $2 }' | sed 's/addr\\://' > /tmp/lixo0123456789.txt")
			< 0)
		return FALSE;
	FILE *fd = fopen("/tmp/lixo0123456789.txt", "r");
	int n = fread(buf, 1, buf_len, fd);
	buf[n - 1] = '\0';
	fclose(fd);
	unlink("/tmp/lixo0123456789.txt");

	if (n <= 0)
		return FALSE;
	if (n >= 256)
		return FALSE;
	return TRUE;
}

// Get local IPv4 address
gboolean init_local_ipv4(struct in_addr *ip) {
	char name[265];
	struct hostent *hp;

	assert(ip != NULL);
	devicename= get_device_name();
	if (devicename != NULL) {
		printf("device name='%s'\n",devicename);
		if (get_local_ipv4name_using_ioctl(devicename, ip))
			return TRUE;
	} else {
		Log("no device name found\n");
	}
	if (!get_local_ipv4name_using_ifconfig(name, sizeof(name)))
		if (gethostname(name, 256)) {
			Log("Failed to get the machine's name\n");
			return FALSE;
		}
	printf("Local name = %s\n", name);
	hp = gethostbyname2(name, AF_INET);
	if (hp == 0) {
		Log("This machine does not have an IPv4 address\n");
		inet_pton(AF_INET, "127.0.0.1", ip);
		return TRUE;
	}
	assert(hp->h_length == 4);
	bcopy(hp->h_addr, ip, hp->h_length);
	return TRUE;
}

/*
 The ioctl SIOCGIFADDR works fine for v4 but usually returns EINVAL for v6.
 */

// Get the local IPv6 address (device dev) using command "ifconfig"
static gboolean get_local_ipv6name_using_ifconfig(const char *dev, char *buf,
		int buf_len) {
	if (system("/sbin/ifconfig | grep inet6 | grep -i 'global' | head -1 | awk '{ print $2 }' > /tmp/lixo0123456789.txt")
				< 0)
		return FALSE;
	FILE *fd = fopen("/tmp/lixo0123456789.txt", "r");
	int n = fread(buf, 1, buf_len, fd);
	fclose(fd);
	unlink("/tmp/lixo0123456789.txt");

	if (n <= 0)
		return FALSE;
	if (n >= 256)
		return FALSE;
	char *p = strchr(buf, '\n');
	if (p == NULL)
		return FALSE;
	*p = '\0';
	return TRUE;
}

// Get the local IPv6 address
gboolean init_local_ipv6(struct in6_addr *ip) {
	char name[256];
	struct hostent *hp;

	assert(ip != NULL);
	if (devicename == NULL) {
		Log("No network device available\n");
	}
	if (!get_local_ipv6name_using_ifconfig(devicename, name, sizeof(name)))
		if (gethostname(name, 256)) {
			Log("Failed to get the machine's name\n");
			return FALSE;
		}
	printf("Local name = %s\n", name);
	hp = gethostbyname2(name, AF_INET6);
	if (hp == 0) {
		Log("This machine does not have an IPv6 global address\n");
		inet_pton(AF_INET6, "::1", ip);
		return TRUE;
	}
	assert(hp->h_length == 16);
	bcopy(hp->h_addr, ip, hp->h_length);
	return TRUE;
}


// Return TRUE if 'ip_str' is a local address
// BUGS: It only works for 1 address per host
gboolean is_local_ip(const char *ip_str) {
  assert(ip_str != NULL);
  set_local_IP();

  if (strchr(ip_str, ':') != NULL) {
    // IPv6
    if (!strcmp(ip_str, "::1"))
      return TRUE;
    if (valid_local_ipv6)
      return !strcmp(ip_str, addr_ipv6(&local_ipv6));
    return FALSE;
  }
  // IPv4
  if (!strncmp("127.", ip_str, 4))
    return TRUE;
  if (valid_local_ipv4) {
      return !strcmp(ip_str, addr_ipv4(&local_ipv4));
  }
  return FALSE;
}


// Convert "::1" to the local global address
void translate_local_ip(struct in6_addr *ip) {
  assert(ip != NULL);
  if (valid_local_ipv6 && !strcmp(addr_ipv6(ip), "::1"))
    // substitui
    bcopy(&local_ipv6, ip, 16);
}


// Convert IPv4 address to the IPv6 equivalent address ::ffff:IPv4
gboolean translate_ipv4_to_ipv6(const char *ipv4_str, struct in6_addr *ipv6) {
  assert((ipv4_str != NULL) && (ipv6 != NULL));
  char temp_address[80];
  sprintf(temp_address, "::ffff:%s", ipv4_str);
  return inet_pton(AF_INET6, temp_address, ipv6);
}

// Read an IPv6 Multicast address
gboolean get_IPv6(const gchar *textIP, struct in6_addr *addrv6) {
  struct in_addr addrv4;

  assert(addrv6 != NULL);
  if (inet_pton(AF_INET, textIP, &addrv4)) {
    Log("Invalid address: sockets IPv6 do not support IPv4 multicast addresses\n");
    return FALSE;
  } else if (inet_pton(AF_INET6, textIP, addrv6)) {
    if ((textIP[0]=='f' || textIP[0]=='F') && (textIP[1]=='f' || textIP[1]=='F'))
      return TRUE;
    else {
      Log("The IPv6 address is not multicast (it must be in the range FF01:: - FF1E::)\n");
      return FALSE;
    }
  } else {
    Log("Invalid address\n");
    return FALSE;
  }
}

// Read an IPv4 Multicast address
gboolean get_IPv4(const gchar *textIP, struct in_addr *addrv4) {
  assert(addrv4 != NULL);
  if (inet_pton(AF_INET, textIP, addrv4)) {
    if (IN_MULTICAST(htonl(addrv4->s_addr)))
      return TRUE;
    else {
      Log("The address IPv4 is not multicast (it must be in the range 224.0.0.0 - 239.255.255.255)\n");
      return FALSE;
    }
  } else {
    Log("Invalid address\n");
    return FALSE;
  }
}

// Return a static temporary string with an IPv4 address
char *addr_ipv4(struct in_addr *addr) {
	static char buf[16];
	inet_ntop(AF_INET, addr, buf, sizeof(buf));
	return buf;
}

// Return a static temporary string with an IPv6 address
char *addr_ipv6(struct in6_addr *addr) {
	static char buf[100];
	inet_ntop(AF_INET6, addr, buf, sizeof(buf));
	return buf;
}

// Initialize an IPv4 socket
//  dom = SOCK_DGRAM or SOCK_STREAM
//  Returns: -1 - error;  >0 - socket number
int init_socket_ipv4(int dom, int porto, gboolean shared) {
	struct sockaddr_in name;
	int s;

	// Create an IPv4 socket
	s = socket(AF_INET, dom, 0);
	if (s < 0) {
		perror("IPv4 socket creation");
		return -1;
	}

	if (shared) {
		/* Makes the IP/port of the socket sharable - allows several servers to be associated
		 * to the same port in the same IP address */
		int reuse = 1;

		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse,
				sizeof(reuse)) < 0) {
			perror("IPv4 setsockopt SO_REUSEADDR failed");
			close(s);
			return -1;
		}
	}

	// Associate a socket to a port number
	name.sin_family = AF_INET; // IPv4 address domain
	name.sin_addr.s_addr = INADDR_ANY; // IP local host (0.0.0.0)
	name.sin_port = htons((short) porto); // Port number
	if (bind(s, (struct sockaddr *) &name, sizeof(name))) {
		if (errno == EINVAL) {
			Log("The IPv4 socket is already associated to a port\n");
		}
		perror("IPv4 port number association");
		return -1;
	}
	return s;
}

// Initialize an IPv6 socket
//  dom = SOCK_DGRAM or SOCK_STREAM
//  Returns: -1 - error;  >0 - socket number
int init_socket_ipv6(int dom, int port, gboolean shared) {
	struct sockaddr_in6 name;
	int s;

	// Create an IPv6 socket
	s = socket(AF_INET6, dom, 0);
	if (s < 0) {
		perror("IPv6 socket creation");
		return -1;
	}

	if (shared) {
		/* Make the IP/port of the socket sharable - allows several servers to be associated
		 * to the same port in the same IP address */
		int reuse = 1;

		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse,
				sizeof(reuse)) < 0) {
			perror("IPv6 setsockopt SO_REUSEADDR failed");
			close(s);
			return -1;
		}
	}

	// Associate a socket to a port number
	name.sin6_family = AF_INET6; // IPv6 address domain
	name.sin6_addr = in6addr_any; // ::
	name.sin6_flowinfo = 0;
	name.sin6_port = htons((short) port); // Port number
	if (bind(s, (struct sockaddr *) &name, sizeof(name))) {
		if (errno == EINVAL) {
			Log("The IPv6 socket is already associated to a port\n");
		}
		perror("IPv4 port number association");
		return -1;
	}
	return s;
}

// Return the port number associated to a socket
int get_portnumber(int s) {
	struct sockaddr_in6 name;
	guint n = sizeof(name);
	if (s <= 0)
		return 0;
	if (getsockname(s, (struct sockaddr *) &name, &n)) {
		perror("Error getting port number");
		return 0;
	}
	if (name.sin6_family == AF_INET) {
		// It is an IPv4 socket
		struct sockaddr_in *name4 = (struct sockaddr_in *) &name;
		return ntohs(name4->sin_port);
	}
	if (name.sin6_family != AF_INET6)
		return 0;
	return ntohs(name.sin6_port);
}

// Read data from an IPv4 socket
// Returns the number of byte read (<0 in case of error) and the sender's address and port
int read_data_ipv4(int sock, char *buf, int n, struct in_addr *ip,
		short unsigned int *port) {
	int m;
	struct sockaddr_in from;
	guint fromlen = sizeof(from);

	assert(buf != NULL);
	assert(n > 0);
	assert(ip != NULL);
	assert(port != NULL);
	if (sock < 0)
		return -1;
	if ((m = recvfrom(sock, buf, n, MSG_DONTWAIT /* non-blocking */,
			(struct sockaddr *) &from, &fromlen)) < 0)
		return m;
	*ip = from.sin_addr; // IP in network format (Big Endian)
	*port = ntohs(from.sin_port); // Converts the port number to machine format (Little Endian on Intel CPU)
	return m;
}

// Read data from an IPv6 socket
// Returns the number of byte read (<0 in case of error) and the sender's address and port
int read_data_ipv6(int sock, char *buf, int n, struct in6_addr *ip,
		short unsigned int *port) {
	int m;
	struct sockaddr_in6 from;
	guint fromlen = sizeof(from);

	assert(buf != NULL);
	assert(n > 0);
	assert(ip != NULL);
	assert(port != NULL);
	if (sock < 0)
		return -1;
	if ((m = recvfrom(sock, buf, n, MSG_DONTWAIT /* non blocking */,
			(struct sockaddr *) &from, &fromlen)) < 0)
		return m;
	*ip = from.sin6_addr; // IP in network format (Big Endian)
	*port = ntohs(from.sin6_port); // Converts the port number to machine format (Little Endian on Intel CPU)
	return m;
}

// Create a GIOchannel object and regist a callback function in the GIO main loop
// event = G_IO_IN ; G_IO_OUT; G_IO_IN | G_IO_OUT
gboolean put_socket_in_mainloop(int sock, void *ptr, guint *chan_id, GIOChannel **chan,
		guint event, gboolean(*callback)(GIOChannel *, GIOCondition, gpointer)) {
	/* Test parameters */
	assert(sock >= 0);
	assert(chan_id != NULL);
	assert(chan != NULL);
	assert(callback != NULL);

	/* create a GIO descriptor associated to an io device descriptor (socket or pipe) */
	if ((*chan = g_io_channel_unix_new(sock)) == NULL) {
		Log("Failed creation of IO channel\n");
		return FALSE;
	}
	/* add the GIO descriptor to the Gtk main loop, registering the callback function */
	if (!(*chan_id
			= g_io_add_watch(*chan, event | G_IO_NVAL | G_IO_ERR, /* read and error events */
			callback /* callback function */, ptr /* callback function parameter's value */))) {
		Log("Failed activation of GIO callback function\n");
		return FALSE;
	}
	return TRUE;
}

// Regists a callback function in the GIO main loop
// event = G_IO_IN ; G_IO_OUT; G_IO_IN | G_IO_OUT
gboolean restore_socket_in_mainloop(int sock, void *ptr, guint *chan_id, GIOChannel *chan,
		guint event, gboolean(*callback)(GIOChannel *, GIOCondition, gpointer)) {
	/* Test parameters */
	assert(sock >= 0);
	assert(chan_id != NULL);
	assert(chan != NULL);
	assert(callback != NULL);

	/* add the GIO descriptor to the Gtk main loop, registering the callback function */
	if (!(*chan_id
			= g_io_add_watch(chan, event | G_IO_NVAL | G_IO_ERR, /* read and error events */
			callback /* callback function */, ptr /* callback function parameter's value */))) {
		Log("Failed reactivation of GIO callback function\n");
		return FALSE;
	}
	return TRUE;
}

// Cancel the callback registration in the GIO main loop
void suspend_socket_from_mainloop(int sock, int chan_id) {
	assert(sock >= 0);
	/* Remove socket from Gtk main loop */
	g_source_remove(chan_id);
}

// Free GIO channel resources
void free_gio_channel(GIOChannel *chan) {
	assert(chan != NULL);
	g_io_channel_shutdown(chan, TRUE, NULL);
	g_io_channel_unref(chan);
}

// Cancel callback registration in the GIO main loop and free the resources allocated
void remove_socket_from_mainloop(int sock, guint chan_id, GIOChannel *chan) {
	assert(sock >= 0);
	assert(chan != NULL);
	/* Remove socket from Gtk main loop */
	g_source_remove(chan_id);
	/* Free the gio descriptor */
	free_gio_channel(chan);
}

// Close the socket
void close_socket(int sock)
{
	if (sock < 0)
		return;
	close(sock);
}
