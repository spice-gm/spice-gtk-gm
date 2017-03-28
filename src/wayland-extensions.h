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
#pragma once

#include <gtk/gtk.h>

void spice_wayland_extensions_init(GtkWidget *widget);
int spice_wayland_extensions_enable_relative_pointer(GtkWidget *widget,
                                                     void (*cb)(void *,
                                                                struct zwp_relative_pointer_v1 *,
                                                                uint32_t, uint32_t,
                                                                wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t));
int spice_wayland_extensions_disable_relative_pointer(GtkWidget *widget);
int spice_wayland_extensions_lock_pointer(GtkWidget *widget,
                                          void (*lock_cb)(void *, struct zwp_locked_pointer_v1 *),
                                          void (*unlock_cb)(void *, struct zwp_locked_pointer_v1 *));
int spice_wayland_extensions_unlock_pointer(GtkWidget *widget);
