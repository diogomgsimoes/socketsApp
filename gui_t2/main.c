/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * main.c
 *
 * Main function
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
 \*****************************************************************************/

#include <gtk/gtk.h>
#include <arpa/inet.h>
#include <assert.h>
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "gui.h"
#include "file.h"
#include "sock.h"
#include "callbacks.h"

/* Public variables */
WindowElements *main_window; // Pointer to all elements of main window
char *out_dir;


// main function
int main(int argc, char *argv[]) {
	char newEntry[256];

	/* allocate the memory needed by our TutorialTextEditor struct */
	main_window = g_slice_new (WindowElements);

	/* initialize GTK+ libraries */
	gtk_init(&argc, &argv);

	if (init_app(main_window) == FALSE)
		return 1; /* error loading UI */
	gtk_widget_show(main_window->window);

#ifdef __i386__
	Log("You are in a 32 bit OS\n");
#endif
#ifdef __x86_64__
	Log("You are in a 64 bit OS\n");
#endif

	// Get local IP
	set_local_IP();
	if (valid_local_ipv4)
		set_LocalIPv4(addr_ipv4(&local_ipv4));
	if (valid_local_ipv6)
		set_LocalIPv6(addr_ipv6(&local_ipv6));

	// Defines local name using the pid (process id)
	sprintf(newEntry, "p%d", getpid());
	gtk_entry_set_text(main_window->entryName, newEntry);

	// Defines the output directory, where the files will be written
	char *homedir = getenv("HOME");
	if (homedir == NULL)
		homedir = getenv("PWD");
	if (homedir == NULL)
		homedir = "/tmp";
	out_dir = g_strdup_printf("%s/out%d", homedir, getpid());
	if (!make_directory(out_dir)) {
		Log("Failed creation of output directory: '");
		Log(out_dir);
		Log("'.\n");
		out_dir = "/tmp";
	}
	Log("Received files will be created at: '");
	Log(out_dir);
	Log("'\n");

	// Infinite loop handled by GTK+3.0
	gtk_main();

	/* free memory we allocated for TutorialTextEditor struct */
	g_slice_free(WindowElements, main_window);

	return 0;
}

