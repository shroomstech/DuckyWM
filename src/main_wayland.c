/*
 * DuckyWM (Wayland / wlroots Version)
 * Lightweight Wayland compositor using wlroots 0.20
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

struct ducky_server {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    
    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener new_xdg_surface;

    struct wlr_seat *seat;
};

struct ducky_xdg_surface {
    struct ducky_server *server;
    struct wlr_xdg_surface *xdg_surface;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
};

static void ducky_xdg_surface_map(struct wl_listener *listener, void *data) {
    struct ducky_xdg_surface *surface = wl_container_of(listener, surface, map);
    (void)data;
    wlr_log(WLR_INFO, "DuckyWM Wayland: Window mapped/opened");
}

static void ducky_xdg_surface_destroy(struct wl_listener *listener, void *data) {
    struct ducky_xdg_surface *surface = wl_container_of(listener, surface, destroy);
    (void)data;
    wl_list_remove(&surface->map.link);
    wl_list_remove(&surface->destroy.link);
    free(surface);
}

static void ducky_new_xdg_surface(struct wl_listener *listener, void *data) {
    struct ducky_server *server = wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg_surface = data;

    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
        return;
    }

    struct ducky_xdg_surface *surface = calloc(1, sizeof(struct ducky_xdg_surface));
    surface->server = server;
    surface->xdg_surface = xdg_surface;

    surface->map.notify = ducky_xdg_surface_map;
    // In wlroots 0.20, map/unmap events are on the underlying wlr_surface
    wl_signal_add(&xdg_surface->surface->events.map, &surface->map);

    surface->destroy.notify = ducky_xdg_surface_destroy;
    wl_signal_add(&xdg_surface->events.destroy, &surface->destroy);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    wlr_log_init(WLR_DEBUG, NULL);

    struct ducky_server server;
    server.wl_display = wl_display_create();
    
    // Pass the display's event loop to wlr_backend_autocreate in wlroots 0.20
    struct wl_event_loop *event_loop = wl_display_get_event_loop(server.wl_display);
    server.backend = wlr_backend_autocreate(event_loop, NULL);
    if (!server.backend) {
        wlr_log(WLR_ERROR, "Failed to create wlr_backend");
        return 1;
    }

    server.renderer = wlr_renderer_autocreate(server.backend);
    wlr_renderer_init_wl_display(server.renderer, server.wl_display);

    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    
    wlr_compositor_create(server.wl_display, 5, server.renderer);
    wlr_subcompositor_create(server.wl_display);
    wlr_data_device_manager_create(server.wl_display);

    server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
    server.new_xdg_surface.notify = ducky_new_xdg_surface;
    wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

    const char *socket = wl_display_add_socket_auto(server.wl_display);
    if (!socket) {
        wlr_backend_destroy(server.backend);
        return 1;
    }

    if (!wlr_backend_start(server.backend)) {
        wlr_backend_destroy(server.backend);
        wl_display_destroy(server.wl_display);
        return 1;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    wlr_log(WLR_INFO, "DuckyWM Wayland running on WAYLAND_DISPLAY=%s", socket);

    wl_display_run(server.wl_display);

    wl_display_destroy_clients(server.wl_display);
    wl_display_destroy(server.wl_display);
    return 0;
}
