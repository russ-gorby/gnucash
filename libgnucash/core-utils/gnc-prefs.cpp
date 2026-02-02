/*
 * gnc-prefs.c:
 *
 * Copyright (C) 2006 Chris Shoemaker <c.shoemaker@cox.net>
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

#include <string>

#include <cstdlib>
#include <glib.h>
#include <config.h>
#include "gnc-prefs.h"
#include "gnc-prefs-p.h"
#include "gnc-version.h"
#include "except-fence.hpp"

static std::string namespace_regexp;
static gboolean is_debugging      = FALSE;
static gboolean extras_enabled    = FALSE;
static gboolean use_compression   = TRUE; // This is also the default in the prefs backend
static gint file_retention_policy = 1;    // 1 = "days", the default in the prefs backend
static gint file_retention_days   = 30;   // This is also the default in the prefs backend


/* Global variables used to remove the preference registered callbacks
 * that were setup via a g_once. */
static gulong reg_auto_raise_lists_id;
static gulong reg_negative_color_pref_id;

PrefsBackend *prefsbackend = NULL;

SAFE_C_API_NOARGS(const gchar *, gnc_prefs_get_namespace_regexp)
{
    return namespace_regexp.c_str();
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_namespace_regexp,
	(const gchar *str), (str))
{
    if (str)
        namespace_regexp = str;
}

SAFE_C_API_NOARGS(gboolean, gnc_prefs_is_debugging_enabled)
{
    return is_debugging;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_debugging,
	(gboolean d), (d))
{
    is_debugging = d;
}

SAFE_C_API_NOARGS(gboolean, gnc_prefs_is_extra_enabled)
{
    return extras_enabled;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_extra,
	(gboolean enabled), (enabled))
{
    extras_enabled = enabled;
}

SAFE_C_API_NOARGS(gboolean, gnc_prefs_get_file_save_compressed)
{
    return use_compression;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_file_save_compressed,
	(gboolean compressed), (compressed))
{
    use_compression = compressed;
}

SAFE_C_API_NOARGS(gint, gnc_prefs_get_file_retention_policy)
{
    return file_retention_policy;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_file_retention_policy,
	(gint policy), (policy))
{
    file_retention_policy = policy;
}

SAFE_C_API_NOARGS(gint, gnc_prefs_get_file_retention_days)
{
    return file_retention_days;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_file_retention_days,
	(gint days), (days))
{
    file_retention_days = days;
}

SAFE_C_API_NOARGS(guint, gnc_prefs_get_long_version)
{
     return PROJECT_VERSION_MAJOR * 1000000 + PROJECT_VERSION_MINOR;
}

SAFE_C_API_ARGS(gulong, gnc_prefs_register_cb,
	(const char *group, const gchar *pref_name,
    gpointer func, gpointer user_data),
	(group, pref_name, func, user_data))
{
    if (prefsbackend && prefsbackend->register_cb)
        return (prefsbackend->register_cb) (group, pref_name, func, user_data);
    else
    {
        g_warning ("no preferences backend loaded, or the backend doesn't define register_cb, returning 0");
        return 0;
    }
}


SAFE_C_API_VOID_ARGS(gnc_prefs_remove_cb_by_func,
	(const gchar *group, const gchar *pref_name,
    gpointer func, gpointer user_data),
	(group, pref_name, func, user_data))
{
    if (prefsbackend && prefsbackend->remove_cb_by_func)
        (prefsbackend->remove_cb_by_func) (group, pref_name, func, user_data);
}


SAFE_C_API_VOID_ARGS(gnc_prefs_remove_cb_by_id,
	(const gchar *group, guint id),
	(group, id))
{
    if (prefsbackend && prefsbackend->remove_cb_by_id)
        (prefsbackend->remove_cb_by_id) (group, id);
}


SAFE_C_API_ARGS(guint, gnc_prefs_register_group_cb,
	(const gchar *group, gpointer func, gpointer user_data),
	(group, func, user_data))
{
    if (prefsbackend && prefsbackend->register_group_cb)
        return (prefsbackend->register_group_cb) (group, func, user_data);
    else
        return 0;
}


SAFE_C_API_VOID_ARGS(gnc_prefs_remove_group_cb_by_func,
	(const gchar *group, gpointer func, gpointer user_data),
	(group, func, user_data))
{
    if (prefsbackend && prefsbackend->remove_group_cb_by_func)
        (prefsbackend->remove_group_cb_by_func) (group, func, user_data);
}


SAFE_C_API_VOID_ARGS(gnc_prefs_bind,
	(const gchar *group,
    /*@ null @*/ const gchar *pref_name,
    /*@ null @*/ const gchar *pref_value,
    gpointer object, const gchar *property),
	(group, pref_name, pref_value, object, property))
{
    if (prefsbackend && prefsbackend->bind)
        (prefsbackend->bind) (group, pref_name, pref_value, object, property);
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_get_bool,
	(const gchar *group, /*@ null @*/ const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->get_bool)
        return (prefsbackend->get_bool) (group, pref_name);
    else
        return FALSE;
}


SAFE_C_API_ARGS(gint, gnc_prefs_get_int,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->get_int)
        return (prefsbackend->get_int) (group, pref_name);
    else
        return 0;
}


SAFE_C_API_ARGS(gint64, gnc_prefs_get_int64,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    gint64 result = 0;
    GVariant *var = gnc_prefs_get_value(group, pref_name);
    result = g_variant_get_int64 (var);
    g_variant_unref (var);
    return result;
}


SAFE_C_API_ARGS(gdouble, gnc_prefs_get_float,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->get_float)
        return (prefsbackend->get_float) (group, pref_name);
    else
        return 0.0;
}


SAFE_C_API_ARGS(gchar *, gnc_prefs_get_string,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->get_string)
        return (prefsbackend->get_string) (group, pref_name);
    else
        return NULL;
}


SAFE_C_API_ARGS(gint, gnc_prefs_get_enum,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->get_enum)
        return (prefsbackend->get_enum) (group, pref_name);
    else
        return 0;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_get_coords,
	(const gchar *group, const gchar *pref_name, gdouble *x, gdouble *y),
	(group, pref_name, x, y))
{
    GVariant *coords = gnc_prefs_get_value (group, pref_name);

    *x = 0;
    *y = 0;

    if (g_variant_is_of_type (coords, (const GVariantType *) "(dd)") )
        g_variant_get (coords, "(dd)", x, y);
    g_variant_unref (coords);
}


SAFE_C_API_ARGS(GVariant *, gnc_prefs_get_value,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->get_value)
        return (prefsbackend->get_value) (group,pref_name);
    else
        return NULL;
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_bool,
	(const gchar *group, const gchar *pref_name, gboolean value),
	(group, pref_name, value))
{
    if (prefsbackend && prefsbackend->set_bool)
        return (prefsbackend->set_bool) (group, pref_name, value);
    else
        return FALSE;
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_int,
	(const gchar *group, const gchar *pref_name, gint value),
	(group, pref_name, value))
{
    if (prefsbackend && prefsbackend->set_int)
        return (prefsbackend->set_int) (group, pref_name, value);
    else
        return FALSE;
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_int64,
	(const gchar *group,const gchar *pref_name, gint64 value),
	(group, pref_name, value))
{
    GVariant *var = g_variant_new ("x",value);
    return gnc_prefs_set_value (group, pref_name, var);
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_float,
	(const gchar *group, const gchar *pref_name, gdouble value),
	(group, pref_name, value))
{
    if (prefsbackend && prefsbackend->set_float)
        return (prefsbackend->set_float) (group, pref_name, value);
    else
        return FALSE;
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_string,
	(const gchar *group, const gchar *pref_name, const gchar *value),
	(group, pref_name, value))
{
    if (prefsbackend && prefsbackend->set_string)
        return (prefsbackend->set_string) (group, pref_name, value);
    else
        return FALSE;
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_enum,
	(const gchar *group, const gchar *pref_name, gint value),
	(group, pref_name, value))
{
    if (prefsbackend && prefsbackend->set_enum)
        return (prefsbackend->set_enum) (group, pref_name, value);
    else
        return FALSE;
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_coords,
	(const gchar *group, const gchar *pref_name, gdouble x, gdouble y),
	(group, pref_name, x, y))
{
    GVariant *var = g_variant_new ("(dd)",x, y);
    return gnc_prefs_set_value (group, pref_name, var);
}


SAFE_C_API_ARGS(gboolean, gnc_prefs_set_value,
	(const gchar *group, const gchar *pref_name, GVariant *value),
	(group, pref_name, value))
{
    if (prefsbackend && prefsbackend->set_value)
        return (prefsbackend->set_value) (group, pref_name, value);
    else
        return FALSE;
}


SAFE_C_API_VOID_ARGS(gnc_prefs_reset,
	(const gchar *group, const gchar *pref_name),
	(group, pref_name))
{
    if (prefsbackend && prefsbackend->reset)
        (prefsbackend->reset) (group, pref_name);
}

SAFE_C_API_VOID_ARGS(gnc_prefs_reset_group,
	(const gchar *group), (group))
{
    if (prefsbackend && prefsbackend->reset_group)
        (prefsbackend->reset_group) (group);
}

SAFE_C_API_NOARGS(gboolean, gnc_prefs_is_set_up)
{
    return (prefsbackend !=NULL);
}

SAFE_C_API_VOID_NOARGS(gnc_prefs_block_all)
{
    if (prefsbackend && prefsbackend->block_all)
        (prefsbackend->block_all) ();
}

SAFE_C_API_VOID_NOARGS(gnc_prefs_unblock_all)
{
    if (prefsbackend && prefsbackend->unblock_all)
        (prefsbackend->unblock_all) ();
}

SAFE_C_API_NOARGS(gulong, gnc_prefs_get_reg_auto_raise_lists_id)
{
    return reg_auto_raise_lists_id;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_reg_auto_raise_lists_id,
	(gulong id), (id))
{
    reg_auto_raise_lists_id = id;
}

SAFE_C_API_NOARGS(gulong, gnc_prefs_get_reg_negative_color_pref_id)
{
    return reg_negative_color_pref_id;
}

SAFE_C_API_VOID_ARGS(gnc_prefs_set_reg_negative_color_pref_id,
	(gulong id), (id))
{
    reg_negative_color_pref_id = id;
}

