#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include "utils.h"

static void convert_function(const char *function, char *target) {
    for (int i = 0; function[i]; i++) {
        if (islower(function[i])) {
            target[i] = toupper(function[i]);
        } else if (function[i] == '_') {
            target[i] = ' ';
        } else {
            target[i] = function[i];
        }
    }
    target[strlen(function)] = '\0'; // Null-terminate the target string
}

void log_message(const gchar *message, GtkWidget *output) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}

void print_error(const char *function, const char *message, ...) {
    char f[strlen(function) + 1];
    convert_function(function, f);
    va_list vl;
    va_start(vl, message);
    char format[256];
    vsprintf(format, message, vl);
    fprintf(stderr, "%s: %s\n", f, format);
    va_end(vl);
}

void print_message(const char *function, const char *message, ...) {
    char f[strlen(function) + 1];
    convert_function(function, f);
    va_list vl;
    va_start(vl, message);
    char format[256];
    vsnprintf(format, sizeof(format), message, vl);
    fprintf(stdout, "%s: %s\n", f, format);
    va_end(vl);
}
