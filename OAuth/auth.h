#ifndef AUTH_H
#define AUTH_H

#include <gtk/gtk.h>

void auth_initialize(void);
void auth_cleanup(void);
void initialize_goa();
void authenticate_user(GtkWidget *widget, gpointer data);


#endif // AUTH_H

