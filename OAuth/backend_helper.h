#ifndef _BACKEND_HELPER_H
#define _BACKEND_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cupsfilters/catalog.h>

#include <cpdb/backend.h>

/* For cups-notifier */
#define NOTIFY_LEASE_DURATION (24 * 60 * 60)
#define CUPS_DBUS_PATH "/org/cups/cupsd/Notifier"

/* New Debug macros */
#define BACKEND_NAME "CUPS"
#define logdebug(...) cpdbBDebugPrintf(CPDB_DEBUG_LEVEL_DEBUG, BACKEND_NAME, __VA_ARGS__)
#define loginfo(...)  cpdbBDebugPrintf(CPDB_DEBUG_LEVEL_INFO, BACKEND_NAME, __VA_ARGS__)
#define logwarn(...)  cpdbBDebugPrintf(CPDB_DEBUG_LEVEL_WARN, BACKEND_NAME, __VA_ARGS__)
#define logerror(...) cpdbBDebugPrintf(CPDB_DEBUG_LEVEL_ERROR, BACKEND_NAME, __VA_ARGS__)

/* Old debug macros */
#define INFO 3
#define WARN 2
#define ERR 1

#define MSG_LOG_LEVEL INFO

/**
 * Represents a CUPS Printer
 */
typedef struct _PrinterCUPS
{
    char *name;
    cups_dest_t *dest;
    http_t *http;
    cups_dinfo_t *dinfo;
    char *stream_socket_path;
} PrinterCUPS;

/**
 * Represents a frontend instance that the backend is associated with
 */
typedef struct _Dialog
{
    int cancel;
    gboolean hide_remote;
    gboolean hide_temp;
    GHashTable *printers;
    gboolean keep_alive;
} Dialog;

typedef struct _Mappings
{
    GHashTable *media;
    GHashTable *color;
    GHashTable *print_quality;
    const char *orientation[10];
    const char *state[6];
} Mappings;

/**
 * Represents the CUPS Backend
 */
typedef struct _BackendObj
{
    GDBusConnection *dbus_connection;
    PrintBackend *skeleton;
    char *obj_path;

    /** the hash table to map from dialog name(char*) to the Dialog struct(Dialog*) **/
    GHashTable *dialogs;

    int num_frontends;
    char *default_printer;
} BackendObj;

/**
 * Represents a single 'option' for a printer
 */
typedef struct _Option
{
    char *option_name;
    int num_supported;
    char **supported_values;
    char *default_value;
} Option;

/**
 * Represents a single 'media' size for a printer and supported margins
 */
typedef struct _Media
{
	char *name;
	int width;
	int length;
	int num_margins;
	int (*margins)[4]; /** int margins[num_margins][4]; left(0), right(1), top(2), bottom(3) **/
} Media;

/*
typedef struct _PrintResult
{
    gchar *jobid;
    gchar *socket;
} PrintResult;
*/

typedef struct _PrintDataThreadData {
    PrinterCUPS *printer;
    int num_options;
    cups_option_t *options;
    int socket_fd;
    struct sockaddr_un server_addr;
} PrintDataThreadData;

/********Backend related functions*******************/

/** Get a new BackendObj **/
BackendObj *get_new_BackendObj();

/** Get the printer-id of the default printer of the CUPS Backend**/
char *get_default_printer(BackendObj *b);

/** Connect the BackendObj to the dbus **/
void connect_to_dbus(BackendObj *, char *obj_path);

/** Add the dialog to the list of dialogs of the particular backend**/
void add_frontend(BackendObj *, const char *dialog_name);

/** Remove the dialog from the list of frontends that this backend is 
 * associated with. 
 */
void remove_frontend(BackendObj *, const char *dialog_name);

/** Checks if the backend isn't associated with any frontend **/
gboolean no_frontends(BackendObj *);

/**
 * Find the dialog with the specified name
 */
Dialog* find_dialog(BackendObj * , const char* dialog_name);

/** Get the variable which controls the cancellation of the enumeration thread for
 * that particular dialog 
 */
int *get_dialog_cancel(BackendObj *, const char *dialog_name);

void set_dialog_cancel(BackendObj *, const char *dialog_name);   //make cancel = 0
void reset_dialog_cancel(BackendObj *, const char *dialog_name); //make cancel = 1

/** Returns whether remote CUPS printers are hidden for this dialog **/
gboolean get_hide_remote(BackendObj *b, char *dialog_name);

/** Hides remote CUPS printers for the dialog **/
void set_hide_remote_printers(BackendObj *, const char *dialog_name);

/** Unhides remote CUPS Printers for the dialog **/
void unset_hide_remote_printers(BackendObj *, const char *dialog_name);

/** Returns whether temporary CUPS Queues(discovered, but not set up) are hidden for this dialog **/
gboolean get_hide_temp(BackendObj *b, char *dialog_name);

/** Hides temporary CUPS queues for the dialog **/
void set_hide_temp_printers(BackendObj *, const char *dialog_name);

/** Unhides temporary CUPS queues for the dialog **/
void unset_hide_temp_printers(BackendObj *, const char *dialog_name);

/** Utility functions for subscribing to CUPS for notifications*/
int create_subscription ();
gboolean renew_subscription (int id);
gboolean renew_subscription_timeout(gpointer userdata);
void cancel_subscription (int id);

/**
 * Returns
 * TRUE if the printer with specified name is found for the dialog
 * FALSE otherwise
 */
gboolean dialog_contains_printer(BackendObj *, const char *dialog_name, const char *printer_name);

/**
 * Adds the corresponding CUPS printer to the dialog's printer list
 * 
 * Returns 
 * the PrinterCUPS* struct added to the dialog
 * NULL if the operation was unsuccesful
 */
PrinterCUPS *add_printer_to_dialog(BackendObj *, const char *dialog_name, const cups_dest_t *dest);

/**
 * Removes the printer with the specified name from the dialog's list of printers
 */
void remove_printer_from_dialog(BackendObj *, const char *dialog_name, const char *printer_name);

void send_printer_state_changed_signal(BackendObj *b, const char *dialog_name, const char *printer_name,
                                        const char *printer_state, gboolean printer_is_accepting_jobs);
void send_printer_added_signal(BackendObj *b, const char *dialog_name, cups_dest_t *dest);
void send_printer_removed_signal(BackendObj *b, const char *dialog_name, const char *printer_name);
void notify_removed_printers(BackendObj *b, const char *dialog_name, GHashTable *new_table);
void notify_added_printers(BackendObj *b, const char *dialog_name, GHashTable *new_table);
void replace_printers(BackendObj *b, const char *dialog_name, GHashTable *new_table);
void refresh_printer_list(BackendObj *b, char *dialog_name);
GHashTable *get_dialog_printers(BackendObj *b, const char *dialog_name);
cups_dest_t *get_dest_by_name(BackendObj *b, const char *dialog_name, const char *printer_name);
PrinterCUPS *get_printer_by_name(BackendObj *b, const char *dialog_name, const char *printer_name);

/*********Printer related functions******************/

/** Get a new PrinterCUPS struct associated with the cups destination**/
PrinterCUPS *get_new_PrinterCUPS(const cups_dest_t *dest);

/** Free up the memory used by the struct **/
void free_PrinterCUPS(PrinterCUPS *);

/** Ensure that we have a connection the server**/
gboolean ensure_printer_connection(PrinterCUPS *p);

/**
 * Get state of the printer
 * state is one of the following {"idle" , "processing" , "stopped"}
 */
const char *get_printer_state(PrinterCUPS *p);
char *get_orientation_default(PrinterCUPS *p);
char *get_default(PrinterCUPS *p, char *option_name);
int get_supported(PrinterCUPS *p, char ***supported_values, const char *option_name);
int get_job_creation_attributes(PrinterCUPS *p, char ***values);

int get_all_options(PrinterCUPS *p, Option **options);
int get_all_media(PrinterCUPS *p, Media **medias);
int add_media_to_options(PrinterCUPS *p, Media *medias, int media_count, Option **options, int count);

static void *print_data_thread(void *data);
void print_socket(PrinterCUPS *p, int num_settings, GVariant *settings, char *job_id_str, char *socket_path, const char *title);


/**
 * Get translation of choice name for a given locale
 */
char *get_option_translation(PrinterCUPS *p, const char *option_name,
                             const char *locale);

/**
 * Get translation of option name for a given locale
 */
char *get_choice_translation(PrinterCUPS *p, const char *option_name,
                             const char *choice_name, const char *locale);

/**
 * Get translations for all printer strings
 */
GVariant *get_printer_translations(PrinterCUPS *p, const char *locale);


void tryPPD(PrinterCUPS *p);
/**********Dialog related funtions ****************/
Dialog *get_new_Dialog();
void free_Dialog(Dialog *);

/*********Option related functions*****************/
void print_option(const Option *opt);
void free_options(int count, Option *opts);
void unpack_option_array(GVariant *var, int num_options, Option **options);
GVariant *pack_option(const Option *opt);
GVariant *pack_media(const Media *media);
/**********Mapping related functions*****************/
Mappings *get_new_Mappings();

/*************CUPS/IPP RELATED FUNCTIONS******************/
const char *cups_printer_state(cups_dest_t *dest);
gboolean cups_is_accepting_jobs(cups_dest_t *dest);
void cups_get_Resolution(cups_dest_t *dest, int *xres, int *yres);
GHashTable *cups_get_all_printers();
GHashTable *cups_get_local_printers();
char *cups_retrieve_string(cups_dest_t *dest, const char *option_name);
gboolean cups_is_temporary(cups_dest_t *dest);
GHashTable *cups_get_printers(gboolean notemp, gboolean noremote);
char *extract_ipp_attribute(ipp_attribute_t *, int index, const char *option_name);
char *extract_res_from_ipp(ipp_attribute_t *, int index);
char *extract_string_from_ipp(ipp_attribute_t *attr, int index);
char *extract_orientation_from_ipp(ipp_attribute_t *attr, int index);
void print_job(cups_job_t *j);
GVariant *pack_cups_job(cups_job_t job);
char *translate_job_state(ipp_jstate_t);

/***************Misc.** **********************/

/**error logging */
void MSG_LOG(const char *msg, int msg_level);
void free_string(char *);
void free_string_array(int count, char **arr);
#endif
