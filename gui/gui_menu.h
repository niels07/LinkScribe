#ifndef GUI_MENU_H
#define GUI_MENU_H

#include <gtk/gtk.h>
#include <stdbool.h>

extern void on_connect_menu_activate(GtkWidget *widget, gpointer data);
extern void on_disconnect_menu_activate(GtkWidget *widget, gpointer data);
extern void on_update_menu_activate(GtkWidget *widget, gpointer data);
extern void on_remove_menu_activate(GtkWidget *widget, gpointer data);
extern GtkWidget *create_menu_bar(void);

#endif