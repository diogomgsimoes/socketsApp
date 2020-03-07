/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2019/2020
 *
 * gui_g3.c
 *
 * Functions that handle graphical user interface interaction
 *
 * Updated on October 8, 2019,16:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/

#include <gtk/gtk.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gui.h"
#include "callbacks.h"
#include "sock.h"

// Set here the glade file name
#define GLADE_FILE "gui_t2.glade"

// Temporary buffer
static char tmp_buf[8000];

// Mutex to synchronize changes to GUI log window
pthread_mutex_t lmutex = PTHREAD_MUTEX_INITIALIZER;
// Mutex to synchronize changes to GUI database of file transfer threads
pthread_mutex_t gmutex = PTHREAD_MUTEX_INITIALIZER;
// Mutex to synchronize changes to GUI database of users
pthread_mutex_t umutex = PTHREAD_MUTEX_INITIALIZER;

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


// Initialization function
gboolean
init_app (WindowElements *win)
{
        GtkBuilder              *builder;
        GError                  *err=NULL;

        /* use GtkBuilder to build our interface from the XML file */
        builder = gtk_builder_new ();
        if (gtk_builder_add_from_file (builder, GLADE_FILE, &err) == 0)
        {
                error_message (err->message);
                g_error_free (err);
                return FALSE;
        }
        /* get the widgets which will be referenced in callbacks */
        win->window = GTK_WIDGET (gtk_builder_get_object (builder,
                                                             "window1"));
        win->entryMIP = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryMIP"));
        win->entryMPort = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryMPort"));
        win->entryName = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryName"));
        win->active = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder,
                                                             "togglebuttonActive"));
        win->check_ip4 = GTK_CHECK_BUTTON (gtk_builder_get_object (builder,
                                                             "checkbuttonIPv4"));
        win->entryIPv4loc = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryIPv4loc"));
        win->check_ip6 = GTK_CHECK_BUTTON (gtk_builder_get_object (builder,
                                                             "checkbuttonIPv6"));
        win->entryIPv6loc = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryIPv6loc"));
        win->entryTCPPort = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryPortT"));
        win->treeUsers = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                             "treeUsers"));
        win->listUsers = GTK_LIST_STORE (gtk_builder_get_object (builder,
                                                             "liststore_users"));
        win->FileName = GTK_ENTRY (gtk_builder_get_object (builder,
                                                             "entryFileName"));
        win->check_Slow = GTK_CHECK_BUTTON (gtk_builder_get_object (builder,
                                                             "checkbuttonSlow"));
       win->treeFiles = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                             "treeFiletx"));
        win->listFiles = GTK_LIST_STORE (gtk_builder_get_object (builder,
                                                             "liststore_filetrans"));
        win->textView = GTK_TEXT_VIEW (gtk_builder_get_object (builder,
                                                                "textview1"));

        /* connect signals, passing our TutorialTextEditor struct as user data */
        gtk_builder_connect_signals (builder, win);

        /* free memory used by GtkBuilder object */
        g_object_unref (G_OBJECT (builder));

        return TRUE;
}


// Logs the message str to the textview and command line
void Log (const gchar * str)
{
  GtkTextBuffer *textbuf;
  GtkTextIter tend;

  pthread_mutex_lock( &lmutex );
  textbuf = GTK_TEXT_BUFFER (gtk_text_view_get_buffer (main_window->textView));
  gtk_text_buffer_get_iter_at_offset (textbuf, &tend, -1);	// Gets reference to the last position
  // Adds text to the textview
  gtk_text_buffer_insert (textbuf, &tend, g_strdup (str), strlen (str));
  // Adds text to the command line
  g_print("%s", str);
  pthread_mutex_unlock( &lmutex );
}


// Translates a string into its numerical value
static int get_number_from_text(const gchar *text) {
  int n= 0;
  char *pt;

  if ((text != NULL) && (strlen(text)>0)) {
    n= strtol(text, &pt, 10);
    return ((pt==NULL) || (*pt)) ? -1 : n;
  } else
    return -1;
}


// Validates a port number
static int get_PortNumber(GtkEntry *entryText) {
	const char *textPort= gtk_entry_get_text(entryText);
	int port= get_number_from_text(textPort);
	if (port>(powl(2,16)-1))
		port= -1;
	return port;
}


// Returns the multicast port number
int get_portM_number(void) {
  return get_PortNumber(main_window->entryMPort);
}


// Writes the TCP port in the GtkEntry box
void set_portT_number(u_short porto_TCP) {
  gtk_entry_set_text(main_window->entryTCPPort, g_strdup_printf("%hu",
  	(unsigned short int)porto_TCP));
}


// Read the multicast address from the box and converts it to the numerical format (addrv4 or addrv6 depending on is_ipv6)
const char *get_IPmult(gboolean *is_ipv6, struct in_addr *addrv4,
	struct in6_addr *addrv6) {
  assert(is_ipv6 != NULL);
  assert(addrv4 != NULL);
  assert(addrv6 != NULL);
  const gchar *textIP;

  textIP= gtk_entry_get_text(main_window->entryMIP);
  if ((*is_ipv6= (strchr(textIP, ':') != NULL))) {
    return get_IPv6(textIP, addrv6) ? textIP : NULL;
  } else {
    return get_IPv4(textIP, addrv4) ? textIP : NULL;
  }
}


// Return the value of the CheckButton "Slow"
gboolean get_slow(void) {
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_Slow));
}


// Set the content of Local IPv6
void set_LocalIPv6(const char *addr) {
	assert(addr != NULL);
	gtk_entry_set_text(main_window->entryIPv6loc, addr);
}


// Set the content of Local IPv4
void set_LocalIPv4(const char *addr) {
	assert(addr != NULL);
	gtk_entry_set_text(main_window->entryIPv4loc, addr);
}


// Block edition of GtkEntry boxes
void block_entrys(gboolean editable)
{
  gtk_editable_set_editable(GTK_EDITABLE(main_window->entryMIP), editable);
  gtk_editable_set_editable(GTK_EDITABLE(main_window->entryMPort), editable);
  gtk_editable_set_editable(GTK_EDITABLE(main_window->entryName), editable);
}


/******************************************************************\
|* Functions to handle the graphical table with the users list    *|
\******************************************************************/

// Search for a user in the GUI users list by name
gboolean GUI_locate_User_by_name(const char *name, GtkTreeIter *iter) {
	assert(name != NULL);
	assert(iter != NULL);
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
	gboolean valid;

	// Get the first iter in the list
	valid = gtk_tree_model_get_iter_first(list_store, iter);

	while (valid) {
		// Walk through the list, reading each row
		gchar *str_name;

		// Make sure you terminate calls to gtk_tree_model_get() with a '-1' value
		gtk_tree_model_get(list_store, iter, 2, &str_name, -1);

		// Do something with the data
#ifdef DEBUG
		g_print("Lookup for: Name='%s'\n", str_name);
#endif
		if (!strcmp(str_name, name)) {
			g_free(str_name);
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(list_store, iter);
		g_free(str_name);
	}
	return FALSE;
}


// Search for a user in the GUI users list by ip and port
gboolean GUI_locate_User_by_ip(const char *ip, int port, GtkTreeIter *iter) {
	assert(ip != NULL);
	assert(iter != NULL);
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
	gboolean valid;

	// Get the first iter in the list
	valid = gtk_tree_model_get_iter_first(list_store, iter);

	while (valid) {
		// Walk through the list, reading each row
		gchar *str_ip;
		int str_port;

		// Make sure you terminate calls to gtk_tree_model_get() with a '-1' value
		gtk_tree_model_get(list_store, iter, 0, &str_ip, 1, &str_port, -1);

		// Do something with the data
#ifdef DEBUG
		g_print("Lookup for: IP='%s' port=%d\n", str_ip, str_port);
#endif
		if (!strcmp(str_ip, ip) && (str_port==port)) {
			g_free(str_ip);
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(list_store, iter);
		g_free(str_ip);
	}
	return FALSE;
}


// Search for a user in the GUI users list by name, ip and port
gboolean GUI_locate_User_by_name_and_ip(const char *name, const char *ip, int port, GtkTreeIter *iter) {
	assert(name != NULL);
	assert(ip != NULL);
	assert(iter != NULL);
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
	gboolean valid;

	// Get the first iter in the list
	valid = gtk_tree_model_get_iter_first(list_store, iter);

	while (valid) {
		// Walk through the list, reading each row
		gchar *str_name;
		gchar *str_ip;
		int str_port;

		// Make sure you terminate calls to gtk_tree_model_get() with a '-1' value
		gtk_tree_model_get(list_store, iter, 0, &str_ip, 1, &str_port, 2, &str_name, -1);

		// Do something with the data
#ifdef DEBUG
		g_print("Lookup for: Name='%s' IP='%s' port=%d\n", str_name, str_ip, str_port);
#endif
		if (!strcmp(str_name, name) && !strcmp(str_ip, ip) && (str_port==port)) {
			g_free(str_name);
			g_free(str_ip);
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(list_store, iter);
		g_free(str_name);
		g_free(str_ip);
	}
	return FALSE;
}


// Regist name in the GUI table
gboolean GUI_regist_name(const char *name, const char *ip, int port)
{
	assert(name != NULL);
	assert(ip != NULL);

	GtkTreeIter iter;

	LOCK_MUTEX(&umutex, "lock_u1\n");
	if (GUI_locate_User_by_name_and_ip(name, ip, port, &iter)) {
		// Name already in the table
		GUI_reset_name_timer(&iter);
		UNLOCK_MUTEX(&umutex, "unlock_u1\n");
		return FALSE;
	} else {
		// New registration
		if (GUI_locate_User_by_ip(ip, port, &iter)) {
			sprintf(tmp_buf, "WARNING: The user at %s:%d did not cancel its previous name\n",
				ip, port);
			Log(tmp_buf);
			// Registration will be replaced
		} else if (GUI_locate_User_by_name(name, &iter)) {
			sprintf(tmp_buf, "WARNING: Duplicate name registered '%s'\n", name);
			Log(tmp_buf);
			gtk_list_store_append(main_window->listUsers, &iter);
		} else {
			// New registration
			gtk_list_store_append(main_window->listUsers, &iter);
		}
		if (GTK_IS_LIST_STORE(main_window->listUsers)) {
			gtk_list_store_set(main_window->listUsers, &iter, 0, ip, 1, port,
					2, name, 3, 0, -1);
			UNLOCK_MUTEX(&umutex, "unlock_u1\n");
			return TRUE;
		} else {
#ifdef DEBUG
			Log("Internal Error: Users' list store not active\n");
#endif
			UNLOCK_MUTEX(&umutex, "unlock_u1\n");
			return FALSE;
		}
	}
}


// Remove the name 'name' from the GUI table
gboolean GUI_cancel_name(const char *name, const char *ip, int port)
{
	assert(name != NULL);
	assert(ip != NULL);
	GtkTreeIter iter;

	LOCK_MUTEX(&umutex, "lock_u2\n");
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
	if (GUI_locate_User_by_name_and_ip(name, ip, port, &iter)) {
		gtk_list_store_remove(GTK_LIST_STORE(list_store), &iter);
		UNLOCK_MUTEX(&umutex, "unlock_u2\n");
		return TRUE;
	} else {
		UNLOCK_MUTEX(&umutex, "unlock_u2\n");
		sprintf(tmp_buf,
		        "WARNING: The user at %s:%hu canceled a non-existing name '%s'\n",
			ip, port, name);
		Log(tmp_buf);
		return FALSE;
	}
}


// Clear the GUI names' list
void GUI_clear_names() {
	LOCK_MUTEX(&umutex, "lock_u3\n");
	gtk_list_store_clear(main_window->listUsers);
	UNLOCK_MUTEX(&umutex, "unlock_u3\n");
}


// Set column 3 (timer counter) with value "0" of GUI's name list
void GUI_reset_name_timer (GtkTreeIter *iter)
{
	assert(iter != NULL);
	gtk_list_store_set(main_window->listUsers, iter, 3, 0, -1);
}


// Increments the value in column 3 and returns TRUE if it is <3 for a name in GUI list
gboolean GUI_test_name_timer (GtkTreeIter *iter)
{
	assert(iter != NULL);

	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
	int cnt= -1;

	gtk_tree_model_get(list_store, iter, 3, &cnt, -1);
	if ((cnt==-1) || (cnt>1)) {
		return FALSE;
	}
	cnt++;
	gtk_list_store_set(GTK_LIST_STORE(main_window->listUsers), iter, 3, cnt, -1);
	return TRUE;
}

// Constructor of a UserInfo structure block
static UserInfo *GUI_new_UserInfo(char *name, char *ip, int port) {
	UserInfo *pt= (UserInfo *)malloc(sizeof(UserInfo));
	pt->name= name;
	pt->ip= ip;
	pt->port= port;
	return pt;
}

// Free all memory allocated in a user info object returned by GUI_test_all_name_timer
void GUI_free_UserInfo(UserInfo *pt) {
	if (pt->name != NULL)
		free (pt->name);
	if (pt->ip != NULL)
		free (pt->ip);
	free (pt);
}

// Free the memory allocated in a UserInfo list returned by GUI_test_all_name_timer
void GUI_free_UserInfo_list (GList *list) {
	while (list != NULL) {
		GUI_free_UserInfo((UserInfo *) list->data);
		list = list->next;
	}
	g_list_free(list);
}


// Test column 3 for all users and return a list with all that are overdue
GList *GUI_test_all_name_timer (void) {
	GtkTreeIter iter;
	gboolean valid;
	GList *list= NULL;
	gchar *ip;
	int port;
	gchar *name;

	LOCK_MUTEX(&umutex, "lock_u4\n");
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
	// Get the first iter in the list
	valid = gtk_tree_model_get_iter_first(list_store, &iter);
	while (valid && active) {
		// Walk through the list, reading each row
		if (!GUI_test_name_timer(&iter)) {
			gtk_tree_model_get(list_store, &iter, 0, &ip, 1, &port, 2, &name, -1);
			sprintf(tmp_buf,
					"User '%s' marked - name timeout\n",
					name);
			g_print("%s", tmp_buf);
			Log(tmp_buf);

			// Add to list
			list= g_list_append(list, GUI_new_UserInfo(name, ip, port));
		}
		valid = gtk_tree_model_iter_next(list_store, &iter);
	}
	UNLOCK_MUTEX(&umutex, "unlock_u4\n");
	return list;
}


// Get selected user data; returns FALSE if none is selected; returns TRUE and iter pointing to the line
gboolean GUI_get_selected_User(char **ip, int *port, char **name, GtkTreeIter *iter) {
	GtkTreeSelection *selection;
	GtkTreeModel	*model;
	assert(iter != NULL);
	assert(ip != NULL);
	assert(port!=NULL);
	assert(name!=NULL);

	LOCK_MUTEX(&umutex, "lock_u5\n");
	selection= gtk_tree_view_get_selection(main_window->treeUsers);
	if (gtk_tree_selection_get_selected(selection, &model, iter)) {
		gtk_tree_model_get (model, iter, 0, ip, 1, port, 2, name, -1);
		UNLOCK_MUTEX(&umutex, "unlock_u5\n");
		return TRUE;
	} else {
		UNLOCK_MUTEX(&umutex, "unlock_u5\n");
		return FALSE; // Not found
	}
}


/**************************************************************\
|* Functions to handle the GUI file transfer subprocess table *|
\**************************************************************/


// Search for a subprocess in the GUI file transfer subprocess list
gboolean GUI_locate_thread_by_tid(unsigned tid, GtkTreeIter *iter) {
	assert(iter != NULL);
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listFiles);
	gboolean valid;

	// Get the first iter in the list
	valid = gtk_tree_model_get_iter_first(list_store, iter);

	while (valid) {
		// Walk through the list, reading each row
		unsigned str_tid;

		// Make sure you terminate calls to gtk_tree_model_get() with a '-1' value
		gtk_tree_model_get(list_store, iter, 0, &str_tid, -1);

		if (tid == str_tid) {
			return TRUE;
		}
		valid = gtk_tree_model_iter_next(list_store, iter);
	}
	return FALSE;
}


// Regist a subprocess in GUI subprocess table
gboolean GUI_regist_thread(unsigned tid, const char *type, const char *name, const char *f_name)
{
	assert(type != NULL);
	assert(name != NULL);
	assert(f_name != NULL);

	GtkTreeIter iter;

	LOCK_MUTEX(&gmutex, "lock_g1\n");
	if (GUI_locate_thread_by_tid(tid, &iter)) {
		// PID already in the table
		UNLOCK_MUTEX(&gmutex, "unlock_g1\n");
		sprintf(tmp_buf, "WARNING: tid %u is already in Filetx table - ignored\n", tid);
		Log(tmp_buf);
		return FALSE;
	} else {
		if (GTK_IS_LIST_STORE(main_window->listFiles)) {
			// New registration
			gtk_list_store_append(main_window->listFiles, &iter);
			gtk_list_store_set(main_window->listFiles, &iter, 0, tid, 1, type, 2, strdup(name),
					3, 0, 4, strdup(f_name), -1);
			UNLOCK_MUTEX(&gmutex, "unlock_g1\n");
			return TRUE;
		} else {
			UNLOCK_MUTEX(&gmutex, "unlock_g1\n");
#ifdef DEBUG
			Log("Internal Error: Filetx' list store not active\n");
#endif
			return FALSE;
		}
	}
}


// Update the bytes sent in GUI subprocess table
gboolean GUI_update_bytes_sent(unsigned tid, int trans)
{
	GtkTreeIter iter;

	LOCK_MUTEX(&gmutex, "lock_g2\n");
	if (!GUI_locate_thread_by_tid(tid, &iter)) {
		UNLOCK_MUTEX(&gmutex, "unlock_g2\n");
		Log("Internal error: update of a non existing thread\n");
		return FALSE;
	}
	gtk_list_store_set(main_window->listFiles, &iter, 3, trans, -1);
	UNLOCK_MUTEX(&gmutex, "unlock_g2\n");
	return TRUE;
}


// Update the filename in GUI subprocess table (to complete information)
gboolean GUI_update_thread_info(unsigned tid, const char *name, const char *name_f)
{
	GtkTreeIter iter;

	LOCK_MUTEX(&gmutex, "unlock_g3\n");
	if (!GUI_locate_thread_by_tid(tid, &iter)) {
		UNLOCK_MUTEX(&gmutex, "unlock_g3\n");
		Log("Internal error: update of a non existing thread\n");
		return FALSE;
	}
	gtk_list_store_set(main_window->listFiles, &iter, 2, strdup(name), 4, strdup(name_f), -1);
	UNLOCK_MUTEX(&gmutex, "unlock_g3\n");
	return TRUE;
}


// Cancels a file transfer subprocess from GUI table
gboolean GUI_remove_thread(unsigned tid)
{
	LOCK_MUTEX(&gmutex, "lock_g4\n");
	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listFiles);

	GtkTreeIter iter;
	if (GUI_locate_thread_by_tid(tid, &iter)) {
		gtk_list_store_remove(GTK_LIST_STORE(list_store), &iter);
		UNLOCK_MUTEX(&gmutex, "unlock_g4\n");
		return TRUE;
	} else {
		UNLOCK_MUTEX(&gmutex, "unlock_g4\n");
		sprintf(tmp_buf,
		        "WARNING: Cancelling a non-existing thread '%u'\n", tid);
		Log(tmp_buf);
		return FALSE;
	}
}


// Clear the GUI threads' list
void GUI_clear_threads() {
	LOCK_MUTEX(&gmutex, "lock_g5\n");
	gtk_list_store_clear(main_window->listFiles);
	UNLOCK_MUTEX(&gmutex, "unlock_g5\n");
}


// Get selected thread data; returns FALSE if none is selected; returns TRUE and iter pointing to the line
gboolean GUI_get_selected_Filetx(unsigned *tid, GtkTreeIter *iter) {
	GtkTreeSelection *selection;
	GtkTreeModel	*model;
	assert(iter != NULL);
	assert(tid != NULL);

	LOCK_MUTEX(&gmutex, "lock_g6\n");
	selection= gtk_tree_view_get_selection(main_window->treeFiles);
	if (gtk_tree_selection_get_selected(selection, &model, iter)) {
		gtk_tree_model_get (model, iter, 0, tid, -1);
		UNLOCK_MUTEX(&gmutex, "unlock_g6\n");
		return TRUE;
	} else {
		UNLOCK_MUTEX(&gmutex, "unlock_g6\n");
		return FALSE; // Not found
	}
}


/*************************************************************\
|* Functions to support the selection of a file to transmit  *|
\*************************************************************/


// Create a window to select a file to open
char *open_select_filename_window() {
	GtkWidget *dialog;
	gint res;
	char *filename= NULL;

	dialog = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(main_window->window), GTK_FILE_CHOOSER_ACTION_OPEN,
			"_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT,
			NULL);

	res = gtk_dialog_run(GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
		filename = gtk_file_chooser_get_filename(chooser);
	}

	gtk_widget_destroy(dialog);
	return filename;
}


// Handler of filename button
void
on_buttonFilename_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
	char *filename= open_select_filename_window();
	if (filename != NULL) {
		gtk_entry_set_text(main_window->FileName, filename);
	}
}


/***************************\
|*   Auxiliary functions   *|
\***************************/

// Creates a window with an error message and outputs it to the command line
void
error_message (const gchar *message)
{
        GtkWidget               *dialog;

        /* log to terminal window */
        g_warning ("%s", message);

        /* create an error message dialog and display modally to the user */
        dialog = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         "%s", message);

        gtk_window_set_title (GTK_WINDOW (dialog), "Error!");
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}


/***********************\
|*   Event handlers    *|
\***********************/

// Handle 'Clear' button - clears textMemo
void on_buttonClear_clicked (GtkButton * button, gpointer user_data)
{
  GtkTextBuffer *textbuf;
  GtkTextIter tbegin, tend;

  pthread_mutex_lock( &lmutex );
  textbuf = GTK_TEXT_BUFFER (gtk_text_view_get_buffer (main_window->textView));
  gtk_text_buffer_get_iter_at_offset (textbuf, &tbegin, 0);
  gtk_text_buffer_get_iter_at_offset (textbuf, &tend, -1);
  gtk_text_buffer_delete (textbuf, &tbegin, &tend);
  pthread_mutex_unlock( &lmutex );
}





