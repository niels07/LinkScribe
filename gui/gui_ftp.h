#ifndef GUI_FTP_H
#define GUI_FTP_H

#include <gtk/gtk.h>
extern void on_ftp_connect(void *arg, const char *message, bool success);
extern void load_ftp();
extern void save_ftp(const gchar *description, const gchar *host, int port, const gchar *protocol, const gchar *username, const gchar *password, const gchar *local_path, const gchar *remote_path);
extern void update_ftp(const gchar *description, const gchar *host, int port, const gchar *protocol, const gchar *username, const gchar *password, const gchar *local_path, const gchar *remote_path);
#endif