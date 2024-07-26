#ifndef UTILS_H
#define UTILS_H

#include <gtk/gtk.h>

extern void log_message(const gchar *message, GtkWidget *output);
extern void print_error(const char *function, const char *message, ...);
extern void print_message(const char *function, const char *message, ...);

#define PRINT_ERROR(message, ...) print_error(__func__, message, ##__VA_ARGS__)
#define PRINT_MESSAGE(message, ...) print_message(__func__, message, ##__VA_ARGS__)

#endif
