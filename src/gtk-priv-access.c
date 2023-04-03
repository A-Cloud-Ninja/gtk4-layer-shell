/* This entire file is licensed under LGPL v3
 *
 * Copyright 2020 Sophie Winter
 *
 * This file is part of GTK Layer Shell.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "gtk-priv-access.h"
#include "gtk-wayland.h"
#include "xdg-popup-surface.h"

#include "wayland-client.h"

typedef enum _PositionMethod
{
  POSITION_METHOD_ENUM
} PositionMethod;
typedef enum _PopupState
{
  POPUP_STATE_IDLE,
  POPUP_STATE_WAITING_FOR_REPOSITIONED,
  POPUP_STATE_WAITING_FOR_CONFIGURE,
  POPUP_STATE_WAITING_FOR_FRAME,
} PopupState;
typedef enum
{
  GDK_HINT_MIN_SIZE    = 1 << 1,
  GDK_HINT_MAX_SIZE    = 1 << 2,
} GdkSurfaceHints;
typedef void (*GdkWaylandToplevelExported) (GdkToplevel *toplevel,
                                            const char  *handle,
                                            gpointer     user_data);
typedef void *EGLSurface;
typedef void *GdkWaylandWindowExported;
typedef void *GdkWaylandTabletToolData;
typedef void *GdkKeymap;

#include "gdk_geometry_priv.h"
#include "gdk_surface_class_priv.h"
#include "gdk_surface_priv.h"
#include "gdk_wayland_surface_priv.h"
#include "gdk_wayland_surface_class_priv.h"
#include "gdk_wayland_pointer_frame_data_priv.h"
#include "gdk_wayland_pointer_data_priv.h"
#include "gdk_wayland_seat_priv.h"
#include "gdk_wayland_touch_data_priv.h"
#include "gdk_wayland_tablet_data_priv.h"

#include <glib-2.0/glib.h>

// The type of the function pointer of GdkWindowImpl's move_to_rect method (gdkwindowimpl.h:78)'
typedef void (*MoveToRectFunc) (GdkWindow *window,
                                const GdkRectangle *rect,
                                GdkGravity rect_anchor,
                                GdkGravity window_anchor,
                                GdkAnchorHints anchor_hints,
                                int rect_anchor_dx,
                                int rect_anchor_dy);

static MoveToRectFunc gdk_window_move_to_rect_real = NULL;

static GdkWindow *
gdk_window_get_priv_transient_for (GdkWindow *gdk_window)
{
    GdkWindow *window_transient_for = gdk_surface_priv_get_transient_for (gdk_window);
    // TODO
    //GdkWaylandSurface *window_impl = (GdkWaylandSurface *) gdk_window;
    //GdkWindow *wayland_transient_for = gdk_wayland_surface_priv_get_transient_for (window_impl);
    //if (wayland_transient_for)
    //    return wayland_transient_for;
    //else
        return window_transient_for;
}

uint32_t
gdk_window_get_priv_latest_serial (GdkSeat *seat)
{
    uint32_t serial = 0;
    GdkWaylandSeat *wayland_seat = (GdkWaylandSeat *)seat;

    serial = MAX(serial, gdk_wayland_seat_priv_get_keyboard_key_serial (wayland_seat));

    GdkWaylandPointerData* pointer_data = gdk_wayland_seat_priv_get_pointer_info_ptr (wayland_seat);
    serial = MAX(serial, gdk_wayland_pointer_data_priv_get_press_serial (pointer_data));

    GHashTableIter i;
    GdkWaylandTouchData *touch;
    g_hash_table_iter_init (&i, gdk_wayland_seat_priv_get_touches (wayland_seat));
    while (g_hash_table_iter_next (&i, NULL, (gpointer *)&touch))
        serial = MAX(serial, gdk_wayland_touch_data_priv_get_touch_down_serial (touch));

    for (GList *l = gdk_wayland_seat_priv_get_tablets (wayland_seat); l; l = l->next) {
        GdkWaylandTabletData *tablet_data = l->data;
        GdkWaylandPointerData *pointer_data = gdk_wayland_tablet_data_priv_get_pointer_info_ptr (tablet_data);
        serial = MAX(serial, gdk_wayland_pointer_data_priv_get_press_serial (pointer_data));
    }

    return serial;
}

static GdkSeat *
gdk_window_get_priv_grab_seat_for_single_window (GdkWindow *gdk_window)
{
    if (!gdk_window)
        return NULL;

    GdkWaylandSurface *wayland_surface = (GdkWaylandSurface *)gdk_window;
    return gdk_wayland_surface_priv_get_grab_input_seat (wayland_surface);
}

GdkSeat *
gdk_window_get_priv_grab_seat (GdkWindow *gdk_window)
{
    GdkSeat *seat = NULL;

    seat = gdk_window_get_priv_grab_seat_for_single_window (gdk_window);
    if (seat)
        return seat;

    // see the comment in find_grab_input_seat ()
    GdkWindow* grab_window = g_object_get_data (G_OBJECT (gdk_window), "gdk-attached-grab-window");
    seat = gdk_window_get_priv_grab_seat_for_single_window (grab_window);
    if (seat)
        return seat;

    while (gdk_window)
    {
        gdk_window = gdk_window_get_priv_transient_for (gdk_window);

        seat = gdk_window_get_priv_grab_seat_for_single_window (gdk_window);
        if (seat)
            return seat;
    }

    return NULL;
}

/*
static void
gdk_window_move_to_rect_impl_override (GdkWindow *window,
                                       const GdkRectangle *rect,
                                       GdkGravity rect_anchor,
                                       GdkGravity window_anchor,
                                       GdkAnchorHints anchor_hints,
                                       int rect_anchor_dx,
                                       int rect_anchor_dy)
{
    g_assert (gdk_window_move_to_rect_real);
    gdk_window_move_to_rect_real (window,
                                  rect,
                                  rect_anchor,
                                  window_anchor,
                                  anchor_hints,
                                  rect_anchor_dx,
                                  rect_anchor_dy);

    GdkWindow *transient_for_gdk_window = gdk_window_get_priv_transient_for (window);
    CustomShellSurface *transient_for_shell_surface = NULL;
    GdkWindow *toplevel_gdk_window = transient_for_gdk_window;
    while (toplevel_gdk_window) {
        toplevel_gdk_window = gdk_window_get_toplevel (toplevel_gdk_window);
        GtkWindow *toplevel_gtk_window = gtk_wayland_gdk_to_gtk_window (toplevel_gdk_window);
        transient_for_shell_surface = gtk_window_get_custom_shell_surface (toplevel_gtk_window);
        if (transient_for_shell_surface)
            break;
        toplevel_gdk_window = gdk_window_get_priv_transient_for (toplevel_gdk_window);
    }
    if (transient_for_shell_surface) {
        g_return_if_fail (rect);
        XdgPopupPosition position = {
            .transient_for_shell_surface = transient_for_shell_surface,
            .transient_for_gdk_window = transient_for_gdk_window,
            .rect = *rect,
            .rect_anchor = rect_anchor,
            .window_anchor = window_anchor,
            .anchor_hints = anchor_hints,
            .rect_anchor_d = {
                .x = rect_anchor_dx,
                .y = rect_anchor_dy,
            },
        };
        gtk_wayland_setup_window_as_custom_popup (window, &position);
    }
}
*/

void
gdk_window_set_priv_mapped (GdkWindow *gdk_window)
{
    GdkWaylandSurface *wayland_surface = (GdkWaylandSurface *)gdk_window;
    gdk_wayland_surface_priv_set_mapped (wayland_surface, TRUE);
}

void gdk_window_notify_priv_mapped (GdkWindow *gdk_window)
{
    // based on gdk_surface_set_is_mapped() in gdksurface.c

    // TODO: clear surface->set_is_mapped_source_id?

    gboolean was_mapped = gdk_surface_priv_get_is_mapped (gdk_window);
    gdk_surface_priv_set_pending_is_mapped (gdk_window, TRUE);
    gdk_surface_priv_set_is_mapped (gdk_window, TRUE);
    if (!was_mapped) {
        g_object_notify (G_OBJECT (gdk_window), "mapped");
    }
}

void
gtk_priv_access_init (GdkWindow *gdk_window)
{
    (void)gdk_window;
    // TODO
    /*
    // Don't do anything once this has run successfully once
    if (gdk_window_move_to_rect_real)
        return;

    GdkWindowImplWayland *window_impl = (GdkWindowImplWayland *)gdk_window_priv_get_impl (gdk_window);
    GdkWindowImplClass *window_class = (GdkWindowImplClass *)G_OBJECT_GET_CLASS(window_impl);

    // If we have not already done the override, set the window's function to be the override and our "real" fp to the one that was there before
    if (gdk_window_impl_class_priv_get_move_to_rect (window_class) != gdk_window_move_to_rect_impl_override) {
        gdk_window_move_to_rect_real = gdk_window_impl_class_priv_get_move_to_rect (window_class);
        gdk_window_impl_class_priv_set_move_to_rect (window_class, gdk_window_move_to_rect_impl_override);
    }
    */
}
