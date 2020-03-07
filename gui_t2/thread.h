/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * thread.h
 *
 * Header file of functions that handle file transfer threads
 *
 * Created on October 8, 2019,16:00
 * @author  Luis Bernardo
\*****************************************************************************/
extern char *out_dir;

#ifndef SUBPROCESS_INC_
#define SUBPROCESS_INC_

#include <gtk/gtk.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "callbacks.h"

//#define DEBUG

/*******************************************************\
|* Functions that implement file transmission threads  *|
\*******************************************************/

// Starts a subprocess for file reception
Thread_Data *start_rcv_file_thread (int msgsock, struct in6_addr *ip, u_short port,
		const char *filename, gboolean optimal);
// File receiving thread
void *rcv_file_thread (void *ptr);
// Starts subprocess for sending a file
Thread_Data *start_snd_file_thread (struct in6_addr *ip_file, u_short port,
		const char *nome, const char *filename, gboolean optimal);
// File send thread
void *snd_file_thread (void *ptr);


#endif
