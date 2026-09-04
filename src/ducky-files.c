#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

static GtkListStore *store;
static GtkWidget *path_entry;
static char current_path[4096];

static void load_directory(const char *path) {
    GDir *directory;
    const char *name;

    directory = g_dir_open(path, 0, NULL);

    if (!directory) {
        return;
    }

    gtk_list_store_clear(store);

    while ((name = g_dir_read_name(directory))) {
        GtkTreeIter row;

        if (name[0] == '.') {
            continue;
        }

        gtk_list_store_append(store, &row);

        gtk_list_store_set(
            store,
            &row,
            0,
            name,
            -1
        );
    }

    g_dir_close(directory);
}

static void open_item(
    GtkTreeView *view,
    GtkTreePath *path,
    GtkTreeViewColumn *column
) {
    GtkTreeModel *model;
    GtkTreeIter row;
    char *name;
    char full_path[8192];

    (void)column;

    model = gtk_tree_view_get_model(view);

    if (!gtk_tree_model_get_iter(model, &row, path)) {
        return;
    }

    gtk_tree_model_get(model, &row, 0, &name, -1);

    snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s",
        current_path,
        name
    );

    if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
        strncpy(current_path, full_path, sizeof(current_path) - 1);
        gtk_entry_set_text(GTK_ENTRY(path_entry), current_path);
        load_directory(current_path);
    } else {
        char command[8192];

        snprintf(
            command,
            sizeof(command),
            "xdg-open '%s' >/dev/null 2>&1",
            full_path
        );

        system(command);
    }

    g_free(name);
}

static void path_entered(GtkEntry *entry) {
    const char *path = gtk_entry_get_text(entry);

    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        strncpy(current_path, path, sizeof(current_path) - 1);
        load_directory(current_path);
    }
}

static void activate(GtkApplication *app) {
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *scroll;
    GtkWidget *view;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    window = gtk_application_window_new(app);

    gtk_window_set_title(
        GTK_WINDOW(window),
        "Ducky Files"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        700,
        450
    );

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    path_entry = gtk_entry_new();

    strncpy(
        current_path,
        g_get_home_dir(),
        sizeof(current_path) - 1
    );

    gtk_entry_set_text(
        GTK_ENTRY(path_entry),
        current_path
    );

    store = gtk_list_store_new(1, G_TYPE_STRING);

    view = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(store)
    );

    renderer = gtk_cell_renderer_text_new();

    column = gtk_tree_view_column_new_with_attributes(
        "Name",
        renderer,
        "text",
        0,
        NULL
    );

    gtk_tree_view_append_column(
        GTK_TREE_VIEW(view),
        column
    );

    scroll = gtk_scrolled_window_new(NULL, NULL);

    gtk_container_add(
        GTK_CONTAINER(scroll),
        view
    );

    gtk_box_pack_start(
        GTK_BOX(box),
        path_entry,
        FALSE,
        FALSE,
        6
    );

    gtk_box_pack_start(
        GTK_BOX(box),
        scroll,
        TRUE,
        TRUE,
        0
    );

    gtk_container_add(
        GTK_CONTAINER(window),
        box
    );

    g_signal_connect(
        path_entry,
        "activate",
        G_CALLBACK(path_entered),
        NULL
    );

    g_signal_connect(
        view,
        "row-activated",
        G_CALLBACK(open_item),
        NULL
    );

    load_directory(current_path);
    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int result;

    app = gtk_application_new(
        "org.duckywm.files",
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
