#ifndef GUI_EVENT_HANDLERS_H
#define GUI_EVENT_HANDLERS_H

#include <gtk/gtk.h>

extern void on_remove_menu_activate(GtkWidget *widget, gpointer data);

extern void on_update_menu_activate(GtkWidget *widget, gpointer data);

extern void on_add_menu_activate(GtkWidget *widget, gpointer data);

extern void on_connect_menu_activate(GtkWidget *widget, gpointer data);

extern void on_disconnect_menu_activate(GtkWidget *widget, gpointer data);

extern void on_add_dialog_ok_button_clicked(GtkWidget *widget, gpointer data);

extern void on_update_dialog_ok_button_clicked(GtkWidget *widget, gpointer data);

extern void on_add_dialog_cancel_button_clicked(GtkWidget *widget, gpointer data);

extern void on_update_dialog_cancel_button_clicked(GtkWidget *widget, gpointer data);

extern void on_local_browse_button_clicked(GtkWidget *widget, gpointer data);

extern void on_remote_browse_button_clicked(GtkWidget *widget, gpointer data);

extern void on_info_tree_view_right_click(GtkWidget *widget, GdkEvent *event, gpointer data);

extern void on_directory_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);

#endif