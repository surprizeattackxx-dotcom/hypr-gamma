#pragma once

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <stdbool.h>

#define APP_ID "io.github.donnie.hyprgamma"

typedef struct {
    char      monitor_name[64];
    GtkWidget *box;
    GtkWidget *switch_enable;
    GtkWidget *scale_brightness;
    GtkWidget *scale_contrast;
    GtkWidget *scale_gamma_r;
    GtkWidget *scale_gamma_g;
    GtkWidget *scale_gamma_b;
    GtkWidget *label_brightness_val;
    GtkWidget *label_contrast_val;
    GtkWidget *label_gamma_r_val;
    GtkWidget *label_gamma_g_val;
    GtkWidget *label_gamma_b_val;
} MonitorControl;

typedef struct {
    GtkWidget *window;
    GtkWidget *overlay;
    GtkWidget *toast_label;
    guint      toast_timer;
    GtkWidget *scroll;
    GtkWidget *monitor_list;
    GPtrArray *monitors; // MonitorControl*
    bool       updating; // suppress signal re-entry
} GammaAppState;

GammaAppState *gamma_app_state_new(void);
void           gamma_app_state_free(GammaAppState *state);
void           gamma_app_state_refresh(GammaAppState *state);
void           gamma_app_state_show_toast(GammaAppState *state, const char *msg);
void           on_activate(GtkApplication *app, gpointer user_data);
void           build_window(GammaAppState *state);
