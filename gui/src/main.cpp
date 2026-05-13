#include "gamma_window.h"
#include <gtk/gtk.h>

int main(int argc, char *argv[]) {
    GammaAppState *state = gamma_app_state_new();

    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), state);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    gamma_app_state_free(state);
    g_object_unref(app);

    return status;
}
