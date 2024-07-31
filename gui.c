#include <gtk/gtk.h>
#include <uuid/uuid.h>
#include <curl/curl.h>
#include <glib.h>
#include "gui.h"
#include "ftp.h"
#include "utils.h"
#include "storage.h"
#include "icon.h"

static GtkWidget *main_window;
static GtkWidget *info_tree_view;
static GtkWidget *status_area;
static FTPDetails ftp_details[256];
static gint ftp_details_count = 0;




    

static gboolean is_directory(const gchar *path) {
    return g_str_has_suffix(path, "/") || g_str_has_prefix(path, "d");
}






















