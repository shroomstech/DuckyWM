// DuckyWM made by duck.ai from duckduckgo

/*
 * DuckyWM
 * A small, lightweight X11 window manager.
 *
 * Keyboard shortcuts:
 *
 * Super + Enter  Launch terminal
 * Super + Q      Close focused window
 * Super + 1-9    Switch workspace
 * Super + H      Focus previous window
 * Super + L      Focus next window
 */

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MOD Mod4Mask
#define MAX_CLIENTS 256
#define MAX_WORKSPACES 9

typedef struct {
    int border_width;
    int gap_size;

    char background[32];
    char normal_border[32];
    char focused_border[32];

    char terminal_background[32];
    char terminal_foreground[32];
    char terminal_cursor[32];

    char terminal[128];
} Config;

typedef struct {
    Window window;
    int workspace;
} Client;

static Display *display;
static Window root;
static int screen;

static Config config;
static Client clients[MAX_CLIENTS];

static int client_count;
static int current_workspace = 1;
static int focused = -1;
static volatile sig_atomic_t reload_config = 0;

static unsigned long colour(const char *name) {
    XColor color;
    Colormap map = DefaultColormap(display, screen);

    if (!XParseColor(display, map, name, &color)) {
        return BlackPixel(display, screen);
    }

    if (!XAllocColor(display, map, &color)) {
        return BlackPixel(display, screen);
    }

    return color.pixel;
}

static void trim(char *text) {
    char *start = text;
    char *end;

    while (*start == ' ' || *start == '\t') {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    if (*text == '\0') {
        return;
    }

    end = text + strlen(text) - 1;

    while (end >= text &&
           (*end == ' ' || *end == '\t' ||
            *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
}

static void defaults(void) {
    config.border_width = 2;
    config.gap_size = 8;

    strcpy(config.background, "#073b3b");
    strcpy(config.normal_border, "#164e4e");
    strcpy(config.focused_border, "#20c4c4");

    strcpy(config.terminal_background, "#102828");
    strcpy(config.terminal_foreground, "#d8ffff");
    strcpy(config.terminal_cursor, "#20c4c4");

    strcpy(config.terminal, "xterm");
}

static void load_config(const char *path) {
    FILE *file = fopen(path, "r");
    char line[256];

    if (!file) {
        return;
    }

    while (fgets(line, sizeof(line), file)) {
        char *equals;
        char *key;
        char *value;

        trim(line);

        if (!line[0] || line[0] == '#') {
            continue;
        }

        equals = strchr(line, '=');

        if (!equals) {
            continue;
        }

        *equals = '\0';
        key = line;
        value = equals + 1;

        trim(key);
        trim(value);

        if (!strcmp(key, "border_width")) {
            config.border_width = atoi(value);
        } else if (!strcmp(key, "gap_size")) {
            config.gap_size = atoi(value);
        } else if (!strcmp(key, "background")) {
            strncpy(config.background, value, 31);
        } else if (!strcmp(key, "normal_border")) {
            strncpy(config.normal_border, value, 31);
        } else if (!strcmp(key, "focused_border")) {
            strncpy(config.focused_border, value, 31);
        } else if (!strcmp(key, "terminal_background")) {
            strncpy(config.terminal_background, value, 31);
        } else if (!strcmp(key, "terminal_foreground")) {
            strncpy(config.terminal_foreground, value, 31);
        } else if (!strcmp(key, "terminal_cursor")) {
            strncpy(config.terminal_cursor, value, 31);
        } else if (!strcmp(key, "terminal")) {
            strncpy(config.terminal, value, 127);
        }
    }

    fclose(file);
}

static void save_root_background(void) {
    XSetWindowBackground(
        display,
        root,
        colour(config.background)
    );

    XClearWindow(display, root);
}

static int find_client(Window window) {
    int i;

    for (i = 0; i < client_count; i++) {
        if (clients[i].window == window) {
            return i;
        }
    }

    return -1;
}

static void set_border(int index, unsigned long color) {
    XSetWindowBorderWidth(
        display,
        clients[index].window,
        config.border_width
    );

    XSetWindowBorder(
        display,
        clients[index].window,
        color
    );
}

static void update_borders(void) {
    int i;

    for (i = 0; i < client_count; i++) {
        if (i == focused &&
            clients[i].workspace == current_workspace) {
            set_border(i, colour(config.focused_border));
        } else {
            set_border(i, colour(config.normal_border));
        }
    }
}

static void focus_client(int index) {
    if (index < 0 || index >= client_count) {
        return;
    }

    if (clients[index].workspace != current_workspace) {
        return;
    }

    focused = index;

    XSetInputFocus(
        display,
        clients[index].window,
        RevertToPointerRoot,
        CurrentTime
    );

    XRaiseWindow(display, clients[index].window);
    update_borders();
}

static void arrange(void) {
    int visible[MAX_CLIENTS];
    int count = 0;
    int i;

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);

    for (i = 0; i < client_count; i++) {
        if (clients[i].workspace == current_workspace) {
            visible[count++] = i;
        }
    }

    for (i = 0; i < count; i++) {
        int index = visible[i];
        int x;
        int y;
        int w;
        int h;

        if (count == 1) {
            x = config.gap_size;
            y = config.gap_size;
            w = width - config.gap_size * 2;
            h = height - config.gap_size * 2;
        } else if (i == 0) {
            x = config.gap_size;
            y = config.gap_size;
            w = width / 2 - config.gap_size * 2;
            h = height - config.gap_size * 2;
        } else {
            int stack_height = height / (count - 1);

            x = width / 2 + config.gap_size / 2;
            y = (i - 1) * stack_height + config.gap_size;
            w = width / 2 - config.gap_size * 2;
            h = stack_height - config.gap_size * 2;
        }

        XMoveResizeWindow(
            display,
            clients[index].window,
            x,
            y,
            w,
            h
        );

        XMapWindow(display, clients[index].window);
    }

    if (count > 0) {
        if (focused < 0 ||
            clients[focused].workspace != current_workspace) {
            focus_client(visible[0]);
        }
    } else {
        focused = -1;
    }

    update_borders();
}

static void add_client(Window window) {
    if (find_client(window) >= 0 ||
        client_count >= MAX_CLIENTS) {
        return;
    }

    clients[client_count].window = window;
    clients[client_count].workspace = current_workspace;

    XSelectInput(
        display,
        window,
        EnterWindowMask | StructureNotifyMask
    );

    XMapWindow(display, window);
    client_count++;
}

static void remove_client(Window window) {
    int index = find_client(window);
    int i;

    if (index < 0) {
        return;
    }

    for (i = index; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }

    client_count--;

    if (focused == index) {
        focused = -1;
    } else if (focused > index) {
        focused--;
    }
}

static void launch_terminal(void) {
    char command[512];

    snprintf(
        command,
        sizeof(command),
        "%s -bg '%s' -fg '%s' -cr '%s'",
        config.terminal,
        config.terminal_background,
        config.terminal_foreground,
        config.terminal_cursor
    );

    if (fork() == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", command, NULL);
        _exit(1);
    }
}

static void launch_program(const char *program) {
    if (fork() == 0) {
        setsid();
        execl(program, program, NULL);
        _exit(1);
    }
}

static void close_window(void) {
    Window window;
    int revert;

    XGetInputFocus(display, &window, &revert);

    if (window == None || window == PointerRoot) {
        return;
    }

    XKillClient(display, window);
}

static void switch_workspace(int workspace) {
    int i;

    current_workspace = workspace;
    focused = -1;

    for (i = 0; i < client_count; i++) {
        if (clients[i].workspace == workspace) {
            XMapWindow(display, clients[i].window);
        } else {
            XUnmapWindow(display, clients[i].window);
        }
    }

    arrange();
}

static void focus_next(void) {
    int i;
    int start = focused < 0 ? 0 : focused + 1;

    for (i = 0; i < client_count; i++) {
        int index = (start + i) % client_count;

        if (clients[index].workspace == current_workspace) {
            focus_client(index);
            return;
        }
    }
}

static void focus_previous(void) {
    int i;
    int start = focused < 0 ? 0 : focused - 1;

    if (start < 0) {
        start = client_count - 1;
    }

    for (i = 0; i < client_count; i++) {
        int index = (start - i + client_count) % client_count;

        if (clients[index].workspace == current_workspace) {
            focus_client(index);
            return;
        }
    }
}

static void reload_handler(int signal_number) {
    (void)signal_number;
    reload_config = 1;
}

static void grab_keys(void) {
    int i;

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_Return),
        MOD,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_q),
        MOD,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_h),
        MOD,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_l),
        MOD,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_s),
        MOD,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    XGrabKey(
        display,
        XKeysymToKeycode(display, XK_f),
        MOD,
        root,
        True,
        GrabModeAsync,
        GrabModeAsync
    );

    for (i = 0; i < MAX_WORKSPACES; i++) {
        XGrabKey(
            display,
            XKeysymToKeycode(display, XK_1 + i),
            MOD,
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );
    }
}

static void keypress(XKeyEvent *event) {
    KeySym key = XLookupKeysym(event, 0);

    if (key == XK_Return) {
        launch_terminal();
    } else if (key == XK_q) {
        close_window();
    } else if (key == XK_h) {
        focus_previous();
    } else if (key == XK_l) {
        focus_next();
    } else if (key == XK_s) {
        launch_program("./ducky-settings");
    } else if (key == XK_f) {
        launch_program("./ducky-files");
    } else if (key >= XK_1 && key <= XK_9) {
        switch_workspace(key - XK_0);
    }
}

int main(int argc, char **argv) {
    const char *config_path;
    XEvent event;

    display = XOpenDisplay(NULL);

    if (!display) {
        fprintf(stderr, "DuckyWM: cannot open X display\n");
        return 1;
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    defaults();

    config_path =
        argc > 1 ? argv[1] : "config/duckywm.conf";

    load_config(config_path);
    save_root_background();

    signal(SIGHUP, reload_handler);

    XSelectInput(
        display,
        root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        KeyPressMask
    );

    grab_keys();
    XSync(display, False);

    for (;;) {
        if (reload_config) {
            reload_config = 0;
            defaults();
            load_config(config_path);
            save_root_background();
            arrange();
        }

        XNextEvent(display, &event);

        if (event.type == MapRequest) {
            add_client(event.xmaprequest.window);
            arrange();
        } else if (event.type == DestroyNotify) {
            remove_client(event.xdestroywindow.window);
            arrange();
        } else if (event.type == UnmapNotify) {
            remove_client(event.xunmap.window);
            arrange();
        } else if (event.type == ConfigureRequest) {
            XConfigureRequestEvent *request =
                &event.xconfigurerequest;

            XWindowChanges changes = {
                request->x,
                request->y,
                request->width,
                request->height,
                request->border_width,
                request->above,
                request->detail
            };

            XConfigureWindow(
                display,
                request->window,
                request->value_mask,
                &changes
            );
        } else if (event.type == KeyPress) {
            keypress(&event.xkey);
        } else if (event.type == EnterNotify) {
            int index = find_client(event.xcrossing.window);

            if (index >= 0) {
                focus_client(index);
            }
        }
    }

    return 0;
}
