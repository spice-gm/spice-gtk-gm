/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2017 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include <stdint.h>
#include <string.h>

#include <gtk/gtk.h>

#include <gdk/gdkwayland.h>
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"

#include "wayland-extensions.h"

static void *
gtk_wl_registry_bind(GtkWidget *widget,
                     uint32_t name,
                     const struct wl_interface *interface,
                     uint32_t version)
{
    GdkDisplay *gdk_display = gtk_widget_get_display(widget);
    struct wl_display *display;
    struct wl_registry *registry;

    if (!GDK_IS_WAYLAND_DISPLAY(gdk_display)) {
        return NULL;
    }

    display = gdk_wayland_display_get_wl_display(gdk_display);
    registry = wl_display_get_registry(display);

    return wl_registry_bind(registry, name, interface, version);
}

static void
gtk_wl_registry_add_listener(GtkWidget *widget, const struct wl_registry_listener *listener)
{
    GdkDisplay *gdk_display = gtk_widget_get_display(widget);
    struct wl_display *display;
    struct wl_registry *registry;

    if (!GDK_IS_WAYLAND_DISPLAY(gdk_display)) {
        return;
    }

    display = gdk_wayland_display_get_wl_display(gdk_display);
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, listener, widget);
    wl_display_roundtrip(display);
}


static void
registry_handle_global(void *data,
                       struct wl_registry *registry,
                       uint32_t name,
                       const char *interface,
                       uint32_t version)
{
    GtkWidget *widget = GTK_WIDGET(data);

    if (strcmp(interface, "zwp_relative_pointer_manager_v1") == 0) {
        struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
        relative_pointer_manager = gtk_wl_registry_bind(widget, name,
                                                        &zwp_relative_pointer_manager_v1_interface,
                                                        1);
        g_object_set_data_full(G_OBJECT(widget),
                               "zwp_relative_pointer_manager_v1",
                               relative_pointer_manager,
                               (GDestroyNotify)zwp_relative_pointer_manager_v1_destroy);
    } else if (strcmp(interface, "zwp_pointer_constraints_v1") == 0) {
        struct zwp_pointer_constraints_v1 *pointer_constraints;
        pointer_constraints = gtk_wl_registry_bind(widget, name,
                                                   &zwp_pointer_constraints_v1_interface,
                                                   1);
        g_object_set_data_full(G_OBJECT(widget),
                               "zwp_pointer_constraints_v1",
                               pointer_constraints,
                               (GDestroyNotify)zwp_pointer_constraints_v1_destroy);
    }
}

static void
registry_handle_global_remove(void *data,
                              struct wl_registry *registry,
                              uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

void
spice_wayland_extensions_init(GtkWidget *widget)
{
    g_return_if_fail(GTK_IS_WIDGET(widget));

    gtk_wl_registry_add_listener(widget, &registry_listener);
}


static GdkDevice *
spice_gdk_window_get_pointing_device(GdkWindow *window)
{
    GdkDisplay *gdk_display = gdk_window_get_display(window);

    return gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display));
}

static struct zwp_relative_pointer_v1_listener relative_pointer_listener;

// NOTE this API works only on a single widget per application
int
spice_wayland_extensions_enable_relative_pointer(GtkWidget *widget,
                                                 void (*cb)(void *,
                                                            struct zwp_relative_pointer_v1 *,
                                                            uint32_t, uint32_t,
                                                            wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t))
{
    struct zwp_relative_pointer_v1 *relative_pointer;

    g_return_val_if_fail(GTK_IS_WIDGET(widget), -1);

    relative_pointer = g_object_get_data(G_OBJECT(widget), "zwp_relative_pointer_v1");

    if (relative_pointer == NULL) {
        struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
        GdkWindow *window = gtk_widget_get_window(widget);
        struct wl_pointer *pointer;

        relative_pointer_manager = g_object_get_data(G_OBJECT(widget), "zwp_relative_pointer_manager_v1");
        if (relative_pointer_manager == NULL)
            return -1;

        pointer = gdk_wayland_device_get_wl_pointer(spice_gdk_window_get_pointing_device(window));
        relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(relative_pointer_manager,
                                                                                pointer);

        relative_pointer_listener.relative_motion = cb;
        zwp_relative_pointer_v1_add_listener(relative_pointer,
                                             &relative_pointer_listener,
                                             widget);

        g_object_set_data_full(G_OBJECT(widget),
                               "zwp_relative_pointer_v1",
                               relative_pointer,
                               (GDestroyNotify)zwp_relative_pointer_v1_destroy);
    }

    return 0;
}

int spice_wayland_extensions_disable_relative_pointer(GtkWidget *widget)
{
    g_return_val_if_fail(GTK_IS_WIDGET(widget), -1);

    /* This will call zwp_relative_pointer_v1_destroy() and stop relative
     * movement */
    g_object_set_data(G_OBJECT(widget), "zwp_relative_pointer_v1", NULL);

    return 0;
}

static struct zwp_locked_pointer_v1_listener locked_pointer_listener;

// NOTE this API works only on a single widget per application
int
spice_wayland_extensions_lock_pointer(GtkWidget *widget,
                                      void (*lock_cb)(void *, struct zwp_locked_pointer_v1 *),
                                      void (*unlock_cb)(void *, struct zwp_locked_pointer_v1 *))
{
    struct zwp_pointer_constraints_v1 *pointer_constraints;
    struct zwp_locked_pointer_v1 *locked_pointer;
    GdkWindow *window;
    struct wl_pointer *pointer;

    g_return_val_if_fail(GTK_IS_WIDGET(widget), -1);

    pointer_constraints = g_object_get_data(G_OBJECT(widget), "zwp_pointer_constraints_v1");
    locked_pointer = g_object_get_data(G_OBJECT(widget), "zwp_locked_pointer_v1");
    if (locked_pointer != NULL) {
        /* A previous lock already in place */
        return 0;
    }

    window = gtk_widget_get_window(widget);
    pointer = gdk_wayland_device_get_wl_pointer(spice_gdk_window_get_pointing_device(window));
    locked_pointer = zwp_pointer_constraints_v1_lock_pointer(pointer_constraints,
                                                             gdk_wayland_window_get_wl_surface(window),
                                                             pointer,
                                                             NULL,
                                                             ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    if (lock_cb || unlock_cb) {
        locked_pointer_listener.locked = lock_cb;
        locked_pointer_listener.unlocked = unlock_cb;
        zwp_locked_pointer_v1_add_listener(locked_pointer,
                                           &locked_pointer_listener,
                                           widget);
    }
    g_object_set_data_full(G_OBJECT(widget),
                           "zwp_locked_pointer_v1",
                           locked_pointer,
                           (GDestroyNotify)zwp_locked_pointer_v1_destroy);

    return 0;
}

int
spice_wayland_extensions_unlock_pointer(GtkWidget *widget)
{
    g_return_val_if_fail(GTK_IS_WIDGET(widget), -1);

    g_object_set_data(G_OBJECT(widget), "zwp_locked_pointer_v1", NULL);

    return 0;
}
