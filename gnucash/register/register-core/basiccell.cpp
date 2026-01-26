/********************************************************************\
 * basiccell.c -- base class for editable cell in a table           *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

/*
 * FILE:
 * basiccell.c
 *
 * FUNCTION:
 * Implements the base class for the cell handler object.
 * See the header file for additional documentation.
 *
 * HISTORY:
 * Copyright (c) 1998 Linas Vepstas
 * Copyright (c) 2000 Dave Peticolas <dave@krondo.com>
 */

#include <config.h>

#include <cstdlib>
#include <clocale>
#include <cstring>

#include "gnc-locale-utils.h"

#include "basiccell.h"
#include "gnc-engine.h"

#include "except-fence.hpp"

/* Debugging module */
static QofLogModule log_module = GNC_MOD_REGISTER;

SAFE_C_API_ARGS(gboolean, gnc_cell_name_equal,
    (const char * cell_name_1, const char * cell_name_2),
	(cell_name_1, cell_name_2))
{
    return (g_strcmp0 (cell_name_1, cell_name_2) == 0);
}

SAFE_C_API_NOARGS(BasicCell *, gnc_basic_cell_new)
{
    BasicCell * cell;

    cell = g_new0 (BasicCell, 1);

    gnc_basic_cell_init (cell);

    return cell;
}

static void
gnc_basic_cell_clear (BasicCell *cell)
{
    g_free (cell->cell_name);
    cell->cell_name = nullptr;
    g_free (cell->cell_type_name);
    cell->cell_type_name = nullptr;
    cell->changed = FALSE;
    cell->conditionally_changed = FALSE;

    cell->value = nullptr;
    cell->value_chars = 0;

    cell->set_value = nullptr;
    cell->enter_cell = nullptr;
    cell->modify_verify = nullptr;
    cell->direct_update = nullptr;
    cell->leave_cell = nullptr;
    cell->gui_realize = nullptr;
    cell->gui_move = nullptr;
    cell->gui_destroy = nullptr;

    cell->is_popup = FALSE;

    cell->gui_private = nullptr;

    g_free (cell->sample_text);
    cell->sample_text = nullptr;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_init, (BasicCell *cell), (cell))
{
    gnc_basic_cell_clear (cell);

    cell->value = g_strdup ("");
}


SAFE_C_API_VOID_ARGS(gnc_basic_cell_destroy, (BasicCell *cell), (cell))
{
    ENTER(" ");
    if (cell->destroy)
        cell->destroy (cell);

    /* give any gui elements a chance to clean up */
    if (cell->gui_destroy)
        (*(cell->gui_destroy)) (cell);

    /* free up data strings */
    g_free (cell->value);
    cell->value = nullptr;

    /* help prevent access to freed memory */
    gnc_basic_cell_clear (cell);

    /* free the object itself */
    g_free (cell);
    LEAVE(" ");
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_name,
    (BasicCell *cell, const char *name),
	(cell, name))
{
    if (!cell) return;
    if (cell->cell_name == name) return;

    g_free (cell->cell_name);
    cell->cell_name = g_strdup (name);
}

SAFE_C_API_ARGS(gboolean, gnc_basic_cell_has_name,
    (BasicCell *cell, const char *name),
	(cell, name))
{
    if (!cell) return FALSE;
    if (!name) return FALSE;
    if (!cell->cell_name) return FALSE;

    return (strcmp (name, cell->cell_name) == 0);
}


SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_type_name,
    (BasicCell *cell, const gchar *type_name),
	(cell, type_name))
{
    if (!cell) return;
    if (cell->cell_type_name == type_name) return;

    g_free (cell->cell_type_name);
    cell->cell_type_name = g_strdup(type_name);
}

SAFE_C_API_ARGS(gboolean, gnc_basic_cell_has_type_name,
    (BasicCell *cell, const gchar *type_name),
	(cell, type_name))
{
    if (!cell) return FALSE;
    if (!type_name) return FALSE;
    if (!cell->cell_type_name) return FALSE;

    return (g_strcmp0 (type_name, cell->cell_type_name));
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_sample_text,
    (BasicCell *cell, const char *sample_text),
	(cell, sample_text))
{
    if (!cell) return;
    if (cell->sample_text == sample_text) return;

    g_free (cell->sample_text);
    cell->sample_text = g_strdup (sample_text);
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_alignment,
    (BasicCell *cell, CellAlignment alignment),
	(cell, alignment))
{
    if (!cell) return;
    cell->alignment = alignment;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_expandable,
    (BasicCell *cell, gboolean expandable),
	(cell, expandable))
{
    if (!cell) return;
    cell->expandable = expandable;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_span,
    (BasicCell *cell, gboolean span),
	(cell, span))
{
    if (!cell) return;
    cell->span = span;
}

SAFE_C_API_ARGS(const char *, gnc_basic_cell_get_value, (BasicCell *cell), (cell))
{
    g_return_val_if_fail (cell != nullptr, nullptr);

    return cell->value;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_value,
    (BasicCell *cell, const char *val),
	(cell, val))
{
    CellSetValueFunc cb;

    cb = cell->set_value;
    if (cb)
    {
        /* avoid recursion by disabling the
         * callback while it's being called. */
        cell->set_value = nullptr;
        cb (cell, val);
        cell->set_value = cb;
    }
    else
        gnc_basic_cell_set_value_internal (cell, val);
}

SAFE_C_API_ARGS(gboolean, gnc_basic_cell_get_changed, (BasicCell *cell), (cell))
{
    if (!cell) return FALSE;

    return cell->changed;
}

SAFE_C_API_ARGS(gboolean, gnc_basic_cell_get_conditionally_changed, (BasicCell *cell), (cell))
{
    if (!cell) return FALSE;

    return cell->conditionally_changed;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_changed,
    (BasicCell *cell, gboolean changed),
	(cell, changed))
{
    if (!cell) return;

    cell->changed = changed;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_conditionally_changed,
    (BasicCell *cell, gboolean changed),
	(cell, changed))
{
    if (!cell) return;

    cell->conditionally_changed = changed;
}

SAFE_C_API_VOID_ARGS(gnc_basic_cell_set_value_internal,
    (BasicCell *cell, const char *value),
	(cell, value))
{
    if (value == nullptr)
        value = "";

    /* If the caller tries to set the value with our own value then do
     * nothing because we have no work to do (or, at least, all the work
     * will result in the status-quo, so why do anything?)  See bug
     * #103174 and the description in the changelog on 2003-09-04.
     */
    if (cell->value == value)
        return;

    g_free (cell->value);
    cell->value = g_strdup (value);
    cell->value_chars = g_utf8_strlen(value, -1);
}

SAFE_C_API_ARGS(char *, gnc_basic_cell_validate,
    (BasicCell *cell, GNCPrintAmountInfo print_info,
    const char *change, const char *newval,
    const char *toks, gint *cursor_position),
	(cell, print_info, change, newval, toks, cursor_position))
{
    struct lconv *lc = gnc_localeconv ();
    gunichar decimal_point;
    gunichar thousands_sep;
    const char *symbol = nullptr;
    char *tokens;

    if (print_info.monetary)
    {
        const gnc_commodity *comm = print_info.commodity;

        decimal_point = g_utf8_get_char (lc->mon_decimal_point);
        thousands_sep = g_utf8_get_char (lc->mon_thousands_sep);

        if (comm)
            symbol = gnc_commodity_get_nice_symbol (comm);
        else
            symbol = gnc_commodity_get_nice_symbol (gnc_default_currency ());

        tokens = g_strconcat (toks, symbol, nullptr);
    }
    else
    {
        decimal_point = g_utf8_get_char (lc->decimal_point);
        thousands_sep = g_utf8_get_char (lc->thousands_sep);

        tokens = g_strdup (toks);
    }

    for (const char *c = change; c && *c; c = g_utf8_next_char (c))
    {
        gunichar uc = g_utf8_get_char (c);
        if (!g_unichar_isdigit (uc) &&
            !g_unichar_isspace (uc) &&
            !g_unichar_isalpha (uc) &&
            (decimal_point != uc) &&
            (thousands_sep != uc) &&
            (g_utf8_strchr (tokens, -1, uc) == nullptr))
        {
            g_free (tokens);
            return nullptr;
        }
    }
    g_free (tokens);

    gnc_filter_text_set_cursor_position (newval, symbol, cursor_position);

    return gnc_filter_text_for_currency_symbol (newval, symbol);
}
