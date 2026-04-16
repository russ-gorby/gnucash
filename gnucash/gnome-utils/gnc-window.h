/*
 * gnc-window.h -- structure which represents a GnuCash window.
 *
 * Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
 * Copyright (C) 2003 David Hampton <hampton@employees.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

/** @addtogroup Windows
    @{ */
/** @addtogroup GncWindow Common Window Functions
    @{ */
/** @file gnc-window.h
    @brief Functions that are supported by all types of windows.
    @author Copyright (C) 2003 Jan Arne Petersen
    @author Copyright (C) 2003 David Hampton <hampton@employees.org>

    GnuCash has two types of "windows" that can show Plugin Pages.
    The first is called a "Main Window" and is implemented on top of a
    GtkWindow.  The second is called an "Embedded Window" and is
    implemented on top of a GtkBox.  The functions in this file will
    work with either type of window.
*/

#ifndef __GNC_WINDOW_H
#define __GNC_WINDOW_H

#include <gtk/gtk.h>
#include "gnc-plugin-page.h"

#ifdef __cplusplus
#define NOEXCEPT noexcept
extern "C"
{
#else
#define NOEXCEPT
#endif

/* type macros */
#define GNC_TYPE_WINDOW            (gnc_window_get_type ())
G_DECLARE_INTERFACE (GncWindow, gnc_window, GNC, WINDOW, GObject)

/* typedefs & structures */
struct _GncWindowInterface
{
    GTypeInterface parent;

    /* Virtual Table */
    GtkWindow  * (* get_gtk_window) (GncWindow *window);
    GtkWidget  * (* get_statusbar) (GncWindow *window);
    GtkWidget  * (* get_progressbar) (GncWindow *window);
    GtkWidget  * (* get_menubar) (GncWindow *window);
    GtkWidget  * (* get_toolbar) (GncWindow *window);
    GMenuModel  * (* get_menubar_model) (GncWindow *window);
    GtkAccelGroup * (* get_accel_group) (GncWindow *window);
    void (* ui_set_sensitive) (GncWindow *window, gboolean sensitive);
};

/* function prototypes */
GtkWindow     *gnc_window_get_gtk_window (GncWindow *window) NOEXCEPT;

void           gnc_window_update_status (GncWindow *window, GncPluginPage *page) NOEXCEPT;
void           gnc_window_set_status (GncWindow *window, GncPluginPage *page, const gchar *message) NOEXCEPT;

void           gnc_window_set_progressbar_window (GncWindow *window) NOEXCEPT;
GncWindow     *gnc_window_get_progressbar_window (void) NOEXCEPT;
GtkWidget     *gnc_window_get_progressbar (GncWindow *window) NOEXCEPT;
void           gnc_window_show_progress (const char *message, double percentage) NOEXCEPT;
GtkWidget     *gnc_window_get_menubar (GncWindow *window) NOEXCEPT;
GtkWidget     *gnc_window_get_toolbar (GncWindow *window) NOEXCEPT;
GtkWidget     *gnc_window_get_statusbar (GncWindow *window) NOEXCEPT;
GMenuModel    *gnc_window_get_menubar_model (GncWindow *window) NOEXCEPT;
GtkAccelGroup *gnc_window_get_accel_group (GncWindow *window) NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* __GNC_WINDOW_H */

/** @} */
/** @} */
