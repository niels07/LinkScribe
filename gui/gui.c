#include <gtk/gtk.h>
#include "gui.h"
#include "gui_ftp.h"
#include "gui_menu.h"
#include "gui_tree_view.h"
#include "ftp.h"
#include "icon.h"
#include "utils.h"

static GtkWidget *main_window;
static GtkWidget *info_tree_view;
static GtkWidget *status_area;
static FTPDetails ftp_details[256];
static gint ftp_details_count = 0;
static GdkPixbuf *window_icon;

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

static GtkWidget *create_window() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "LinkScribe");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    return window;
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

GtkWidget *gui_get_main_window(void) {
    return main_window;
}

GtkWidget *gui_get_tree_view(void) {
    return info_tree_view;
}

GtkWidget *gui_get_status_area(void) {
    return status_area;
}

GdkPixbuf *gui_get_window_icon(void) {
    return window_icon;
}

FTPDetails *gui_get_ftp_details(void) {
    gint index = get_selected_index();
    return ftp_details + index;
}

void gui_add_ftp_details(const FTPDetails *details) {
    ftp_details[ftp_details_count++] = *details;
}

void gui_create_main_window(void) {
    main_window = create_window();
    
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    GError *error = NULL;

    if (!gdk_pixbuf_loader_write(loader, icon_png, icon_png_len, &error)) {
        PRINT_ERROR("Failed to load icon data: %s", error->message);
        g_error_free(error);        
    } else if (!gdk_pixbuf_loader_close(loader, &error)) {
        PRINT_ERROR("Failed to close icon loader: %s", error->message);
        g_error_free(error);
    }

    window_icon = gdk_pixbuf_loader_get_pixbuf(loader);
    gtk_window_set_icon(GTK_WINDOW(main_window), window_icon);
    g_object_unref(loader);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    GtkWidget *menubar = create_menu_bar();
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    info_tree_view = create_tree_view();

    GtkListStore *store = gtk_list_store_new(8, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
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