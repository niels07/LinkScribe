#include <gtk/gtk.h>
#include "ftp.h"
#include "gui.h"
#include "gui_dialogs.h"
#include "gui_ftp.h"
#include "storage.h"
#include "utils.h"

typedef struct {
    GtkTextBuffer *buffer;
    gchar *text;
} InsertTextData;

void on_update_menu_activate(GtkWidget *widget, gpointer data) {
    create_update_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)), data);
}

static void on_add_menu_activate(GtkWidget *widget, gpointer data) {
    create_add_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
}

void on_remove_menu_activate(GtkWidget *widget, gpointer data) {
    GtkWidget *main_window = gui_get_main_window();
    GtkWidget *tree_view = gui_get_tree_view();
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Are you sure you want to delete this connection?");

    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_YES) {
        char *description;
        gtk_tree_model_get(model, &iter, 0, &description, -1);
        remove_ftp_details(description);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        g_free(description);
    }
    gtk_widget_destroy(dialog);    
}

void on_connect_menu_activate(GtkWidget *widget, gpointer data) {
    GtkWidget *tree_view = gui_get_tree_view();
    GtkWidget *status_area = gui_get_status_area();
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }

    gtk_tree_model_get(model, &iter, -1);
    log_message("Connecting...", status_area);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 7, "Connecting", -1);
    FTPDetails *details = gui_get_ftp_details();
    ftp_connect(details, on_ftp_connect, model);        
}

static gboolean insert_text_idle(gpointer data) {
    InsertTextData *insert_data = (InsertTextData *)data;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(insert_data->buffer, &end);
    gtk_text_buffer_insert(insert_data->buffer, &end, insert_data->text, -1);
    g_free(insert_data->text);
    g_free(insert_data);
    return FALSE;
}

void on_ftp_connect(void *arg, const char *message, bool success) {
    GtkWidget *status_area = gui_get_status_area();
    InsertTextData *insert_data = g_malloc(sizeof(InsertTextData));
    insert_data->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_area));
    insert_data->text = g_strdup(message);
    g_idle_add(insert_text_idle, insert_data);

    GtkTreeModel *model = GTK_TREE_MODEL(arg);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(model, &iter)) {
        return;
    }
    do {
        gchar *host;
        gtk_tree_model_get(model, &iter, 4, &host, -1);
        if (g_strcmp0(host, message) == 0) {
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, 7, success ? "Connected" : "Connection Failed", -1);
            g_free(host);
            break;
        }
        g_free(host);
    } while (gtk_tree_model_iter_next(model, &iter));    
}

void on_disconnect_menu_activate(GtkWidget *widget, gpointer data) {
    GtkWidget *tree_view = gui_get_tree_view();
    GtkWidget *status_area = gui_get_status_area();
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        log_message("Disconnecting...", status_area);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 7, "Not Connected", -1);
    }
}

static void add_file_menu_item(GtkWidget *menubar) {
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_menu_item = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);

    GtkWidget *add_menu_item = gtk_menu_item_new_with_label("Add");
    g_signal_connect(add_menu_item, "activate", G_CALLBACK(on_add_menu_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), add_menu_item);

    GtkWidget *update_menu_item = gtk_menu_item_new_with_label("Update");
    g_signal_connect(update_menu_item, "activate", G_CALLBACK(on_update_menu_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), update_menu_item);

    GtkWidget *remove_menu_item = gtk_menu_item_new_with_label("Remove");
    g_signal_connect(remove_menu_item, "activate", G_CALLBACK(on_remove_menu_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), remove_menu_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);
}

static void add_connection_menu_item(GtkWidget *menubar) {
    GtkWidget *connection_menu = gtk_menu_new();
    GtkWidget *connection_menu_item = gtk_menu_item_new_with_label("Connection");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(connection_menu_item), connection_menu);

    GtkWidget *connect_menu_item = gtk_menu_item_new_with_label("Connect");
    g_signal_connect(connect_menu_item, "activate", G_CALLBACK(on_connect_menu_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(connection_menu), connect_menu_item);

    GtkWidget *disconnect_menu_item = gtk_menu_item_new_with_label("Disconnect");
    g_signal_connect(disconnect_menu_item, "activate", G_CALLBACK(on_disconnect_menu_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(connection_menu), disconnect_menu_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), connection_menu_item);
}

static void add_help_menu_item(GtkWidget *menubar) {
    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_menu_item = gtk_menu_item_new_with_label("Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);

    GtkWidget *check_updates_menu_item = gtk_menu_item_new_with_label("Check for updates");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), check_updates_menu_item);

    GtkWidget *get_help_menu_item = gtk_menu_item_new_with_label("Get Help");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), get_help_menu_item);

    GtkWidget *about_menu_item = gtk_menu_item_new_with_label("About");
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_menu_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_menu_item);
}

GtkWidget *create_menu_bar(void) {
    GtkWidget *menubar = gtk_menu_bar_new();  
    add_file_menu_item(menubar);
    add_connection_menu_item(menubar);
    add_help_menu_item(menubar);
    return menubar;
}