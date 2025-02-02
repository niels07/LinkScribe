#include <gtk/gtk.h>
#include "loading_data.h"

static const unsigned char loading_gif_data[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x32, 0x00, 0x32, 0x00, 0xf7, 0x00, 0x00, 
    0xff, 0xff, 0xff, 0xdc, 0xdc, 0xdc, 0xb6, 0xb6, 0xb6, 0x94, 0x94, 0x94, 0x72, 
    0x72, 0x72, 0x54, 0x54, 0x54, 0x36, 0x36, 0x36, 0x1a, 0x1a, 0x1a, 0x00, 0x00, 
    0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x32, 0x00, 0x00, 0x04, 0xff, 
    0x10, 0xc9, 0x49, 0xab, 0xbd, 0x38, 0x67, 0x8c, 0x30, 0xea, 0xbd, 0x38, 0x67, 
    0x8c, 0x73, 0x13, 0x2b, 0x1c, 0x25, 0x22, 0x33, 0x24, 0x38, 0x27, 0x4d, 0xa2, 
    0x9c, 0x1a, 0xb7, 0x82, 0x5d, 0x7b, 0xb4, 0x3c, 0xf7, 0x04, 0x23, 0x0b, 0xa6, 
    0x7c, 0x44, 0x3b, 0x57, 0x3b, 0xd1, 0xb6, 0x9b, 0xe6, 0x3c, 0x72, 0x1a, 0x77, 
    0xc6, 0xa4, 0x9b, 0x18, 0x27, 0xc5, 0xa3, 0x3b, 0x72, 0xc5, 0xa3, 0xd9, 0xac, 
    0x56, 0x4b, 0x71, 0xe4, 0x72, 0xc5, 0xf9, 0xec, 0x56, 0x4b, 0x2c, 0x59, 0x0d, 
    0x6a, 0x77, 0xb3, 0x6c, 0x56, 0x2c, 0x47, 0x28, 0x50, 0x21, 0x7d, 0x4a, 0x8f, 
    0x7c, 0xf4, 0x7c, 0x8a, 0x8c, 0x8e, 0x74, 0x21, 0x3e, 0x8d, 0x76, 0x4a, 0x91, 
    0x73, 0x3e, 0x8d, 0x75, 0x25, 0x7e, 0x8e, 0x90, 0x9f, 0x91, 0xa5, 0xa1, 0x8e, 
    0x22, 0x3f, 0x8e, 0x7c, 0x92, 0xa4, 0x7d, 0x46, 0x7d, 0x9d, 0x79, 0x8b, 0x8c, 
    0x73, 0x3e, 0x8f, 0x8d, 0x7d, 0x6e, 0x4d, 0x2b, 0x1a, 0x99, 0x22, 0x2d, 0x7a, 
    0x1e, 0x91, 0x86, 0x86, 0x82, 0x88, 0x94, 0x2e, 0x1d, 0x00, 0x3b
};
static const unsigned int loading_gif_len = 310;

static GdkPixbuf *load_loading_image_from_data() {
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, loading_gif_data, sizeof(loading_gif_data), NULL);
    gdk_pixbuf_loader_close(loader, NULL);
    GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    g_object_unref(loader);
    return pixbuf;
}

GtkWidget *create_loading_image() {
    GdkPixbuf *pixbuf = load_loading_image_from_data();
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    return image;
}