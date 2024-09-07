#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <cups/cups.h>

#include "cups-notifier.h"

#include <cpdb/backend.h>
#include "backend_helper.h"
#include "auth.h"  // Include the authentication header

#define _CUPS_NO_DEPRECATED 1
#define BUS_NAME "org.openprinting.Backend.CUPS"

// Function declarations
static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer not_used);
static void acquire_session_bus_name(char *bus_name);
gpointer list_printers(gpointer _dialog_name);
int send_printer_added(void *_dialog_name, unsigned flags, cups_dest_t *dest);
void connect_to_signals();
void init_authentication();  // Add function for initializing authentication

BackendObj *b;

void update_printer_lists()
{
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, b->dialogs);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        char *dialog_name = key;
        refresh_printer_list(b, dialog_name);
    }
}

static void on_printer_state_changed (CupsNotifier *object, const gchar *text, const gchar *printer_uri,
                                       const gchar *printer, guint printer_state, const gchar *printer_state_reasons,
                                       gboolean printer_is_accepting_jobs, gpointer user_data)
{
    loginfo("Printer state change on printer %s: %s\n", printer, text);

    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, b->dialogs);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        const char *dialog_name = key;
        PrinterCUPS *p = get_printer_by_name(b, dialog_name, printer);
        const char *state = get_printer_state(p);
        send_printer_state_changed_signal(b, dialog_name, printer, state, printer_is_accepting_jobs);
    }
}

static void on_printer_added (CupsNotifier *object, const gchar *text, const gchar *printer_uri, const gchar *printer,
                              guint printer_state, const gchar *printer_state_reasons, gboolean printer_is_accepting_jobs,
                              gpointer user_data)
{
    loginfo("Printer added: %s\n", text);
    update_printer_lists();
}

static void on_printer_deleted (CupsNotifier *object, const gchar *text, const gchar *printer_uri, const gchar *printer,
                                guint printer_state, const gchar *printer_state_reasons, gboolean printer_is_accepting_jobs,
                                gpointer user_data)
{
    loginfo("Printer deleted: %s\n", text);
    update_printer_lists();
}

int main()
{
    /* Initialize internal default settings of the CUPS library */
    int p = ippPort();

    b = get_new_BackendObj();
    cpdbInit();
    acquire_session_bus_name(BUS_NAME);

    init_authentication();  // Initialize authentication

    int subscription_id = create_subscription();

    g_timeout_add_seconds (NOTIFY_LEASE_DURATION - 60, renew_subscription_timeout, &subscription_id);

    GError *error = NULL;
    CupsNotifier *cups_notifier = cups_notifier_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, 0, NULL,
                                                                       CUPS_DBUS_PATH, NULL, &error);
    if (error)
    {
        logwarn("Error creating cups notify handler: %s", error->message);
        g_error_free(error);
        cups_notifier = NULL;
    }

    if (cups_notifier != NULL)
    {
        g_signal_connect(cups_notifier, "printer-state-changed", G_CALLBACK(on_printer_deleted), NULL);
        g_signal_connect(cups_notifier, "printer-deleted", G_CALLBACK(on_printer_deleted), NULL);
        g_signal_connect(cups_notifier, "printer-added", G_CALLBACK(on_printer_added), NULL);
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    /* Main loop exited */
    logdebug("Main loop exited");
    g_main_loop_unref(loop);
    loop = NULL;

    cancel_subscription(subscription_id);
    if (cups_notifier)
        g_object_unref(cups_notifier);
}

static void acquire_session_bus_name(char *bus_name)
{
    g_bus_own_name(G_BUS_TYPE_SESSION, bus_name, 0, NULL, on_name_acquired, NULL, NULL, NULL);
}

static void on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer not_used)
{
    b->dbus_connection = connection;
    b->skeleton = print_backend_skeleton_new();
    connect_to_signals();
    connect_to_dbus(b, CPDB_BACKEND_OBJ_PATH);
}

static gboolean on_handle_get_printer_list(PrintBackend *interface, GDBusMethodInvocation *invocation, gpointer user_data)
{
    int num_printers;
    GHashTableIter iter;
    gpointer key, value;
    GVariantBuilder builder;
    GVariant *printer, *printers;

    cups_dest_t *dest;
    gboolean accepting_jobs;
    const char *state;
    char *name, *info, *location, *make;

    GHashTable *table = cups_get_all_printers();
    const char *dialog_name = g_dbus_method_invocation_get_sender(invocation);

    add_frontend(b, dialog_name);
    num_printers = g_hash_table_size(table);
    if (num_printers == 0)
    {
        printers = g_variant_new_array(G_VARIANT_TYPE ("(v)"), NULL, 0);
        print_backend_complete_get_printer_list(interface, invocation, 0, printers);
        return TRUE;
    }

    g_hash_table_iter_init(&iter, table);
    g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        name = key;
        dest = value;
        loginfo("Found printer : %s\n", name);
        info = cups_retrieve_string(dest, "printer-info");
        location = cups_retrieve_string(dest, "printer-location");
        make = cups_retrieve_string(dest, "printer-make-and-model");
        accepting_jobs = cups_is_accepting_jobs(dest);
        state = cups_printer_state(dest);
        add_printer_to_dialog(b, dialog_name, dest);
        printer = g_variant_new(CPDB_PRINTER_ARGS, dest->name, dest->name, info, location, make, accepting_jobs, state, BACKEND_NAME);
        g_variant_builder_add(&builder, "(v)", printer);
        free(key);
        cupsFreeDests(1, value);
        free(info);
        free(location);
        free(make);
    }
    g_hash_table_destroy(table);
    printers = g_variant_builder_end(&builder);

    print_backend_complete_get_printer_list(interface, invocation, num_printers, printers);
    return TRUE;
}

static gboolean on_handle_get_all_translations(PrintBackend *interface, GDBusMethodInvocation *invocation, const gchar *printer_name, const gchar *locale, gpointer user_data)
{
    PrinterCUPS *p;
    GVariant *translations;
    const char *dialog_name;

    dialog_name = g_dbus_method_invocation_get_sender(invocation);
    p = get_printer_by_name(b, dialog_name, printer_name);
    translations = get_printer_translations(p, locale);
    print_backend_complete_get_all_translations(interface, invocation, translations);

    return TRUE;
}

// Define authentication initialization function
void init_authentication()
{
    // Initialize your authentication library here
    // For example, you could call a function to set up OAuth or other authentication methods
    auth_initialize();
}

void cleanup_authentication(void) {
    auth_cleanup();
    // Additional cleanup code
}

// Rest of your existing function implementations...

void connect_to_signals()
{
    PrintBackend *skeleton = b->skeleton;
    g_signal_connect(skeleton, "handle-get-printer-list", G_CALLBACK(on_handle_get_printer_list), NULL);
    g_signal_connect(skeleton, "handle-get-all-options", G_CALLBACK(on_handle_get_all_options), NULL);
    g_signal_connect(skeleton, "handle-ping", G_CALLBACK(on_handle_ping), NULL);
    g_signal_connect(skeleton, "handle-get-default-printer", G_CALLBACK(on_handle_get_default_printer), NULL);
    g_signal_connect(skeleton, "handle-print-socket", G_CALLBACK(on_handle_print_socket), NULL);
    g_signal_connect(skeleton, "handle-get-printer-state", G_CALLBACK(on_handle_get_printer_state), NULL);
    g_signal_connect(skeleton, "handle-is-accepting-jobs", G_CALLBACK(on_handle_is_accepting_jobs), NULL);
    g_signal_connect(skeleton, "handle-keep-alive", G_CALLBACK(on_handle_keep_alive), NULL);
    g_signal_connect(skeleton, "handle-replace", G_CALLBACK(on_handle_replace), NULL);
    g_signal_connect(skeleton, "handle-get-option-translation", G_CALLBACK(on_handle_get_option_translation), NULL);
    g_signal_connect(skeleton, "handle-get-all-translations", G_CALLBACK(on_handle_get_all_translations), NULL);
}


