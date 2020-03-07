/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * gui.h
 *
 * Header file of functions that handle graphical user interface interaction
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/

#ifndef _INCL_GUI_H
#define _INCL_GUI_H

#include <gtk/gtk.h>
#include <netinet/in.h>

/* store the widgets which may need to be accessed in a typedef struct */
typedef struct
{
        GtkWidget               *window;
        GtkEntry				*entryMIP;
        GtkEntry				*entryMPort;
        GtkEntry				*entryName;
        GtkToggleButton			*active;
        GtkCheckButton			*check_ip4;
        GtkEntry				*entryIPv4loc;
        GtkCheckButton			*check_ip6;
        GtkEntry				*entryIPv6loc;
        GtkEntry				*entryTCPPort;
        GtkTreeView				*treeUsers;
        GtkListStore			*listUsers;
        GtkEntry				*FileName;
        GtkCheckButton			*check_Slow;
        GtkTreeView				*treeFiles;
        GtkListStore			*listFiles;
        GtkTextView				*textView;
} WindowElements;

/** temporary struct to return lists of users */
typedef struct {
		gchar 					*name;
		gchar 					*ip;
		int						 port;
} UserInfo;


// Global pointer to the main window elements
extern WindowElements *main_window;

// Initialization function
gboolean init_app (WindowElements *window);

// Log the message str to the textview and command line
void Log (const gchar * str);


/********************************************************\
|* Functions for writing and reading from main window   *|
\********************************************************/
// Write a message to the GtkTextView window
void Log(const gchar *str);
// Return the multicast port number
int get_portM_number(void);
// Write the TCP port in the GtkEntry box
void set_portT_number(u_short porto_TCP);
// Read the multicast address from the box and converts it to the numerical format (addrv4 or addrv6 depending on is_ipv6)
const char *get_IPmult(gboolean *is_ipv6, struct in_addr *addrv4,
	struct in6_addr *addrv6);
// Return the value of the CheckButton "Slow"
gboolean get_slow(void);
// Set the content of Local IPv6
void set_LocalIPv6(const char *addr);
// Set the content of Local IPv4
void set_LocalIPv4(const char *addr);
// Block edition of GtkEntry boxes
void block_entrys(gboolean editable);

/******************************************************************\
|* Functions to handle the graphical table with the users list    *|
\******************************************************************/
// Search for a user in the GUI users list by name
gboolean GUI_locate_User_by_name(const char *name, GtkTreeIter *iter);
// Search for a user in the GUI users list by ip and port
gboolean GUI_locate_User_by_ip(const char *ip, int port, GtkTreeIter *iter);
// Search for a user in the GUI users list by name, ip and port
gboolean GUI_locate_User_by_name_and_ip(const char *name, const char *ip, int port, GtkTreeIter *iter);
// Regist name in the GUI table
gboolean GUI_regist_name(const char *name, const char *ip, int port);
// Remove the name 'name' from the GUI table
gboolean GUI_cancel_name(const char *name, const char *ip_str, int port);
// Clear the GUI names' list
void GUI_clear_names();
// Get selected user data; returns FALSE if none is selected; returns TRUE and iter pointing to the line
gboolean GUI_get_selected_User(char **ip, int *port, char **name, GtkTreeIter *iter);
// Set column 3 (timer counter) with value "0" of GUI's name list
void GUI_reset_name_timer(GtkTreeIter *iter);
// Increments the value in column 3 and returns TRUE if it is <3 for a name in GUI list
gboolean GUI_test_name_timer (GtkTreeIter *iter);
// Test column 3 for all users and return a list with all that are overdue
GList *GUI_test_all_name_timer (void);
// Free the memory allocated in a UserInfo list returned by GUI_test_all_name_timer
void GUI_free_UserInfo_list (GList *list);


/****************************************************************\
|* Functions to handle the GUI file transfer subprocess table   *|
\****************************************************************/
// Search for a subprocess in the GUI file transfer subprocess list
gboolean GUI_locate_thread_by_tid(unsigned tid, GtkTreeIter *iter);
// Regist a subprocess in GUI subprocess table
gboolean GUI_regist_thread(unsigned tid, const char *type,
		const char *name, const char *f_name);
// Update the bytes sent in GUI subprocess table
gboolean GUI_update_bytes_sent(unsigned tid, int trans);
// Update the filename in GUI subprocess table (to complete information)
gboolean GUI_update_thread_info(unsigned tid, const char *name, const char *name_f);
// Cancels a file transfer subprocess from GUI table
gboolean GUI_remove_thread(unsigned tid);
// Clear the GUI subprocess' list
void GUI_clear_threads();
// Get selected subprocess data; returns FALSE if none is selected; returns TRUE and iter pointing to the line
gboolean GUI_get_selected_Filetx(unsigned *tid, GtkTreeIter *iter);


/*************************************************************\
|* Functions to support the selection of a file to transmit  *|
\*************************************************************/
// Create a window to select a file to open
char *open_select_filename_window();
// Handler of filename button
void on_buttonFilename_clicked(GtkButton *button, gpointer user_data);

/***************************\
|*   Auxiliary functions   *|
\***************************/

// Create a window with an error message and outputs it to the command line
void error_message (const gchar *message);

/***********************\
|*   Event handlers    *|
\***********************/
// Event handlers in gui_g3.c

// Handles 'Clear' button - clears textMemo
void on_buttonClear_clicked (GtkButton *button, gpointer user_data);

// External event handlers in callbacks.c
void on_togglebutton1_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_buttonStop_clicked (GtkButton *button, gpointer user_data);


#endif



