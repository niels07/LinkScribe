
#include <gtk/gtk.h>
#include <uuid/uuid.h>
#include "ftp.h"
#include "gui.h"
#include "storage.h"
#include "utils.h"

void load_ftp() {
    GtkWidget *tree_view = gui_get_tree_view();
    GList *details_list = NULL;
    load_ftp_details(&details_list);
    for (GList *l = details_list; l != NULL; l = l->next) {
        FTPDetails *details = (FTPDetails *)l->data;
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view)));
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(
            store, 
            &iter,
            0, details->description,
            1, details->host,
            2, details->port,
            3, details->protocol,
            4, details->username,
            5, details->local_path,
            6, details->remote_path,
            7, "Not Connected",
            -1
        );
        gui_add_ftp_details(details);
        g_free(details);
    }
    g_list_free(details_list);
}

void save_ftp(
    const gchar *description, 
    const gchar *host, 
    int port, 
    const gchar *protocol, 
    const gchar *username, 
    const gchar *password, 
    const gchar *local_path,
    const gchar *remote_path
) {
    FTPDetails details;
    uuid_t binuuid;
    uuid_generate(binuuid);
    uuid_unparse(binuuid, details.id);
    strncpy(details.description, description, sizeof(details.description) - 1);
    strncpy(details.host, host, sizeof(details.host) - 1);    
    details.port = port;
    strncpy(details.protocol, protocol, sizeof(details.protocol) - 1);
    strncpy(details.username, username, sizeof(details.username) - 1);
    strncpy(details.password, password, sizeof(details.password) - 1);
    strncpy(details.remote_path, remote_path, sizeof(details.remote_path) - 1);
    save_ftp_details(&details);
    gui_add_ftp_details(&details);
}

void update_ftp(
    const gchar *description, 
    const gchar *host, 
    int port, 
    const gchar *protocol,
    const gchar *username,
    const gchar *password, 
    const gchar *local_path,
    const gchar *remote_path
) {
    FTPDetails *details = gui_get_ftp_details();
    strncpy(details->description, description, sizeof(details->description) - 1);
    strncpy(details->host, host, sizeof(details->host) - 1);
    details->port = port;
    strncpy(details->protocol, protocol, sizeof(details->protocol) - 1);
    strncpy(details->username, username, sizeof(details->username) - 1);
    strncpy(details->password, password, sizeof(details->password) - 1);
    strncpy(details->local_path, local_path, sizeof(details->local_path) - 1);
    strncpy(details->remote_path, remote_path, sizeof(details->remote_path) - 1);
    update_ftp_details(details);
}

void gui_on_ftp_list_dirs(void *arg, const char *directory) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(arg);
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(tree_view));
    GtkTreeIter iter;

    PRINT_MESSAGE("Received directory data: %s", directory);

    char *dir_copy = g_strdup(directory);
    char *line = strtok(dir_copy, "\n");

    while (line != NULL) {
        PRINT_MESSAGE("Processing line: %s", line);

        if (!g_utf8_validate(line, -1, NULL)) {
            PRINT_ERROR("Invalid UTF-8 string: %s", line);
            line = strtok(NULL, "\n");
            continue;
        }
        
        if (line[0] != 'd') {
            // Only show directories
            line = strtok(NULL, "\n");
            continue;
        }

        gchar **fields = g_regex_split_simple("\\s+", line, 0, 0);
        if (strcmp(fields[8], ".") == 0 || strcmp(fields[8], "..") == 0) {
            // Ignore '.' and '..' 
            line = strtok(NULL, "\n");
            continue;
        }

        gchar updated[256];
        sprintf(updated, "%s %s %s", fields[5], fields[6], fields[7]);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, fields[8], 1, fields[0], 2, fields[2], 3, fields[3], 4, updated, -1);
        PRINT_MESSAGE("Added directory: %s", fields[8]);
        g_strfreev(fields); 
        line = strtok(NULL, "\n");
    }

    g_free(dir_copy);

    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL) > 0) {
        gtk_widget_show(GTK_WIDGET(tree_view));
    }    
}