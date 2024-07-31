#include <gtk/gtk.h>
#include "gui.h"
#include "gui_ftp.h"
#include "gui_tree_view.h"
#include "ftp.h"
#include "storage.h"
#include "utils.h"

#define REQUIRE_FIELD(dialog, field, message)               \
    do {                                                    \
        if (g_strcmp0(field, "") == 0) {                    \
            show_error_dialog(GTK_DIALOG(dialog), message); \
            return;                                         \
        }                                                   \
    } while (0)

static void show_error_dialog(GtkDialog *parent, const gchar *message) {
    GtkWidget *error_dialog = gtk_message_dialog_new(
        GTK_WINDOW(parent), 
        GTK_DIALOG_DESTROY_WITH_PARENT, 
        GTK_MESSAGE_ERROR, 
        GTK_BUTTONS_CLOSE, 
        "%s", message);
    gtk_dialog_run(GTK_DIALOG(error_dialog));
    gtk_widget_destroy(error_dialog);
}

static GtkWidget *create_dialog(GtkWindow *parent, const gchar *title) {
    GtkWidget *dialog = gtk_dialog_new();
    GdkPixbuf *window_icon = gui_get_window_icon();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_icon(GTK_WINDOW(dialog), window_icon);
    return dialog;
}

static GtkWidget *create_description_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Description:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 0, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "description_entry", entry);
    return entry;
}

static GtkWidget *create_host_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Host:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 1, 2, 1);   
    g_object_set_data(G_OBJECT(dialog), "host_entry", entry);
    return entry;
}

static GtkWidget *create_port_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Port:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "21"); 
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 2, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "port_entry", entry);
    return entry;
}

static GtkWidget *create_protocol_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Protocol:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);

    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FTP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "SFTP");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_grid_attach(GTK_GRID(grid), combo, 1, 3, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "protocol_combo", combo);
    return combo;
}

static GtkWidget *create_username_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Username:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 4, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "username_entry", entry);
    return entry;
}

static GtkWidget *create_password_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Password:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 5, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "password_entry", entry);
    return entry;
}

static void on_local_browse_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *entry = GTK_WIDGET(data);
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Directory", 
        GTK_WINDOW(gtk_widget_get_toplevel(widget)), 
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, 
        "Cancel", 
        GTK_RESPONSE_CANCEL, 
        "Select", 
        GTK_RESPONSE_ACCEPT, 
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(entry), folder);
        g_free(folder);
    }

    gtk_widget_destroy(dialog);
}

static void on_up_button_clicked(GtkWidget *widget, gpointer data) {
    FTPDetails *details = (FTPDetails *)data;
    
    char *last_slash = strrchr(details->remote_path, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        strncpy(details->remote_path, "/", sizeof(details->remote_path) - 1);
    }

    if (!g_utf8_validate(details->remote_path, -1, NULL)) {
        PRINT_ERROR("Invalid UTF-8 string for remote path: %s", details->remote_path);
        return;
    }

    GtkWidget *window = gtk_widget_get_toplevel(widget);
    GList *children = gtk_container_get_children(GTK_CONTAINER(window));
    for (GList *child = children; child != NULL; child = child->next) {
        if (GTK_IS_LABEL(child->data)) {
            gtk_label_set_text(GTK_LABEL(child->data), details->remote_path);
        }
    }
    g_list_free(children);

    GtkWidget *tree_view = g_object_get_data(G_OBJECT(window), "tree_view");
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)));
    gtk_list_store_clear(store);

    ftp_list_dirs(details, gui_on_ftp_list_dirs, tree_view);
}

static void on_directory_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    FTPDetails *details = (FTPDetails *)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(model, &iter, path)) {
        return;
    }
    gchar *directory;
    gtk_tree_model_get(model, &iter, 0, &directory, -1);

    if (!directory || !g_utf8_validate(directory, -1, NULL)) {
        return;
    }

    gchar updated_path[1024] = {0};
    
    if (g_str_has_suffix(details->remote_path, "/")) {
        snprintf(updated_path, sizeof(updated_path), "%s%s/", details->remote_path, directory);
    } else {
        snprintf(updated_path, sizeof(updated_path), "%s/%s/", details->remote_path, directory);
    }
    
    strncpy(details->remote_path, updated_path, sizeof(details->remote_path) - 1);

    GtkWidget *dialog = gtk_widget_get_toplevel(GTK_WIDGET(tree_view));
    GtkWidget *path_label = g_object_get_data(G_OBJECT(dialog), "path_label");
    gtk_label_set_text(GTK_LABEL(path_label), details->remote_path);

    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(tree_view));
    gtk_list_store_clear(store);

    ftp_list_dirs(details, gui_on_ftp_list_dirs, tree_view);
    g_free(directory);
}

static GtkWidget *create_dir_list_tree_view() {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *tree_view = gtk_tree_view_new();

    add_tree_view_column(tree_view, "Filename", 0);
    add_tree_view_column(tree_view, "Permissions", 1);
    add_tree_view_column(tree_view, "Owner", 2);
    add_tree_view_column(tree_view, "Group", 3);
    add_tree_view_column(tree_view, "Updated", 4);

    GtkListStore *store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));
    g_object_unref(store);
    return tree_view;
}

static void on_remote_browse_ok_button_clicked(GtkWidget *widget, gpointer data) {
    FTPDetails *details = (FTPDetails *)data;
    GtkWindow *current_dialog = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    GtkWindow *parent_dialog = gtk_window_get_transient_for(current_dialog);
    GtkWidget *entry = g_object_get_data(G_OBJECT(parent_dialog), "remote_path_entry");

    gtk_entry_set_text(GTK_ENTRY(entry), details->remote_path);
    g_free(details);
    gtk_widget_destroy(GTK_WIDGET(current_dialog));
}

static void on_remote_browse_cancel_button_clicked(GtkWidget *widget, gpointer data) {
    FTPDetails *details = (FTPDetails *)data;
    GtkWidget *dialog = gtk_widget_get_toplevel(widget);
    g_free(details);
    gtk_widget_destroy(widget);
}

static void create_remote_browse_dialog(GtkWindow *parent, FTPDetails *details) {
    if (!g_utf8_validate(details->remote_path, -1, NULL)) {
        PRINT_ERROR("Invalid UTF-8 string for initial remote path: %s", details->remote_path);
        return;
    }

    GtkWidget *dialog = create_dialog(parent, "Remote Directory Listing");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    GtkWidget *path_label = gtk_label_new(details->remote_path);
    gtk_box_pack_start(GTK_BOX(vbox), path_label, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(dialog), "path_label", path_label);

    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

    GtkWidget *up_button = gtk_button_new_with_label("Up");
    gtk_box_pack_start(GTK_BOX(nav_box), up_button, FALSE, FALSE, 0);
    g_signal_connect(up_button, "clicked", G_CALLBACK(on_up_button_clicked), details);

    GtkWidget *down_button = gtk_button_new_with_label("Down");
    gtk_box_pack_start(GTK_BOX(nav_box), down_button, FALSE, FALSE, 0);
    g_signal_connect(down_button, "clicked", G_CALLBACK(on_directory_selected), details);

    gtk_box_pack_start(GTK_BOX(vbox), nav_box, FALSE, FALSE, 0);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 400);  // Set the height to 400 pixels
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    GtkWidget *tree_view = create_dir_list_tree_view();
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_directory_selected), details);

    g_object_set_data(G_OBJECT(dialog), "tree_view", tree_view);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    gtk_box_pack_end(GTK_BOX(hbox), ok_button, FALSE, FALSE, 0);
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_remote_browse_ok_button_clicked), details);

    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    gtk_box_pack_end(GTK_BOX(hbox), cancel_button, FALSE, FALSE, 0);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_remote_browse_cancel_button_clicked), details);

    gtk_widget_show_all(dialog);

    ftp_list_dirs(details, gui_on_ftp_list_dirs, tree_view);
}

static void on_remote_browse_button_clicked(GtkWidget *widget, gpointer data) {
    FTPDetails *details = g_malloc(sizeof(FTPDetails));
    GtkWidget *dialog = gtk_widget_get_toplevel(widget); 
    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");

    strncpy(details->host, gtk_entry_get_text(GTK_ENTRY(host_entry)), sizeof(details->host) - 1);   
    details->port = atoi(gtk_entry_get_text(GTK_ENTRY(port_entry)));
    strncpy(details->protocol, gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo)), sizeof(details->protocol) - 1);
    strncpy(details->username, gtk_entry_get_text(GTK_ENTRY(username_entry)), sizeof(details->username) - 1);
    strncpy(details->password, gtk_entry_get_text(GTK_ENTRY(password_entry)), sizeof(details->password) - 1);
    strncpy(details->remote_path, "/", sizeof(details->remote_path) - 1); 
    
    create_remote_browse_dialog(GTK_WINDOW(dialog), details); 
}

static void set_list_item_add(
    const gchar *description, 
    const gchar *host, 
    int port, 
    const gchar *protocol, 
    const gchar *username, 
    const gchar *local_path, 
    const gchar *remote_path
) {
    GtkWidget *tree_view = gui_get_tree_view();
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)));
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(
        store, 
        &iter, 
        0, description, 
        1, host, 
        2, port, 
        3, protocol, 
        4, username, 
        5, local_path, 
        6, remote_path, 
        7, "Not Connected", 
        -1
    );
}

void on_add_dialog_ok_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = GTK_WIDGET(data);
    GtkWidget *description_entry = g_object_get_data(G_OBJECT(dialog), "description_entry");
    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");    
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *local_path_entry = g_object_get_data(G_OBJECT(dialog), "local_path_entry");
    GtkWidget *remote_path_entry = g_object_get_data(G_OBJECT(dialog), "remote_path_entry");

    const gchar *description = gtk_entry_get_text(GTK_ENTRY(description_entry));
    const gchar *host = gtk_entry_get_text(GTK_ENTRY(host_entry));
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));  
    const gchar *local_path = gtk_entry_get_text(GTK_ENTRY(local_path_entry));
    const gchar *remote_path = gtk_entry_get_text(GTK_ENTRY(remote_path_entry));

    REQUIRE_FIELD(dialog, description, "Description cannot be empty.");
    REQUIRE_FIELD(dialog, host, "Host cannot be empty.");    
    REQUIRE_FIELD(dialog, port_str, "Port cannot be empty.");
    REQUIRE_FIELD(dialog, username, "Username cannot be empty.");   
    REQUIRE_FIELD(dialog, password, "Password cannot be empty.");
    REQUIRE_FIELD(dialog, local_path, "Local path cannot be empty.");
    REQUIRE_FIELD(dialog, remote_path, "Remote path cannot be empty.");

    int port = atoi(port_str);
    save_ftp(description, host, port, protocol, username, password, local_path, remote_path);
    set_list_item_add(description, host, port, protocol, username, local_path, remote_path);
    gtk_widget_destroy(dialog);
}

static void on_add_dialog_cancel_button_clicked(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(GTK_WIDGET(data));
}

static void set_list_item_update(
    GtkTreeView *list, 
    const gchar *description, 
    const gchar *host, 
    int port, 
    const gchar *protocol, 
    const gchar *username, 
    const gchar *local_path, 
    const gchar *remote_path
) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_list_store_set(
            GTK_LIST_STORE(model), 
            &iter, 
            0, description, 
            1, host, 
            2, port, 
            3, protocol, 
            4, username, 
            5, local_path, 
            6, remote_path, 
            7, 
            "Not Connected", 
            -1
        );
    }    
}

static void on_update_dialog_ok_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *tree_view = gui_get_tree_view();
    GtkWidget *dialog = GTK_WIDGET(data);
    GtkWidget *description_entry = g_object_get_data(G_OBJECT(dialog), "description_entry");
    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");    
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *local_path_entry = g_object_get_data(G_OBJECT(dialog), "local_path_entry");   
    GtkWidget *remote_path_entry = g_object_get_data(G_OBJECT(dialog), "remote_path_entry");

    const gchar *description = gtk_entry_get_text(GTK_ENTRY(description_entry));
    const gchar *host = gtk_entry_get_text(GTK_ENTRY(host_entry));
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));    
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const gchar *local_path = gtk_entry_get_text(GTK_ENTRY(local_path_entry));
    const gchar *remote_path = gtk_entry_get_text(GTK_ENTRY(remote_path_entry));   

    REQUIRE_FIELD(dialog, description, "Description cannot be empty.");
    REQUIRE_FIELD(dialog, host, "Host cannot be empty.");    
    REQUIRE_FIELD(dialog, port_str, "Port cannot be empty.");
    REQUIRE_FIELD(dialog, username, "Username cannot be empty.");   
    REQUIRE_FIELD(dialog, password, "Password cannot be empty.");
    REQUIRE_FIELD(dialog, local_path, "Local path cannot be empty.");
    REQUIRE_FIELD(dialog, remote_path, "Remote path cannot be empty.");

    int port = atoi(port_str);
    update_ftp(description, host, port, protocol, username, password, local_path, remote_path);
    set_list_item_update(GTK_TREE_VIEW(tree_view), description, host, port, protocol, username, local_path, remote_path);
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_update_dialog_cancel_button_clicked(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(GTK_WIDGET(data));
}

static GtkWidget *create_local_path_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Local Path:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 6, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 6, 1, 1);

    GtkWidget *button = gtk_button_new_with_label("Browse");
    g_signal_connect(button, "clicked", G_CALLBACK(on_local_browse_button_clicked), entry);
    gtk_grid_attach(GTK_GRID(grid), button, 2, 6, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "local_path_entry", entry);
    return entry;
}

static GtkWidget *create_remote_path_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Remote Path:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 7, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 7, 1, 1);

    GtkWidget *button = gtk_button_new_with_label("Browse");
    g_signal_connect(button, "clicked", G_CALLBACK(on_remote_browse_button_clicked), entry); // Added this line
    gtk_grid_attach(GTK_GRID(grid), button, 2, 7, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "remote_path_entry", entry);
    return entry;
}

void create_add_dialog(GtkWindow *parent) {
    GtkWidget *dialog = create_dialog(parent, "Add FTP Connection");
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    create_description_field(dialog, grid);
    create_host_field(dialog, grid);
    create_port_field(dialog, grid);
    create_protocol_field(dialog, grid);
    create_username_field(dialog, grid);
    create_password_field(dialog, grid);    
    create_local_path_field(dialog, grid);
    create_remote_path_field(dialog, grid);   

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_add_dialog_ok_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), ok_button, 1, 8, 1, 1);

    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_add_dialog_cancel_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), cancel_button, 2, 8, 1, 1);  

    gtk_widget_show_all(dialog);
}

void create_update_dialog(GtkWindow *parent, gpointer data) {
    GtkWidget *entry;
    FTPDetails *details = gui_get_ftp_details();

    GtkWidget *dialog = create_dialog(parent, "Update FTP Connection");
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    entry = create_description_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details->description);

    entry = create_host_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details->host);

    entry = create_port_field(dialog, grid);
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", details->port);
    gtk_entry_set_text(GTK_ENTRY(entry), port_str);

    GtkWidget *protocol_combo = create_protocol_field(dialog, grid);
    if (g_strcmp0(details->protocol, "FTP") == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 0);
    } else if (g_strcmp0(details->protocol, "SFTP") == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 1);
    }
     
    entry = create_username_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details->username);

    entry = create_password_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details->password);  

    entry = create_local_path_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details->local_path);
    
    entry = create_remote_path_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details->remote_path);

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_update_dialog_ok_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), ok_button, 1, 8, 1, 1);

    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_update_dialog_cancel_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), cancel_button, 2, 8, 1, 1); 

    gtk_widget_show_all(dialog);
}