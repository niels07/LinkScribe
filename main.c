#include <gtk/gtk.h>
#include "gui/gui.h"
#include "encryption.h"

int main(int argc, char *argv[]) {
    if (!generate_and_save_key_iv()) {
        return 1;
    }
    gtk_init(&argc, &argv);
    gui_create_main_window();
    gtk_main();
    return 0;
}
