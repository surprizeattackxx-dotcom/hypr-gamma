#include "gamma_window.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char *run_hyprctl(const char *cmd) {
    g_autoptr(GError) error = NULL;
    char              *out  = NULL;
    if (!g_spawn_command_line_sync(cmd, &out, NULL, NULL, &error)) {
        g_warning("hyprctl failed: %s", error ? error->message : "unknown");
        return NULL;
    }
    return out;
}

static void hyprctl_set(const char *monitor, const char *key, double value) {
    g_autofree char *cmd = g_strdup_printf("hyprctl hyprgamma:set %s %s %g", monitor, key, value);
    g_autofree char *out = run_hyprctl(cmd);
    (void)out;
}

static void hyprctl_enable(const char *monitor, bool enabled) {
    g_autofree char *cmd = g_strdup_printf("hyprctl hyprgamma:%s %s",
                                            enabled ? "enable" : "disable", monitor);
    g_autofree char *out = run_hyprctl(cmd);
    (void)out;
}

static void on_enable_switch(GtkSwitch *sw, gboolean, gpointer user_data) {
    MonitorControl *mc = (MonitorControl *)user_data;
    GammaAppState  *st = (GammaAppState *)g_object_get_data(G_OBJECT(mc->box), "state");
    if (st->updating) return;
    hyprctl_enable(mc->monitor_name, gtk_switch_get_active(sw));
}

static void on_slider_changed(GtkRange *range, gpointer user_data) {
    MonitorControl *mc = (MonitorControl *)user_data;
    GammaAppState  *st = (GammaAppState *)g_object_get_data(G_OBJECT(mc->box), "state");
    if (st->updating) return;

    const char *key = (const char *)g_object_get_data(G_OBJECT(range), "gamma_key");
    double      val = gtk_range_get_value(range);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", val);

    if (range == GTK_RANGE(mc->scale_brightness))
        gtk_label_set_text(GTK_LABEL(mc->label_brightness_val), buf);
    else if (range == GTK_RANGE(mc->scale_contrast))
        gtk_label_set_text(GTK_LABEL(mc->label_contrast_val), buf);
    else if (range == GTK_RANGE(mc->scale_gamma_r))
        gtk_label_set_text(GTK_LABEL(mc->label_gamma_r_val), buf);
    else if (range == GTK_RANGE(mc->scale_gamma_g))
        gtk_label_set_text(GTK_LABEL(mc->label_gamma_g_val), buf);
    else if (range == GTK_RANGE(mc->scale_gamma_b))
        gtk_label_set_text(GTK_LABEL(mc->label_gamma_b_val), buf);

    if (key)
        hyprctl_set(mc->monitor_name, key, val);
}

static void on_reset(GtkButton *, gpointer user_data) {
    MonitorControl *mc = (MonitorControl *)user_data;
    GammaAppState  *st = (GammaAppState *)g_object_get_data(G_OBJECT(mc->box), "state");

    st->updating = true;
    gtk_range_set_value(GTK_RANGE(mc->scale_brightness), 1.0);
    gtk_range_set_value(GTK_RANGE(mc->scale_contrast), 1.0);
    gtk_range_set_value(GTK_RANGE(mc->scale_gamma_r), 1.0);
    gtk_range_set_value(GTK_RANGE(mc->scale_gamma_g), 1.0);
    gtk_range_set_value(GTK_RANGE(mc->scale_gamma_b), 1.0);
    gtk_switch_set_active(GTK_SWITCH(mc->switch_enable), TRUE);
    st->updating = false;

    g_autofree char *cmd = g_strdup_printf("hyprctl hyprgamma:reset %s", mc->monitor_name);
    g_autofree char *out = run_hyprctl(cmd);
    (void)out;
}

void gamma_app_state_refresh(GammaAppState *state) {
    g_autofree char *json_str = run_hyprctl("hyprctl -j hyprgamma:list");
    if (!json_str)
        return;

    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError)     err    = NULL;
    if (!json_parser_load_from_data(parser, json_str, -1, &err)) {
        g_warning("JSON parse error: %s", err->message);
        return;
    }

    JsonNode   *root = json_parser_get_root(parser);
    JsonObject *obj  = json_node_get_object(root);
    JsonObject *mons = json_object_get_object_member(obj, "monitors");
    if (!mons) return;

    state->updating = true;

    for (guint i = 0; i < state->monitors->len; i++) {
        MonitorControl *mc = (MonitorControl *)g_ptr_array_index(state->monitors, i);
        if (!json_object_has_member(mons, mc->monitor_name))
            continue;

        JsonObject *mo = json_object_get_object_member(mons, mc->monitor_name);
        if (!mo) continue;

        gtk_switch_set_active(GTK_SWITCH(mc->switch_enable),
                              json_object_get_boolean_member_with_default(mo, "enabled", true));

        gtk_range_set_value(GTK_RANGE(mc->scale_brightness),
                            json_object_get_double_member_with_default(mo, "brightness", 1.0));
        gtk_range_set_value(GTK_RANGE(mc->scale_contrast),
                            json_object_get_double_member_with_default(mo, "contrast", 1.0));
        gtk_range_set_value(GTK_RANGE(mc->scale_gamma_r),
                            json_object_get_double_member_with_default(mo, "gamma_r", 1.0));
        gtk_range_set_value(GTK_RANGE(mc->scale_gamma_g),
                            json_object_get_double_member_with_default(mo, "gamma_g", 1.0));
        gtk_range_set_value(GTK_RANGE(mc->scale_gamma_b),
                            json_object_get_double_member_with_default(mo, "gamma_b", 1.0));
    }

    state->updating = false;
}

static gboolean toast_hide_cb(gpointer user_data) {
    GammaAppState *state = (GammaAppState *)user_data;
    gtk_widget_set_opacity(state->toast_label, 0.0);
    state->toast_timer = 0;
    return G_SOURCE_REMOVE;
}

void gamma_app_state_show_toast(GammaAppState *state, const char *msg) {
    gtk_label_set_text(GTK_LABEL(state->toast_label), msg);
    gtk_widget_set_opacity(state->toast_label, 1.0);
    if (state->toast_timer)
        g_source_remove(state->toast_timer);
    state->toast_timer = g_timeout_add_seconds(3, toast_hide_cb, state);
}

static void add_monitor_control(GammaAppState *state, const char *name) {
    MonitorControl *mc = g_new0(MonitorControl, 1);
    g_strlcpy(mc->monitor_name, name, sizeof(mc->monitor_name));

    mc->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(mc->box, 12);
    gtk_widget_set_margin_end(mc->box, 12);
    gtk_widget_set_margin_top(mc->box, 8);
    gtk_widget_set_margin_bottom(mc->box, 8);
    g_object_set_data(G_OBJECT(mc->box), "state", state);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label  = gtk_label_new(NULL);
    g_autofree char *header_text = g_strdup_printf("<b>%s</b>", name);
    gtk_label_set_markup(GTK_LABEL(label), header_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(header), label);

    mc->switch_enable = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(mc->switch_enable), TRUE);
    g_signal_connect(mc->switch_enable, "state-set", G_CALLBACK(on_enable_switch), mc);
    gtk_box_append(GTK_BOX(header), mc->switch_enable);
    gtk_box_append(GTK_BOX(mc->box), header);

    auto add_slider = [&](const char *label_text, const char *key,
                          double min, double max, double step,
                          GtkWidget **scale_out, GtkWidget **val_out) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *lbl = gtk_label_new(label_text);
        gtk_widget_set_size_request(lbl, 90, -1);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row), lbl);

        GtkWidget *sc = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
        gtk_widget_set_hexpand(sc, TRUE);
        gtk_scale_set_draw_value(GTK_SCALE(sc), FALSE);
        g_object_set_data(G_OBJECT(sc), "gamma_key", (gpointer)key);
        g_signal_connect(sc, "value-changed", G_CALLBACK(on_slider_changed), mc);
        gtk_box_append(GTK_BOX(row), sc);

        GtkWidget *vl = gtk_label_new("1.00");
        gtk_widget_set_size_request(vl, 40, -1);
        gtk_box_append(GTK_BOX(row), vl);
        gtk_box_append(GTK_BOX(mc->box), row);

        *scale_out = sc;
        *val_out   = vl;
    };

    add_slider("Brightness", "brightness", 0.0, 2.0, 0.01,
               &mc->scale_brightness, &mc->label_brightness_val);
    add_slider("Contrast", "contrast", 0.0, 2.0, 0.01,
               &mc->scale_contrast, &mc->label_contrast_val);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(mc->box), sep);

    add_slider("Gamma R", "gamma_r", 0.1, 5.0, 0.01,
               &mc->scale_gamma_r, &mc->label_gamma_r_val);
    add_slider("Gamma G", "gamma_g", 0.1, 5.0, 0.01,
               &mc->scale_gamma_g, &mc->label_gamma_g_val);
    add_slider("Gamma B", "gamma_b", 0.1, 5.0, 0.01,
               &mc->scale_gamma_b, &mc->label_gamma_b_val);

    GtkWidget *btn = gtk_button_new_with_label("Reset Monitor");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_reset), mc);
    gtk_box_append(GTK_BOX(mc->box), btn);

    GtkWidget *big_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(mc->box), big_sep);

    gtk_box_append(GTK_BOX(state->monitor_list), mc->box);
    g_ptr_array_add(state->monitors, mc);
}

void build_window(GammaAppState *state) {
    state->window = gtk_application_window_new(GTK_APPLICATION(
        g_application_get_default()));

    gtk_window_set_title(GTK_WINDOW(state->window), "HyprGamma — Per-Monitor Controls");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 480, 600);

    state->overlay = gtk_overlay_new();
    gtk_window_set_child(GTK_WINDOW(state->window), state->overlay);

    state->monitor_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    state->scroll       = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(state->scroll), state->monitor_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(state->scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_overlay_set_child(GTK_OVERLAY(state->overlay), state->scroll);

    state->toast_label = gtk_label_new("");
    gtk_widget_set_opacity(state->toast_label, 0.0);
    gtk_widget_set_halign(state->toast_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(state->toast_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(state->toast_label, 16);
    gtk_widget_add_css_class(state->toast_label, "toast");
    gtk_overlay_add_overlay(GTK_OVERLAY(state->overlay), state->toast_label);

    {
        g_autofree char *json_str = run_hyprctl("hyprctl -j hyprgamma:list");
        if (json_str) {
            g_autoptr(JsonParser) parser = json_parser_new();
            g_autoptr(GError)     err    = NULL;
            if (json_parser_load_from_data(parser, json_str, -1, &err)) {
                JsonNode   *root = json_parser_get_root(parser);
                JsonObject *obj  = json_node_get_object(root);
                JsonObject *mons = json_object_get_object_member(obj, "monitors");
                if (mons) {
                    JsonObjectIter iter;
                    const char    *member_name;
                    JsonNode      *member_node;
                    json_object_iter_init(&iter, mons);
                    while (json_object_iter_next(&iter, &member_name, &member_node))
                        add_monitor_control(state, member_name);
                }
            }
        }
    }

    if (state->monitors->len == 0) {
        GtkWidget *placeholder = gtk_label_new("No monitors detected.\nMake sure hyprgamma plugin is loaded.");
        gtk_widget_set_margin_top(placeholder, 40);
        gtk_box_append(GTK_BOX(state->monitor_list), placeholder);
    }

    gamma_app_state_refresh(state);
}

void on_activate(GtkApplication *app, gpointer user_data) {
    GammaAppState *state = (GammaAppState *)user_data;
    build_window(state);
    gtk_window_present(GTK_WINDOW(state->window));
}

GammaAppState *gamma_app_state_new(void) {
    GammaAppState *st       = g_new0(GammaAppState, 1);
    st->monitors            = g_ptr_array_new_with_free_func(g_free);
    st->updating            = false;
    st->toast_timer         = 0;
    return st;
}

void gamma_app_state_free(GammaAppState *state) {
    if (!state) return;
    if (state->toast_timer)
        g_source_remove(state->toast_timer);
    if (state->monitors)
        g_ptr_array_unref(state->monitors);
    g_free(state);
}
