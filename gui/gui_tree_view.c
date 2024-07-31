#include <gtk/gtk.h>
#include "gui.h"
#include "gui_menu.h"

static void add_info_tree_menu_item(GtkWidget *menu, const gchar *label, void callback(GtkWidget *widget, gpointer data)) {
    GtkWidget *menu_item = gtk_menu_item_new_with_label(label);
    g_signal_connect(menu_item, "activate", G_CALLBACK(callback), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
}

static void on_info_tree_view_right_click(GtkWidget *widget, GdkEvent *event, gpointer data) {
    if (event->type != GDK_BUTTON_PRESS || ((GdkEventButton *)event)->button != 3) {
        return;
    }
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    GtkTreePath *path;
    if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), ((GdkEventButton *)event)->x, ((GdkEventButton *)event)->y, &path, NULL, NULL, NULL)) {
        return;
    }
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);

    GtkWidget *menu = gtk_menu_new();

    add_info_tree_menu_item(menu, "Connect", on_connect_menu_activate);
    add_info_tree_menu_item(menu, "Disconnect", on_disconnect_menu_activate);
    add_info_tree_menu_item(menu, "Update", on_update_menu_activate);
    add_info_tree_menu_item(menu, "Remove", on_remove_menu_activate);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), event);    
}

void add_tree_view_column(GtkWidget *tree_view, const gchar *text, const int index) {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(text, renderer, "text", index, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
}

GtkWidget *create_tree_view(void) {
    GtkWidget *tree_view = gtk_tree_view_new();
    add_tree_view_column(tree_view, "Description", 0);  
    add_tree_view_column(tree_view, "Host", 1);  
    add_tree_view_column(tree_view, "Port", 2);
    add_tree_view_column(tree_view, "Protocol", 3);    
    add_tree_view_column(tree_view, "Username", 4);
    add_tree_view_column(tree_view, "Local Directory", 5);
    add_tree_view_column(tree_view, "Remote Directory", 6);  
    add_tree_view_column(tree_view, "Status", 7);
    g_signal_connect(tree_view, "button-press-event", G_CALLBACK(on_info_tree_view_right_click), NULL);
    return tree_view;
}