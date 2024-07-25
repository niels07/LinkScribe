#ifndef FTP_H
#define FTP_H

#include <gtk/gtk.h>
#include "storage.h"

typedef struct {
    char host[256];
    char username[256];
    char password[256];
    int port;
    char protocol[10];
    GtkWidget *output_text_view;
    GtkTreeIter iter;
    GtkListStore *store;
} FTPConnectData;

extern gboolean ftp_connect(const FTPDetails *details, GtkWidget *output_text_view, GtkTreeIter iter, GtkListStore *store);

#endif
