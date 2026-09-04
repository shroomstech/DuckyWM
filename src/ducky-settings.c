#include <gtk/gtk.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_PATH "config/duckywm.conf"

static GtkWidget *background_entry;
static GtkWidget *normal_entry;
static GtkWidget *focused_entry;
static GtkWidget *terminal_bg_entry;
static GtkWidget *terminal_fg_entry;
static GtkWidget *cursor_entry;
static GtkWidget *gap_entry;
static GtkWidget *border_entry;

static void save_settings(GtkWidget *button, gpointer data) {
    FILE *file;

    (void)button;
    (void)data;

    file = fopen(CONFIG_PATH, "w");

    if (!file) {
        return;
    }

    fprintf(file, "# DuckyWM configuration\n\n");

    fprintf(
        file,
        "border_width = %s\n",
        gtk_entry_get_text(GTK_ENTRY(border_entry))
    );

    fprintf(
        file,
        "gap_size = %s\n\n",
        gtk_entry_get_text(GTK_ENTRY(gap_entry))
    );

    fprintf(
        file,
        "background = %s\n",
        gtk_entry_get_text(GTK_ENTRY(background_entry))
    );

    fprintf(
        file,
        "normal_border = %s\n",
        gtk_entry_get_text(GTK_ENTRY(normal_entry))
    );

    fprintf(
        file,
        "focused_border = %s\n\n",
        gtk_entry_get_text(GTK_ENTRY(focused_entry))
    );

    fprintf(
        file,
        "terminal_background = %s\n",
        gtk_entry_get_text(GTK_ENTRY(terminal_bg_entry))
    );

    fprintf(
        file,
        "terminal_foreground = %s\n",
        gtk_entry_get_text(GTK_ENTRY(terminal_fg_entry))
    );

    fprintf(
        file,
        "terminal_cursor = %s\n",
        gtk_entry_get_text(GTK_ENTRY(cursor_entry))
    );

    fprintf(file, "terminal = xterm\n");

    fclose(file);

    system("pkill -HUP duckywm 2>/dev/null");

    gtk_main_quit();
}

static GtkWidget *setting(
    GtkWidget *box,
    const char *label,
    const char *value
) {
    GtkWidget *row;
    GtkWidget *text;
    GtkWidget *entry;

    row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    text = gtk_label_new(label);
    entry = gtk_entry_new();

    gtk_entry_set_text(GTK_ENTRY(entry), value);
    gtk_widget_set_size_request(entry, 180, -1);

    gtk_box_pack_start(
        GTK_BOX(row),
        text,
        FALSE,
        FALSE,
        4
    );

    gtk_box_pack_end(
        GTK_BOX(row),
        entry,
        FALSE,
        FALSE,
        4
    );

    gtk_box_pack_start(
        GTK_BOX(box),
        row,
        FALSE,
        FALSE,
        4
    );

    return entry;
}

static void activate(GtkApplication *app) {
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *save_button;

    window = gtk_application_window_new(app);

    gtk_window_set_title(
        GTK_WINDOW(window),
        "DuckyWM Settings"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        420,
        430
    );

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 14);

    background_entry =
        setting(box, "Desktop background", "#073b3b");

    normal_entry =
        setting(box, "Normal border", "#164e4e");

    focused_entry =
        setting(box, "Focused border", "#20c4c4");

    terminal_bg_entry =
        setting(box, "Terminal background", "#102828");

    terminal_fg_entry =
        setting(box, "Terminal text", "#d8ffff");

    cursor_entry =
        setting(box, "Terminal cursor", "#20c4c4");

    gap_entry =
        setting(box, "Window gap", "8");

    border_entry =
        setting(box, "Border width", "2");

    save_button = gtk_button_new_with_label(
        "Save and reload DuckyWM"
    );

    gtk_box_pack_end(
        GTK_BOX(box),
        save_button,
        FALSE,
        FALSE,
        8
    );

    gtk_container_add(
        GTK_CONTAINER(window),
        box
    );

    g_signal_connect(
        save_button,
        "clicked",
        G_CALLBACK(save_settings),
        NULL
    );

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int result;

    app = gtk_application_new(
        "org.duckywm.settings",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(activate),
        NULL
    );

    result = g_application_run(
        G_APPLICATION(app),
        argc,
        argv
    );

    g_object_unref(app);
    return result;
}
