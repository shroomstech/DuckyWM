/*
 * DuckyWM
 * Small X11 window manager with workspaces and a top bar.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MOD Mod4Mask
#define MAX_CLIENTS 256
#define MAX_WORKSPACES 9
#define BAR_HEIGHT 28

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
static Window bar;
static GC bar_gc;
static XFontStruct *bar_font;

static int screen;
static Config config;
static Client clients[MAX_CLIENTS];

static int client_count = 0;
static int current_workspace = 1;
static int focused = -1;

static unsigned int numlock_mask = 0;
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

static void copy_string(
    char *destination,
    size_t destination_size,
    const char *source
) {
    if (destination_size > 0) {
        snprintf(destination, destination_size, "%s", source);
    }
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

    if (!*text) {
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

    copy_string(config.background, sizeof(config.background), "#073b3b");
    copy_string(config.normal_border,
                sizeof(config.normal_border), "#164e4e");
    copy_string(config.focused_border,
                sizeof(config.focused_border), "#20c4c4");

    copy_string(config.terminal_background,
                sizeof(config.terminal_background), "#102828");
    copy_string(config.terminal_foreground,
                sizeof(config.terminal_foreground), "#d8ffff");
    copy_string(config.terminal_cursor,
                sizeof(config.terminal_cursor), "#20c4c4");

    copy_string(config.terminal,
                sizeof(config.terminal), "xterm");
}

static void load_config(const char *path) {
    FILE *file;
    char line[256];

    file = fopen(path, "r");

    if (!file) {
        fprintf(stderr, "DuckyWM: using defaults; cannot read %s\n", path);
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
            if (config.border_width < 0) {
                config.border_width = 0;
            }
        } else if (!strcmp(key, "gap_size")) {
            config.gap_size = atoi(value);
            if (config.gap_size < 0) {
                config.gap_size = 0;
            }
        } else if (!strcmp(key, "background")) {
            copy_string(config.background,
                        sizeof(config.background), value);
        } else if (!strcmp(key, "normal_border")) {
            copy_string(config.normal_border,
                        sizeof(config.normal_border), value);
        } else if (!strcmp(key, "focused_border")) {
            copy_string(config.focused_border,
                        sizeof(config.focused_border), value);
        } else if (!strcmp(key, "terminal_background")) {
            copy_string(config.terminal_background,
                        sizeof(config.terminal_background), value);
        } else if (!strcmp(key, "terminal_foreground")) {
            copy_string(config.terminal_foreground,
                        sizeof(config.terminal_foreground), value);
        } else if (!strcmp(key, "terminal_cursor")) {
            copy_string(config.terminal_cursor,
                        sizeof(config.terminal_cursor), value);
        } else if (!strcmp(key, "terminal")) {
            copy_string(config.terminal,
                        sizeof(config.terminal), value);
        }
    }

    fclose(file);
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

static const char *window_title(void) {
    static char title[256];
    XTextProperty property;

    title[0] = '\0';

    if (focused < 0 || focused >= client_count) {
        return "Desktop";
    }

    if (!XGetWMName(
            display,
            clients[focused].window,
            &property
        )) {
        return "Untitled";
    }

    if (!property.value) {
        return "Untitled";
    }

    copy_string(title, sizeof(title), (char *)property.value);
    XFree(property.value);

    return title[0] ? title : "Untitled";
}

static void draw_bar(void) {
    char text[512];
    int width = DisplayWidth(display, screen);

    XSetForeground(display, bar_gc, colour("#102828"));
    XFillRectangle(display, bar, bar_gc, 0, 0, width, BAR_HEIGHT);

    snprintf(
        text,
        sizeof(text),
        "DuckyWM   Workspace %d/%d   |   %s",
        current_workspace,
        MAX_WORKSPACES,
        window_title()
    );

    XSetForeground(display, bar_gc, colour("#d8ffff"));

    XDrawString(
        display,
        bar,
        bar_gc,
        10,
        19,
        text,
        strlen(text)
    );

    XFlush(display);
}

static void create_bar(void) {
    XSetWindowAttributes attributes;
    XGCValues values;
    int width = DisplayWidth(display, screen);

    attributes.override_redirect = True;
    attributes.background_pixel = colour("#102828");
    attributes.event_mask = ExposureMask;

    bar = XCreateWindow(
        display,
        root,
        0,
        0,
        width,
        BAR_HEIGHT,
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWOverrideRedirect |
        CWBackPixel |
        CWEventMask,
        &attributes
    );

    values.foreground = colour("#d8ffff");

    bar_gc = XCreateGC(
        display,
        bar,
        GCForeground,
        &values
    );

    bar_font = XLoadQueryFont(display, "fixed");

    if (bar_font) {
        XSetFont(display, bar_gc, bar_font->fid);
    }

    XMapRaised(display, bar);
    draw_bar();
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
    draw_bar();
}

static void arrange(void) {
    int visible[MAX_CLIENTS];
    int count = 0;
    int i;

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen) - BAR_HEIGHT;

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
            y = BAR_HEIGHT + config.gap_size;
            w = width - config.gap_size * 2;
            h = height - config.gap_size * 2;
        } else if (i == 0) {
            x = config.gap_size;
            y = BAR_HEIGHT + config.gap_size;
            w = width / 2 - config.gap_size * 2;
            h = height - config.gap_size * 2;
        } else {
            int stack_height = height / (count - 1);

            x = width / 2 + config.gap_size / 2;
            y = BAR_HEIGHT +
                (i - 1) * stack_height +
                config.gap_size;
            w = width / 2 - config.gap_size * 2;
            h = stack_height - config.gap_size * 2;
        }

        if (w < 1) {
            w = 1;
        }

        if (h < 1) {
            h = 1;
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
            focused >= client_count ||
            clients[focused].workspace != current_workspace) {
            focused = -1;
            focus_client(visible[0]);
        }
    } else {
        focused = -1;
    }

    update_borders();
    draw_bar();
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
        EnterWindowMask |
        StructureNotifyMask |
        PropertyChangeMask
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
    if (fork() == 0) {
        setsid();
        execlp(config.terminal, config.terminal, NULL);
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

    if (window != None && window != PointerRoot) {
        XKillClient(display, window);
    }
}

static void switch_workspace(int workspace) {
    int i;

    if (workspace < 1 || workspace > MAX_WORKSPACES) {
        return;
    }

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
    int start;

    if (client_count == 0) {
        return;
    }

    start = focused < 0 ? 0 : focused + 1;

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
    int start;

    if (client_count == 0) {
        return;
    }

    start = focused < 0 ? client_count - 1 : focused - 1;

    for (i = 0; i < client_count; i++) {
        int index =
            (start - i + client_count) % client_count;

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

static void detect_numlock(void) {
    XModifierKeymap *modmap;
    KeyCode numlock;
    int i;
    int j;

    numlock = XKeysymToKeycode(display, XK_Num_Lock);
    modmap = XGetModifierMapping(display);

    if (!modmap) {
        return;
    }

    for (i = 0; i < 8; i++) {
        for (j = 0; j < modmap->max_keypermod; j++) {
            KeyCode keycode =
                modmap->modifiermap[
                    i * modmap->max_keypermod + j
                ];

            if (keycode == numlock) {
                numlock_mask = 1 << i;
            }
        }
    }

    XFreeModifiermap(modmap);
}

static void grab_key(KeySym keysym) {
    KeyCode keycode;
    unsigned int modifiers[4];
    int i;

    keycode = XKeysymToKeycode(display, keysym);

    if (keycode == NoSymbol) {
        return;
    }

    modifiers[0] = MOD;
    modifiers[1] = MOD | LockMask;
    modifiers[2] = MOD | numlock_mask;
    modifiers[3] = MOD | LockMask | numlock_mask;

    for (i = 0; i < 4; i++) {
        XGrabKey(
            display,
            keycode,
            modifiers[i],
            root,
            True,
            GrabModeAsync,
            GrabModeAsync
        );
    }
}

static void grab_keys(void) {
    int i;

    detect_numlock();

    grab_key(XK_Return);
    grab_key(XK_q);
    grab_key(XK_h);
    grab_key(XK_l);
    grab_key(XK_s);
    grab_key(XK_f);

    for (i = 0; i < MAX_WORKSPACES; i++) {
        grab_key(XK_1 + i);
    }

    XSync(display, False);
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
        switch_workspace((int)(key - XK_0));
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

    XSetWindowBackground(
        display,
        root,
        colour(config.background)
    );

    XClearWindow(display, root);

    signal(SIGHUP, reload_handler);

    XSelectInput(
        display,
        root,
        SubstructureRedirectMask |
        SubstructureNotifyMask |
        KeyPressMask
    );

    create_bar();
    grab_keys();

    for (;;) {
        XNextEvent(display, &event);

        if (reload_config) {
            reload_config = 0;
            defaults();
            load_config(config_path);
            XSetWindowBackground(
                display,
                root,
                colour(config.background)
            );
            XClearWindow(display, root);
            draw_bar();
            arrange();
        }

        if (event.type == Expose &&
            event.xexpose.window == bar) {
            draw_bar();

        } else if (event.type == MapRequest) {
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
            int index =
                find_client(event.xcrossing.window);

            if (index >= 0) {
                focus_client(index);
            }

        } else if (event.type == PropertyNotify) {
            int index =
                find_client(event.xproperty.window);

            if (index >= 0 &&
                event.xproperty.atom == XA_WM_NAME) {
                draw_bar();
            }
        }
    }

    return 0;
}
