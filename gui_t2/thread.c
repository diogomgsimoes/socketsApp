/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * thread.c
 *
 * Functions that implement file transfer threads
 *
 * Created on October 8, 2019,16:00
 * @author  Luis Bernardo
\*****************************************************************************/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>       
#include <sys/wait.h>
#include <sys/types.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include "thread.h"
#include "callbacks.h"
#include "sock.h"
#include "file.h"
#include "gui.h"
#include <netinet/tcp.h>

#ifdef DEBUG
#define debugstr(x)     g_print("%s", x)
#else
#define debugstr(x)
#endif

#define SLOW_SLEEPTIME	500000		// Sleep time between reads and writes in slow sending


/*******************************************************\
|* Functions that implement file transmission threads  *|
\*******************************************************/


// Auxiliary macro that stops a thread and frees the descriptor
#define STOP_THREAD(pt) { if (pt->self == pt) \
								free_file_thread_desc((unsigned)pt->tid, pt); \
						  return NULL; \
						}

// Auxiliary macro that tests if a thread has been stopped and frees the descriptor
#define TEST_INTERRUPTED(pt) { if (!active || (pt->self!=pt) || pt->finished) {  \
									if (pt->self==pt) \
										g_print("%s interrupted\n", pt->name_str); \
									STOP_THREAD(pt); \
								} \
							 }


// Starts a thread for receiving a file
void *rcv_file_thread (void *ptr)
{
	assert(ptr!=NULL);
	Thread_Data *pt= (Thread_Data *)ptr;

#define RCV_BUFLEN 63*1024
	// Starts a thread that receives data from the TCP socket
	char buf[RCV_BUFLEN+1];
	char nome_p[129];
	char f_name[256];
	short int slen;
	short int flen;
	long n, m;
	short int c, last_c;
	struct timeval tv1, tv2;
	struct timezone tz;
	long diff= 0;
	struct timeval tv;
	long len = 63*1024;

	// *************************************************************************************
	// *      THREAD                                                                   *
	// *************************************************************************************

	sprintf(pt->name_str, "RCV(%u)> ", (unsigned)pt->tid);
	fprintf(stderr, "%s started receiving thread (tid = %u)\n", pt->name_str, (unsigned)pt->tid);
	buf[RCV_BUFLEN]= '\0';

	// Don't forget to configure your socket to define a timeout time for reading operations and
	// to set buffers or other any configuration that maximizes throughput

	// configure rcv buffer size
	setsockopt(pt->s, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
	// configure timeout -> 10 seconds
	tv.tv_sec = 10;
	setsockopt(pt->s, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

	// Receive header
	// Read and validate the user name length
	if (!active || (pt->self != pt) || pt->finished || read(pt->s, &slen, sizeof(slen)) != sizeof(slen)) {
		g_print("%s did not receive the user name length - aborting\n", pt->name_str);
		STOP_THREAD(pt);
	}
	if (slen> 129) {
		g_print("%s invalid user name length - aborting\n", pt->name_str);
		STOP_THREAD(pt);
	}

	// Read and validate the user name
	if (!active || (pt->self != pt) || pt->finished || read(pt->s, nome_p, slen) != slen) {
		g_print("%s did not receive the user name - aborting\n", pt->name_str);
		STOP_THREAD(pt);
	}
	if (nome_p[slen-1] != '\0') {
		g_print("%s user name does not have '\\0'- aborting\n", pt->name_str);
		STOP_THREAD(pt);
	}

	// TASK 5:
	// Complete the reading thread code

	// Read the file name length
	if (!active || (pt->self != pt) || pt->finished || read(pt->s, &flen, sizeof(flen)) != sizeof(flen)) {
		g_print("%d did not receive the file name length - aborting\n", flen);
		STOP_THREAD(pt);
	}
	if (flen > 257) {
		g_print("%d invalid file name length - aborting\n", flen);
		STOP_THREAD(pt);
	}

	// Read the file name
	if (!active || (pt->self != pt) || pt->finished || read(pt->s, f_name, flen) != flen) {
		g_print("%s did not receive the file name - aborting\n", pt->fname);
		STOP_THREAD(pt);
	}
	if (f_name[flen-1] != '\0') {
		g_print("%s invalid file name - aborting\n", pt->fname);
		STOP_THREAD(pt);
	}

	// Read the file length and store it in pt->flen
	if (!active || (pt->self != pt) || pt->finished || read(pt->s, &pt->flen, sizeof(pt->flen)) != sizeof(pt->flen)) {
		g_print("%lld did not receive the file name - aborting\n", pt->flen);
		STOP_THREAD(pt);
	}
	if (pt->flen < 0) {
		g_print("%lld invalid file length - aborting\n", pt->flen);
		STOP_THREAD(pt);
	}

	// update gui with read fields
	GUI_update_thread_info((unsigned)pt->tid, nome_p, f_name);

	g_print("%s receiving file %s from %s with %lld bytes\n", pt->name_str, f_name, nome_p, pt->flen);

	// Open file for writing
	if ((pt->f= fopen(pt->fname, "w")) == NULL) {
		perror("Error creating file for writing");
		fprintf(stderr, "%s failed to create file '%s' for writing\n", pt->name_str, pt->fname);
		STOP_THREAD(pt);
	}

	// Memorize the time when transmission started
	if (gettimeofday(&tv1, &tz))
		Log("Error getting the time to start reception\n");

	// Receive file from pt->s and store it in the output file pt->f
	// See the file copy example in the documentation, and adapt to a socket scenario ...
	// Loop forever until end of file
	do {
		// read from buffer
		n = read(pt->s, buf, RCV_BUFLEN);
		// add bytes read to pt->total
		pt->total += n;
		// if read was sucessfull
		if (n > 0){
			if ((m = fwrite(buf, 1, n, pt->f) != n))
				break;
		}
		// if not sucessfull
		else {
			if(n == 0)
				g_print("transfer completed\n");
			if(n < 0) {
				g_print("transfer error\n");
				STOP_THREAD(pt);
			}
		}
		// calculate the percentage of file already sent
		c = (int)((pt->total*100.0)/pt->flen);
		// if the percentage changed since last iteration
		if(c != last_c){
			GUI_update_bytes_sent((unsigned)pt->tid, c);
			last_c = c;
		}
		// if percentage reaches 100, flag finished activated
		if (c == 100)
			pt->finished = 1;
		// if slow mode activated
		if (pt->slow)
			usleep(SLOW_SLEEPTIME);
	 } while (active && (n > 0) && (pt->flen - pt->total) > 0 && !pt->finished);
	// while the EOF isn't reached or flag finished not true

	//close fill and clear pointer
	fclose(pt->f);
	pt->f= NULL;

	if (gettimeofday(&tv2, &tz)) {
		Log("Error getting the time to stop reception\n");
		diff= 0;
	} else
		diff= (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);

	sprintf(buf, "%s receiving thread ended - lasted %ld usec\n",
			pt->name_str, diff);
	Log(buf);

	STOP_THREAD(pt);

	//*************************************************************************************
	//*      end of Thread                                                        *
	//*************************************************************************************
}

// Starts a thread for file reception
Thread_Data *start_rcv_file_thread (int msgsock, struct in6_addr *ip, u_short port,
		const char *filename, gboolean slow)
{
	if (!active)
		return NULL;
	Thread_Data *pt= new_file_thread_desc(FALSE, ip, port, filename, slow);
	// Store the socket information
	pt->s= msgsock;
	// Start the thread
	if (pthread_create(&pt->tid, NULL, rcv_file_thread, (void *)pt)) {
		fprintf(stderr, "main: error starting thread\n");
		free (pt);
		return NULL;
	}
	// Add to the FList table
	GUI_regist_thread((unsigned)pt->tid, "RCV", "?", filename);
	return pt;
}

// Starts thread for sending a file
void *snd_file_thread (void *ptr)
{
	assert(ptr!=NULL);
	Thread_Data *pt= (Thread_Data *)ptr;

#define SND_BUFLEN 63*1024
	struct timeval tv1, tv2;
	struct timezone tz;
	char buf[SND_BUFLEN+1];
	long diff= 0;
	short int slen;
	struct sockaddr_in6 server;
	short int flen;
	short int c, last_c = 0;
	long n, m;
	struct timeval tv;
	int len = 63*1024;

	//*************************************************************************************
	//*      THREAD                                                                       *
	//*************************************************************************************

	sprintf(pt->name_str, "SND(%u)> ", (unsigned)pt->tid);
	fprintf(stderr, "%sstarted sending subprocess (file= '%s' tid = %u)\n", pt->name_str, pt->fname, (int)pt->tid);
	buf[SND_BUFLEN]= '\0';

	// TASK 8:

	// Create a new temporary socket TCP IPv6 to send the file
	pt->s = socket(AF_INET6, SOCK_STREAM, 0);

	// check if the socket creation was sucessfull
	if (pt->s < 0) {
		perror("opening stream socket");
		exit(1);
	}

	// associate socket to port
	server.sin6_family = AF_INET6;
	server.sin6_flowinfo= 0;
	server.sin6_port = htons(pt->port);
	server.sin6_addr = pt->ip;

	unsigned int length = sizeof(server);

	/* Connect the socket to (pt->ip : pt->port) */
	if (connect(pt->s, (struct sockaddr *)&server, length) < 0){
		perror("SND>error connecting the TCP socket to send the file");
		fprintf(stderr, "%sconnection failed\n", pt->name_str);
		STOP_THREAD(pt);
	}

	short aux = 1;

	// socket description of send buffer size
	setsockopt(pt->s, SOL_SOCKET, SO_SNDBUF, &aux, sizeof(len));
	// socket description to use the nagle algorithm
	setsockopt(pt->s, IPPROTO_TCP, TCP_NODELAY, &aux, sizeof(aux));
	// socket description of maximum timeout time -> 10 seconds
	tv.tv_sec = 10;
	setsockopt(pt->s, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

	// Open file
	if ((pt->f= fopen(pt->fname, "r")) == NULL) {
		perror("Error opening file");
		STOP_THREAD(pt);
	}

	// Get the file length
	pt->flen= get_filesize(pt->fname);

	// Send the user name length
	slen= strlen(user_name)+1;
	if (!active || (pt->self != pt) || pt->finished || send(pt->s, &slen, sizeof(slen), 0) < 0) {
		g_print("%s failed sending user name length - aborting\n", pt->name_str);
		STOP_THREAD(pt);
	}

	// Validate and send the user name
	if (!active || (pt->self != pt) || pt->finished || send(pt->s, user_name, slen, 0) < 0) {
		g_print("%s did not send the user name - aborting\n", pt->name_str);
		STOP_THREAD(pt);
	}

	// Prepare the filename removing the path part from the complete pathname using
	// the function get_trunc_filename

	strcpy(pt->fname, get_trunc_filename(pt->fname));

	flen = strlen(pt->fname)+1;
	// Send the file name length
	if (!active || (pt->self != pt) || pt->finished || send(pt->s, &flen, sizeof(flen), 0) < 0) {
		g_print("%d did not send the file name length - aborting\n", flen);
		STOP_THREAD(pt);
	}

	// Send the file name
	if (!active || (pt->self != pt) || pt->finished || send(pt->s, pt->fname, flen, 0) < 0) {
		g_print("%s did not send the file name - aborting\n", pt->fname);
		STOP_THREAD(pt);
	}

	// Send the file length
	if (!active || (pt->self != pt) || pt->finished || send(pt->s, &pt->flen, sizeof(pt->flen), 0) < 0) {
		g_print("%lld did not send the file name - aborting\n", pt->flen);
		STOP_THREAD(pt);
	}

	g_print("%s sending file %s from %s with %lld bytes\n", user_name, pt->fname, pt->nome, pt->flen);

	if (gettimeofday(&tv1, &tz))
		Log("Error getting the time to start sending\n");

	// Send the file contents from pt->f to pt->s
	// See the file copy example in the documentation, and adapt to a socket scenario ...
	// Loop forever until end of file
	do {
		// read from buffer
		n = fread(buf, 1, SND_BUFLEN, pt->f);
		// add bytes sent
		pt->total += n;
		// if read was sucessfull
		if (n > 0) {
			if ((m = write(pt->s, buf, n)) < 0)
				break;
		}
		// if not sucessfull
		else {
			if(n == 0)
				g_print("transfer completed\n");
			if(n < 0) {
				g_print("transfer error\n");
				STOP_THREAD(pt);
			}
		}
		// calculate the percentage of file already sent
		c = (int)((pt->total*100.0)/pt->flen);
		// if the percentage changed since last iteration
		if(c != last_c){
			GUI_update_bytes_sent((unsigned)pt->tid, c);
			last_c = c;
		}
		// if percentage reaches 100, flag finished activated
		if (c == 100)
			pt->finished = 1;
		// if slow mode activated
		if (pt->slow)
			usleep(SLOW_SLEEPTIME);
	} while (active && (n > 0) && (pt->flen - pt->total) > 0 && !pt->finished);
	// while the EOF isn't reached or flag finished not true

	//close fill and clear pointer
	fclose(pt->f);
	pt->f= NULL;

	if (gettimeofday(&tv2, &tz)) {
		Log("Error getting the time to stop sending\n");
		diff= 0;
	} else
		diff= (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);

	sprintf(buf, "%ssending thread ended - lasted %ld usec\n",
			pt->name_str, diff);

	Log(buf);
	STOP_THREAD(pt);

	//*********************************************************************************
	//*      end of Thread                                                            *
	//*********************************************************************************
}


// Starts a thread for file reception
Thread_Data *start_snd_file_thread (struct in6_addr *ip_file, u_short port,
		const char *nome, const char *filename, gboolean slow)
{
	assert(ip_file != NULL);
	assert(nome != NULL);
	assert(filename != NULL);

	Thread_Data *pt= new_file_thread_desc(FALSE, ip_file, port, filename, slow);
	// Store the name information
	strncpy(pt->nome, nome, sizeof(pt->nome));

	// TASK 7:
	// Start the thread
	// ...
	// Update the FList table

	// Check if pt is not null
	assert(pt != NULL);

	// Start the thread
	if (pthread_create(&pt->tid, NULL, snd_file_thread, (void *)pt)) {
		fprintf(stderr, "main: error starting thread\n");
		free (pt);
		return NULL;
	}

	// Update the Flist table
	GUI_regist_thread((unsigned)pt->tid, "SND", nome, filename);
	return pt;
}
