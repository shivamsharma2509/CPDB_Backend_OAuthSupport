#define GOA_API_IS_SUBJECT_TO_CHANGE
#include "auth.h"
#include <goa/goa.h>
#include <gtk/gtk.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define CLIENT_ID "Ov23lirB62bclUpviNM1"
#define CLIENT_SECRET "65ca463e0d048e12a0be7e8cc36073ba5c88bbe"
#define AUTHORIZATION_URL "https://github.com/login/oauth/authorize"
#define TOKEN_URL "https://github.com/login/oauth/access_token"

// Global variables
static gchar *auth_code = NULL;
static GoaClient *client;
GtkWidget *token_label;  // Define token_label here

// Function to handle HTTP requests
static enum MHD_Result handle_request(void *cls, struct MHD_Connection *connection,
                                       const char *url, const char *method,
                                       const char *version, const char *upload_data,
                                       size_t *upload_data_size, void **con_cls) {
    if (strcmp(url, "/callback") == 0) {
        if (strcmp(method, "GET") == 0) {
            const char *code = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code");

            if (code) {
                auth_code = g_strdup(code);
                g_print("Authorization code received: %s\n", auth_code);

                // Exchange authorization code for access token
                gchar *post_data = g_strdup_printf("client_id=%s&client_secret=%s&code=%s",
                                                    CLIENT_ID, CLIENT_SECRET, auth_code);

                // Use libcurl here to POST `post_data` to TOKEN_URL and handle the response

                g_free(post_data);
            }

            const char *response = "<html><body><h1>Authentication successful! You can close this window.</h1></body></html>";
            struct MHD_Response *response_obj = MHD_create_response_from_buffer(strlen(response), (void*)response, MHD_HTTP_OK);
            int ret = MHD_queue_response(connection, MHD_HTTP_OK, response_obj);
            MHD_destroy_response(response_obj);
            return ret;
        }
    }

    return MHD_NO;
}

// Function to start the local server
void start_local_server() {
    struct MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &handle_request, NULL, MHD_OPTION_END);
    if (daemon == NULL) {
        g_error("Failed to start local server.");
    }
    g_print("Local server started on port %d\n", PORT);
}

// Function to start the OAuth2 flow
void start_oauth_flow() {
    gchar *auth_url = g_strdup_printf(
        "%s?client_id=%s&redirect_uri=http://localhost:8080/callback&scope=user",
        AUTHORIZATION_URL, CLIENT_ID);

    g_print("Visit the following URL to authorize the application:\n%s\n", auth_url);
    g_free(auth_url);
}

// Callback when the GoaClient is ready
void on_goa_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    client = goa_client_new_finish(res, &error);

    if (error) {
        g_warning("Failed to initialize GoaClient: %s", error->message);
        g_error_free(error);
        return;
    }

    g_print("GoaClient initialized successfully.\n");

    // Start the OAuth2 flow after initializing the client
    start_oauth_flow();
}

// Initialize the GoaClient
void initialize_goa() {
    goa_client_new(NULL, on_goa_ready, NULL);
}

// Handle the access token received
void on_access_token_received(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GError *error = NULL;
    gchar *access_token = NULL;
    gint expires_in;

    gboolean success = goa_oauth2_based_call_get_access_token_finish(
        GOA_OAUTH2_BASED(source_object), &access_token, &expires_in, res, &error);

    if (!success) {
        g_warning("Failed to get access token: %s", error->message);
        g_error_free(error);
        gtk_label_set_text(GTK_LABEL(token_label), "Failed to get access token.");
        return;
    }

    if (access_token) {
        g_print("Access Token: %s\n", access_token);
        gtk_label_set_text(GTK_LABEL(token_label), access_token);
        g_free(access_token);
    } else {
        g_warning("Access token is NULL.");
        gtk_label_set_text(GTK_LABEL(token_label), "Access token is NULL.");
    }
}

// Handle account activation and get access token
void on_account_activated(GoaObject *object, gpointer user_data) {
    GoaOAuth2Based *oauth2_based = GOA_OAUTH2_BASED(goa_object_peek_account(object));
    goa_oauth2_based_call_get_access_token(oauth2_based, NULL, on_access_token_received, NULL);
}

// Authenticate the user by ensuring credentials
void authenticate_user(GtkWidget *widget, gpointer data) {
    GList *accounts = goa_client_get_accounts(client);

    for (GList *l = accounts; l != NULL; l = l->next) {
        GoaObject *object = GOA_OBJECT(l->data);
        GoaAccount *account = goa_object_peek_account(object);

        const gchar *provider_type = goa_account_get_provider_type(account);
        if (g_strcmp0(provider_type, "oauth2") == 0) {
            g_signal_connect(object, "notify::access-token", G_CALLBACK(on_account_activated), NULL);
            goa_account_call_ensure_credentials_sync(account, NULL, NULL, NULL);
            break;
        }
    }

    g_list_free_full(accounts, g_object_unref);
}

// Main function
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "OAuth2 Authentication");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    token_label = gtk_label_new("Token will appear here...");
    gtk_box_pack_start(GTK_BOX(vbox), token_label, TRUE, TRUE, 5);

    GtkWidget *auth_button = gtk_button_new_with_label("Authenticate");
    gtk_box_pack_start(GTK_BOX(vbox), auth_button, TRUE, TRUE, 5);
    g_signal_connect(auth_button, "clicked", G_CALLBACK(authenticate_user), NULL);

    gtk_widget_show_all(window);

    initialize_goa();
    start_local_server();

    gtk_main();

    return 0;
}



