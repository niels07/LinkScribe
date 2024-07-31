#ifndef GUI_H
#define GUI_H

#include "ftp.h"

extern void gui_create_main_window(void);

extern GtkWidget *gui_get_main_window(void);

extern GtkWidget *gui_get_tree_view(void);

extern GtkWidget *gui_get_status_area(void);

extern FTPDetails *gui_get_ftp_details(void);

extern GdkPixbuf *gui_get_window_icon(void);

extern void gui_add_ftp_details(const FTPDetails *details);

extern void gui_on_ftp_list_dirs(void *arg, const char *directory);

#endif