#include <gtk/gtk.h>
#include <uuid/uuid.h>
#include <curl/curl.h>
#include "gui.h"
#include "ftp.h"
#include "utils.h"
#include "storage.h"
#include "icon.h"
#include "loading_data.h"

static GtkWidget *main_window;
static GtkWidget *info_tree_view;
static GtkWidget *status_area;
static FTPDetails ftp_details[256];
static gint ftp_details_count = 0;

typedef struct {
    GtkTextBuffer *buffer;
    gchar *text;
} InsertTextData;

#define REQUIRE_FIELD(dialog, field, message)               \
    do {                                                    \
        if (g_strcmp0(field, "") == 0) {                    \
            show_error_dialog(GTK_DIALOG(dialog), message); \
            return;                                         \
        }                                                   \
    } while (0)
    
static void on_update_menu_activate(GtkWidget *widget, gpointer data);

static gboolean insert_text_idle(gpointer data) {
    InsertTextData *insert_data = (InsertTextData *)data;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(insert_data->buffer, &end);
    gtk_text_buffer_insert(insert_data->buffer, &end, insert_data->text, -1);
    g_free(insert_data->text);
    g_free(insert_data);
    return FALSE;
}

static gint get_selected_index(void) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint index = -1; 
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(info_tree_view));

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

void on_ftp_list_dirs(void *arg, const char *directory) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(arg);
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(tree_view));
    GtkTreeIter iter;

    // Split the directory entries
    char *dir_copy = g_strdup(directory);
    char *line = strtok(dir_copy, "\n");

    while (line != NULL) {
        if (g_str_has_suffix(line, "/") || g_str_has_prefix(line, "d")) {
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, 0, line, -1);
            PRINT_MESSAGE("Added directory: %s", line);
        }
        line = strtok(NULL, "\n");
    }

    g_free(dir_copy);

    // After the first directory entry, hide the loading image and show the tree view
    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL) > 0) {
        GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(tree_view));
        GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
        for (GList *child = children; child != NULL; child = child->next) {
            if (GTK_IS_IMAGE(child->data)) {
                gtk_widget_hide(GTK_WIDGET(child->data));
            }
        }
        g_list_free(children);
        gtk_widget_show(tree_view);
    }
}

static GtkWidget *create_text_view(void) {
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    return text_view;
}

void on_directory_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    FTPDetails *details = (FTPDetails *)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar *directory;
        gtk_tree_model_get(model, &iter, 0, &directory, -1);

        // Add the selected directory to the FTP details
        strncpy(details->remote_path, directory, sizeof(details->remote_path) - 1);

        // Clear the previous entries
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(tree_view));
        gtk_list_store_clear(store);

        // Display the loading image
        GtkWidget *parent_widget = gtk_widget_get_parent(GTK_WIDGET(tree_view));
        if (GTK_IS_CONTAINER(parent_widget)) {
            GList *children = gtk_container_get_children(GTK_CONTAINER(parent_widget));
            for (GList *child = children; child != NULL; child = child->next) {
                if (GTK_IS_IMAGE(child->data)) {
                    gtk_widget_show(GTK_WIDGET(child->data));
                }
            }
            g_list_free(children);
        }

        // Start directory listing for the selected directory
        ftp_list_dirs(details, on_ftp_list_dirs, tree_view);

        g_free(directory);
    }
}

static GtkWidget *create_directory_tree_view() {
    GtkWidget *tree_view = gtk_tree_view_new();
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Directory", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));
    g_object_unref(store);

    gtk_widget_set_size_request(tree_view, 600, 400); // Ensure tree view is appropriately sized

    return tree_view;
}

static void create_remote_browse_window(FTPDetails *details) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Remote Directory Listing");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *loading_image = create_loading_image();
    gtk_box_pack_start(GTK_BOX(vbox), loading_image, FALSE, FALSE, 0);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    GtkWidget *tree_view = create_directory_tree_view();
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_directory_selected), details);

    gtk_widget_show_all(window);

    // Hide the tree view initially and show the loading image
    gtk_widget_hide(tree_view);
    gtk_widget_show(loading_image);

    // Start directory listing
    PRINT_MESSAGE("Starting directory listing...");
    ftp_list_dirs(details, on_ftp_list_dirs, tree_view);
}



static void on_remote_browse_button_clicked(GtkWidget *widget, gpointer data) {
    FTPDetails details;
    GtkWidget *dialog = gtk_widget_get_toplevel(widget); 
    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");

    strncpy(details.host, gtk_entry_get_text(GTK_ENTRY(host_entry)), sizeof(details.host) - 1);
    strncpy(details.username, gtk_entry_get_text(GTK_ENTRY(username_entry)), sizeof(details.username) - 1);
    strncpy(details.password, gtk_entry_get_text(GTK_ENTRY(password_entry)), sizeof(details.password) - 1);
    details.port = atoi(gtk_entry_get_text(GTK_ENTRY(port_entry)));
    strncpy(details.protocol, gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo)), sizeof(details.protocol) - 1);

    create_remote_browse_window(&details); 
}

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

static void save_ftp(gchar *id, const gchar *description, const gchar *host, const gchar *local_path, const gchar *username, const gchar *password, const int port, const gchar *protocol) {
    FTPDetails details;
    uuid_t binuuid;
    uuid_generate(binuuid);
    uuid_unparse(binuuid, details.id);
    strncpy(details.description, description, sizeof(details.description) - 1);
    strncpy(details.host, host, sizeof(details.host) - 1);
    strncpy(details.local_path, local_path, sizeof(details.local_path) - 1);
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

static void update_ftp(const gchar *host, const gchar *local_path, const gchar *username, const gchar *password, const int port, const gchar *protocol) {
    gint index = get_selected_index();
    if (index < 0) {
        return;
    }
    FTPDetails details = ftp_details[index];    
    strncpy(details.host, host, sizeof(details.host) - 1);
    strncpy(details.local_path, local_path, sizeof(details.local_path) - 1);
    strncpy(details.username, username, sizeof(details.username) - 1);
    strncpy(details.password, password, sizeof(details.password) - 1);
    details.port = port;
    strncpy(details.protocol, protocol, sizeof(details.protocol) - 1);
    update_ftp_details(&details);
}

static void set_list_item_add(const gchar *description, const gchar *local_path, const gchar *username, const gchar *protocol, const gchar *host, int port) {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(info_tree_view)));
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, description, 1, local_path, 2, username, 3, protocol, 4, host, 5, port, 6, "",  7, "Not Connected", -1);
}

static void set_list_item_update(GtkTreeView *list, const gchar *local_path, const gchar *username, const gchar *protocol, const gchar *host, int port) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list);
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, local_path, 2, username, 3, protocol, 4, host, 5, port, 7, "Not Connected", -1);
    }    
}

static void on_add_dialog_response(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = GTK_WIDGET(data);
    GtkWidget *description_entry = g_object_get_data(G_OBJECT(dialog), "description_entry");
    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *local_path_entry = g_object_get_data(G_OBJECT(dialog), "local_path_entry");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");

    const gchar *description = gtk_entry_get_text(GTK_ENTRY(description_entry));
    const gchar *host = gtk_entry_get_text(GTK_ENTRY(host_entry));
    const gchar *local_path = gtk_entry_get_text(GTK_ENTRY(local_path_entry));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo)); // Corrected this line

    REQUIRE_FIELD(dialog, description, "Description cannot be empty.");
    REQUIRE_FIELD(dialog, host, "Host cannot be empty.");
    REQUIRE_FIELD(dialog, local_path, "Local directory cannot be empty.");
    REQUIRE_FIELD(dialog, username, "Username cannot be empty.");   
    REQUIRE_FIELD(dialog, password, "Password cannot be empty.");
    REQUIRE_FIELD(dialog, port_str, "Port cannot be empty.");

    int port = atoi(port_str);
    gchar id[256];
    save_ftp(id, description, host, local_path, username, password, port, protocol);
    set_list_item_add(description, local_path, username, protocol, host, port);
    gtk_widget_destroy(dialog);
}

static void on_update_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id != GTK_RESPONSE_OK) {
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return;
    }

    GtkWidget *host_entry = g_object_get_data(G_OBJECT(dialog), "host_entry");
    GtkWidget *local_path_entry = g_object_get_data(G_OBJECT(dialog), "local_path_entry");
    GtkWidget *username_entry = g_object_get_data(G_OBJECT(dialog), "username_entry");
    GtkWidget *password_entry = g_object_get_data(G_OBJECT(dialog), "password_entry");
    GtkWidget *port_entry = g_object_get_data(G_OBJECT(dialog), "port_entry");
    GtkWidget *protocol_combo = g_object_get_data(G_OBJECT(dialog), "protocol_combo");

    const gchar *host = gtk_entry_get_text(GTK_ENTRY(host_entry));
    const gchar *local_path = gtk_entry_get_text(GTK_ENTRY(local_path_entry));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo)); // Corrected this line

    REQUIRE_FIELD(dialog, host, "Host cannot be empty.");
    REQUIRE_FIELD(dialog, local_path, "Local directory cannot be empty.");
    REQUIRE_FIELD(dialog, username, "Username cannot be empty.");
    REQUIRE_FIELD(dialog, password, "Password cannot be empty.");
    REQUIRE_FIELD(dialog, port_str, "Port cannot be empty.");

    int port = atoi(port_str);
    update_ftp(host, local_path, username, password, port, protocol);
    set_list_item_update(GTK_TREE_VIEW(info_tree_view), local_path, username, protocol, host, port);
    gtk_widget_destroy(GTK_WIDGET(dialog));
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

static void on_ftp_connect(void *arg, const char *message, bool success) {
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
        FTPDetails details = ftp_details[index];
        ftp_connect(&details, on_ftp_connect, model);     
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
    add_tree_view_column(tree_view, "Protocol", 1);
    add_tree_view_column(tree_view, "Host", 2);
    add_tree_view_column(tree_view, "Port", 3);
    add_tree_view_column(tree_view, "Username", 4);
    add_tree_view_column(tree_view, "Local Directory", 5);
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
            1, details->local_path,
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

static GtkWidget *create_username_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Username:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 2, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "username_entry", entry);
    return entry;
}

static GtkWidget *create_password_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Password:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 3, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "password_entry", entry);
    return entry;
}

static GtkWidget *create_port_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Port:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "21"); 
    gtk_grid_attach(GTK_GRID(grid), entry, 1, 4, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "port_entry", entry);
    return entry;
}

static GtkWidget *create_protocol_field(GtkWidget *dialog, GtkWidget *grid) {
    GtkWidget *label = gtk_label_new("Protocol:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);

    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FTP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "SFTP");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_grid_attach(GTK_GRID(grid), combo, 1, 5, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "protocol_combo", combo);
    return combo;
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

static void create_add_dialog(void) {
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Add FTP Connection");
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    create_description_field(dialog, grid);
    create_host_field(dialog, grid);
    create_username_field(dialog, grid);
    create_password_field(dialog, grid);
    create_port_field(dialog, grid);
    create_protocol_field(dialog, grid);
    create_local_path_field(dialog, grid);
    create_remote_path_field(dialog, grid);   

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_add_dialog_response), dialog);
    gtk_grid_attach(GTK_GRID(grid), ok_button, 1, 8, 1, 1);

    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), cancel_button, 2, 8, 1, 1);  

    gtk_widget_show_all(dialog);
}

static void on_add_menu_activate(GtkWidget *widget, gpointer data) {
    create_add_dialog();
}

static void create_update_dialog(gpointer data) {
    GtkWidget *entry;
    gint index = get_selected_index();
    FTPDetails details = ftp_details[index];  

    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Update FTP Connection");
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    entry = create_description_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details.description);

    entry = create_host_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details.host);
     
    entry = create_username_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details.username);

    entry = create_password_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details.password);

    entry = create_port_field(dialog, grid);
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", details.port);
    gtk_entry_set_text(GTK_ENTRY(entry), port_str);

    GtkWidget *protocol_combo = create_protocol_field(dialog, grid);
    if (g_strcmp0(details.protocol, "FTP") == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 0);
    } else if (g_strcmp0(details.protocol, "SFTP") == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 1);
    }

    entry = create_local_path_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details.local_path);
    
    entry = create_remote_path_field(dialog, grid);
    gtk_entry_set_text(GTK_ENTRY(entry), details.remote_path);

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_update_dialog_response), dialog);
    gtk_grid_attach(GTK_GRID(grid), ok_button, 1, 8, 1, 1);

    GtkWidget *cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), dialog);
    gtk_grid_attach(GTK_GRID(grid), cancel_button, 2, 8, 1, 1); 

    gtk_widget_show_all(dialog);
}


static void on_update_menu_activate(GtkWidget *widget, gpointer data) {
    create_update_dialog(data);
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
    
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    GError *error = NULL;

    if (!gdk_pixbuf_loader_write(loader, icon_png, icon_png_len, &error)) {
        g_warning("Failed to load icon data: %s", error->message);
        g_error_free(error);
        return;
    }

    if (!gdk_pixbuf_loader_close(loader, &error)) {
        g_warning("Failed to close icon loader: %s", error->message);
        g_error_free(error);
        return;
    }

    GdkPixbuf *icon = gdk_pixbuf_loader_get_pixbuf(loader);
    if (icon == NULL) {
        g_warning("Failed to get pixbuf from loader");
        g_object_unref(loader);
        return;
    }
    gtk_window_set_icon(GTK_WINDOW(main_window), icon);
    g_object_unref(loader);

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