#ifndef GUI_TREE_VIEW_H
#define GUI_TREE_VIEW_H

#include <gtk/gtk.h>

extern GtkWidget *create_tree_view(void);
extern void add_tree_view_column(GtkWidget *tree_view, const gchar *text, const int index);

#endif