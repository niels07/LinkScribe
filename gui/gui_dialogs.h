#ifndef GUI_DIALOGS_H
#define GUI_DIALOGS_H

#include <gtk/gtk.h>
#include "ftp.h"

extern void create_update_dialog(GtkWindow *parent, gpointer data);
extern void create_add_dialog(GtkWindow *parent);

#endif