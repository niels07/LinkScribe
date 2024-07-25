#include <gtk/gtk.h>
#include <uuid/uuid.h>
#include "gui.h"
#include "ftp.h"
#include "utils.h"
#include "storage.h"

static GtkWidget *main_window;
static GtkWidget *info_tree_view;
static GtkWidget *status_area;
static FTPDetails ftp_details[256];
static gint ftp_details_count = 0;

#define REQUIRE_FIELD(dialog, field, message)               \
    do {                                                    \
        if (g_strcmp0(field, "") == 0) {                    \
            show_error_dialog(GTK_DIALOG(dialog), message); \
            return;                                         \
        }                                                   \
    } while (0)
    


static gint get_selected_index(void) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint index = -1; 
    GtkTreeSelection *selection = gtk_tree_view_get_selection(info_tree_view);

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
        gint *indices = gtk_tree_path_get_indices(path);
        if (indices) {
            index = indices[0];
        }
        gtk_tree_path_free(path);
    }
    return index;
}

static void on_cancel_button_clicked(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(GTK_WIDGET(data));
}

static void on_browse_button_clicked(GtkWidget *widget, gpointer data) {
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

static void show_error_dialog(GtkDialog *parent, const gchar *message) {
    GtkWidget *error_dialog = gtk_message_dialog_new(
        GTK_WINDOW(parent), 
        GTK_DIALOG_DESTROY_WITH_PARENT, 
        GTK_MESSAGE_ERROR, 
        GTK_BUTTONS_CLOSE, 
        message);
    gtk_dialog_run(GTK_DIALOG(error_dialog));
    gtk_widget_destroy(error_dialog);
}

static void save_ftp(gchar *id, const gchar *description, const gchar *host, const gchar *local_dir, const gchar *username, const gchar *password, const int port, const gchar *protocol) {
    FTPDetails details;
    uuid_t binuuid;
    uuid_generate(binuuid);
    uuid_unparse(binuuid, details.id);
    strncpy(details.description, description, sizeof(details.description) - 1);
    strncpy(details.host, host, sizeof(details.host) - 1);
    strncpy(details.local_dir, local_dir, sizeof(details.local_dir) - 1);
    strncpy(details.username, username, sizeof(details.username) - 1);
    strncpy(details.password, password, sizeof(details.password) - 1);
    details.port = port;
    strncpy(details.protocol, protocol, sizeof(details.protocol) - 1);
    details.remote_path[0] = '\0';
    save_ftp_details(&details);
    ftp_details[ftp_details_count++] = details;
    strncpy(id, details.id, sizeof(id) - 1);
    ftp_details[ftp_details_count++] = details;
}

static void update_ftp(const gchar *host, const gchar *local_dir, const gchar *username, const gchar *password, const int port, const gchar *protocol) {
    gint index = get_selected_index();
    if (index < 0) {
        return;
    }
    FTPDetails details = ftp_details[index];    
    strncpy(details.host, host, sizeof(details.host) - 1);
    strncpy(details.local_dir, local_dir, sizeof(details.local_dir) - 1);
    strncpy(details.username, username, sizeof(details.username) - 1);
    strncpy(details.password, password, sizeof(details.password) - 1);
    details.port = port;
    strncpy(details.protocol, protocol, sizeof(details.protocol) - 1);
    update_ftp_details(&details);
}

static void set_list_item_add(const gchar *description, const gchar *local_dir, const gchar *username, const gchar *protocol, const gchar *host, const gchar *port) {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(info_tree_view)));
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, description, 1, local_dir, 2, username, 3, protocol, 4, host, 5, port, 6, "",  7, "Not Connected", -1);
}

static void set_list_item_update(GtkTreeView *list, const gchar *local_dir, const gchar *username, const gchar *protocol, const gchar *host, const int port) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, local_dir, 2, username, 3, protocol, 4, host, 5, port, 7, "Not Connected", -1);
    }    
}

static void on_add_dialog_response(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = GTK_WIDGET(data);
    GtkWidget *description_entry = g_object_get_data(G_OBJECT(dialog), "description_entry");
    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *local_dir_entry = g_object_get_data(G_OBJECT(dialog), "local_dir_entry");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");

    const gchar *description = gtk_entry_get_text(GTK_ENTRY(description_entry));
    const gchar *host = gtk_entry_get_text(GTK_ENTRY(host_entry));
    const gchar *local_dir = gtk_entry_get_text(GTK_ENTRY(local_dir_entry));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));

    REQUIRE_FIELD(dialog, description, "Description cannot be empty.");
    REQUIRE_FIELD(dialog, host, "Host cannot be empty.");
    REQUIRE_FIELD(dialog, local_dir, "Local directory cannot be empty.");
    REQUIRE_FIELD(dialog, username, "Username cannot be empty.");   
    REQUIRE_FIELD(dialog, password, "Password cannot be empty.");
    REQUIRE_FIELD(dialog, port_str, "Port cannot be empty.");

    int port = atoi(port_str);
    gchar id[256];
    save_ftp(id, description, host, local_dir, username, password, port, protocol);
    set_list_item_add(description, local_dir, username, protocol, host, port_str);
    gtk_widget_destroy(dialog);
}

static void on_update_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id != GTK_RESPONSE_OK) {
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return;
    }

    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *local_dir_entry = g_object_get_data(G_OBJECT(dialog), "local_dir_entry");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");

    const gchar *host = gtk_entry_get_text(GTK_ENTRY(host_entry));
    const gchar *local_dir = gtk_entry_get_text(GTK_ENTRY(local_dir_entry));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));    

    REQUIRE_FIELD(dialog, host, "Host cannot be empty.");
    REQUIRE_FIELD(dialog, local_dir, "Local directory cannot be empty.");
    REQUIRE_FIELD(dialog, username, "Username cannot be empty.");
    REQUIRE_FIELD(dialog, password, "Password cannot be empty.");
    REQUIRE_FIELD(dialog, port_str, "Port cannot be empty.");

    int port = atoi(port_str);
    update_ftp(host, local_dir, username, password, port, protocol);
    set_list_item_update(GTK_TREE_VIEW(info_tree_view), local_dir, username, protocol, host, port);
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_update_menu_activate(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(info_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    char *id, *host, *local_dir, *username, *password, *protocol;
    int port;

    gtk_tree_model_get(model, &iter, 0, &id, 1, &local_dir, 2, &username, 3, &protocol, 4, &host, 5, &port, -1);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Update FTP Connection",
        GTK_WINDOW(data),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_OK", GTK_RESPONSE_OK,
        "_Cancel", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    GtkWidget *host_label = gtk_label_new("Host:");
    GtkWidget *host_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(host_entry), host);
    gtk_grid_attach(GTK_GRID(grid), host_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), host_entry, 1, 0, 1, 1);

    GtkWidget *local_dir_label = gtk_label_new("Local Directory:");
    GtkWidget *local_dir_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(local_dir_entry), local_dir);
    gtk_grid_attach(GTK_GRID(grid), local_dir_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), local_dir_entry, 1, 1, 1, 1);

    GtkWidget *username_label = gtk_label_new("Username:");
    GtkWidget *username_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(username_entry), username);
    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), username_entry, 1, 2, 1, 1);

    GtkWidget *password_label = gtk_label_new("Password:");
    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_text(GTK_ENTRY(password_entry), password);
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 3, 1, 1);

    GtkWidget *port_label = gtk_label_new("Port:");
    GtkWidget *port_entry = gtk_entry_new();
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);
    gtk_entry_set_text(GTK_ENTRY(port_entry), port_str);
    gtk_grid_attach(GTK_GRID(grid), port_label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), port_entry, 1, 4, 1, 1);

    GtkWidget *protocol_label = gtk_label_new("Protocol:");
    GtkWidget *protocol_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "FTP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "SFTP");
    gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), strcmp(protocol, "SFTP") == 0 ? 1 : 0);
    gtk_grid_attach(GTK_GRID(grid), protocol_label, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), protocol_combo, 1, 5, 1, 1);

    g_object_set_data(G_OBJECT(dialog), "id", id);
    g_object_set_data(G_OBJECT(dialog), "host_entry", host_entry);
    g_object_set_data(G_OBJECT(dialog), "local_dir_entry", local_dir_entry);
    g_object_set_data(G_OBJECT(dialog), "username_entry", username_entry);
    g_object_set_data(G_OBJECT(dialog), "password_entry", password_entry);
    g_object_set_data(G_OBJECT(dialog), "port_entry", port_entry);
    g_object_set_data(G_OBJECT(dialog), "protocol_combo", protocol_combo);

    g_signal_connect(dialog, "response", G_CALLBACK(on_update_dialog_response), dialog);

    gtk_widget_show_all(dialog);

    g_free(id);
    g_free(host);
    g_free(local_dir);
    g_free(username);
    g_free(protocol);
}

static void on_remove_menu_activate(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(info_tree_view));
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

static void on_connect_menu_activate(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(info_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }

    gtk_tree_model_get(model, &iter, -1);
    log_message("Connecting...", status_area);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 7, "Connecting", -1);
    gint index = get_selected_index();
    if (index >= 0) {
        FTPDetails details = ftp_details[index];    -
        ftp_connect(&details, status_area, iter, GTK_LIST_STORE(model));     
    }   
}

static void on_disconnect_menu_activate(GtkWidget *widget, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(info_tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        log_message("Disconnecting...", status_area);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 7, "Not Connected", -1);
    }
}

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

static GtkWidget *create_window() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "LinkScribe");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    return window;
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

static void add_tree_view_column(GtkWidget *tree_view, const gchar *text, const int index) {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(text, renderer, "text", index, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
}

static GtkWidget *create_tree_view() {
    GtkWidget *tree_view = gtk_tree_view_new();
    add_tree_view_column(tree_view, "Description", 0);
    add_tree_view_column(tree_view, "Local Directory", 1);
    add_tree_view_column(tree_view, "Username", 2);
    add_tree_view_column(tree_view, "Protocol", 3);
    add_tree_view_column(tree_view, "Host", 4);
    add_tree_view_column(tree_view, "Port", 5);
    add_tree_view_column(tree_view, "Remote Directory", 6);  
    add_tree_view_column(tree_view, "Status", 7);
    g_signal_connect(tree_view, "button-press-event", G_CALLBACK(on_info_tree_view_right_click), NULL);
    return tree_view;
}

static void add_scroll_window(GtkWidget *box) {
    GtkWidget *scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll_window), info_tree_view);
    gtk_box_pack_start(GTK_BOX(box), scroll_window, TRUE, TRUE, 5);
}

static GtkWidget *create_text_view(void) {
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    return text_view;
}

static GtkWidget *create_scrolled_window(GtkWidget *box) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), status_area);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 5);
    return scrolled_window;
}

static void load_ftp() {
    GList *details_list = NULL;
    load_ftp_details(&details_list);
    ftp_details_count = 0;
    for (GList *l = details_list; l != NULL; l = l->next) {
        FTPDetails *details = (FTPDetails *)l->data;
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(info_tree_view)));
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(
            store, 
            &iter,
            0, details->description,
            1, details->local_dir,
            2, details->username,
            3, details->protocol,
            4, details->host,
            5, details->port,
            6, details->remote_path,
            7, "Not Connected",
            -1
        );
        ftp_details[ftp_details_count++] = *details;
        g_free(details);
    }
    g_list_free(details_list);
}

static void create_add_dialog(void) {
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Add FTP Connection");
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    GtkWidget *description_label = gtk_label_new("Description:");
    gtk_grid_attach(GTK_GRID(grid), description_label, 0, 0, 1, 1);

    GtkWidget *description_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), description_entry, 1, 0, 2, 1);

    GtkWidget *host_label = gtk_label_new("Host:");
    gtk_grid_attach(GTK_GRID(grid), host_label, 0, 1, 1, 1);

    GtkWidget *host_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), host_entry, 1, 1, 2, 1);

    GtkWidget *remote_path_label = gtk_label_new("Remote Path:");
    gtk_grid_attach(GTK_GRID(grid), remote_path_label, 0, 2, 1, 1);

    GtkWidget *remote_path_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), remote_path_entry, 1, 2, 2, 1);

    GtkWidget *local_dir_label = gtk_label_new("Local Directory:");
    gtk_grid_attach(GTK_GRID(grid), local_dir_label, 0, 3, 1, 1);

    GtkWidget *local_dir_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), local_dir_entry, 1, 3, 1, 1);

    GtkWidget *local_dir_button = gtk_button_new_with_label("Browse");
    g_signal_connect(local_dir_button, "clicked", G_CALLBACK(on_browse_button_clicked), local_dir_entry);
    gtk_grid_attach(GTK_GRID(grid), local_dir_button, 2, 3, 1, 1);

    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_grid_attach(GTK_GRID(grid), username_label, 0, 4, 1, 1);

    GtkWidget *username_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), username_entry, 1, 4, 2, 1);

    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 5, 1, 1);

    GtkWidget *password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 5, 2, 1);

    GtkWidget *port_label = gtk_label_new("Port:");
    gtk_grid_attach(GTK_GRID(grid), port_label, 0, 6, 1, 1);

    GtkWidget *port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(port_entry), "21"); // default FTP port
    gtk_grid_attach(GTK_GRID(grid), port_entry, 1, 6, 2, 1);

    GtkWidget *protocol_label = gtk_label_new("Protocol:");
    gtk_grid_attach(GTK_GRID(grid), protocol_label, 0, 7, 1, 1);

    GtkWidget *protocol_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "FTP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "SFTP");
    gtk_grid_attach(GTK_GRID(grid), protocol_combo, 1, 7, 2, 1);

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_add_dialog_response), dialog);
    gtk_grid_attach(GTK_GRID(grid), ok_button, 0, 8, 1, 1);

    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), cancel_button, 1, 8, 1, 1);

    g_object_set_data(G_OBJECT(dialog), "description_entry", description_entry);
    g_object_set_data(G_OBJECT(dialog), "host_entry", host_entry);
    g_object_set_data(G_OBJECT(dialog), "remote_path_entry", remote_path_entry);
    g_object_set_data(G_OBJECT(dialog), "local_dir_entry", local_dir_entry);
    g_object_set_data(G_OBJECT(dialog), "username_entry", username_entry);
    g_object_set_data(G_OBJECT(dialog), "password_entry", password_entry);
    g_object_set_data(G_OBJECT(dialog), "port_entry", port_entry);
    g_object_set_data(G_OBJECT(dialog), "protocol_combo", protocol_combo);

    gtk_widget_show_all(dialog);
}

static void on_add_menu_activate(GtkWidget *widget, gpointer data) {
    create_add_dialog();
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

static GtkWidget *create_menu_bar(void) {
    GtkWidget *menubar = gtk_menu_bar_new();  
    add_file_menu_item(menubar);
    add_connection_menu_item(menubar);
    add_help_menu_item(menubar);
    return menubar;
}

void create_main_window() {
    main_window = create_window();

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    GtkWidget *menubar = create_menu_bar();
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    info_tree_view = create_tree_view();

    GtkListStore *store = gtk_list_store_new(8, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(info_tree_view), GTK_TREE_MODEL(store));
    g_object_unref(store);

    add_scroll_window(vbox);

    status_area = create_text_view();

    GtkWidget *status_scroll = create_scrolled_window(vbox);

    GtkWidget *footer = gtk_label_new("LinkScribe v0.01");
    gtk_box_pack_start(GTK_BOX(vbox), footer, FALSE, FALSE, 5);

    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(main_window);

    load_ftp();
}