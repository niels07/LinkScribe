#ifndef STORAGE_H
#define STORAGE_H

#include <gtk/gtk.h>

typedef struct {
    char id[37]; // UUID string
    char description[256];
    char host[256];
    char remote_path[256];
    char local_dir[256];
    char username[256];
    char password[256];
    int port;
    char protocol[10];
} FTPDetails;


void save_ftp_details(const FTPDetails *details);
void load_ftp_details(GList **details_list);
void update_ftp_details(const FTPDetails *details);
void remove_ftp_details(const char *description);

#endif
