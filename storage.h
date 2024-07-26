#ifndef STORAGE_H
#define STORAGE_H

#include <gtk/gtk.h>
#include "ftp.h"

void save_ftp_details(const FTPDetails *details);
void load_ftp_details(GList **details_list);
void update_ftp_details(const FTPDetails *details);
void remove_ftp_details(const char *description);

#endif
