/********************************************************************\
 * table-layout.c -- 2D table layout                                *
 * Copyright (c) 2001 Free Software Foundation                      *
 * Author: Dave Peticolas <dave@krondo.com>                         *
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

#include <config.h>

#include <glib.h>
#include <cstring>

#include "basiccell.h"
#include "cellblock.h"
#include "table-layout.h"

#include "except-fence.hpp"

struct table_layout_struct
{
    GList *cells;   /* The cells in the table */
    GList *cursors; /* The cursors in the table */

    CellBlock *primary_cursor;
};

typedef struct _CellBuffer CellBuffer;
struct _CellBuffer
{
    char * cell_name;
    char * value;
    guint32 changed;
    guint32 conditionally_changed;
};

struct cursor_buffer_struct
{
    GList *cell_buffers;
};


SAFE_C_API_NOARGS(TableLayout *, gnc_table_layout_new)
{
    TableLayout *layout;

    layout = g_new0 (TableLayout, 1);

    return layout;
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_destroy, (TableLayout *layout), (layout))
{
    GList *node;

    if (!layout)
        return;

    for (node = layout->cells; node; node = node->next)
    {
        BasicCell *cell = static_cast<BasicCell *>(node->data);

        gnc_basic_cell_destroy (cell);
    }
    g_list_free (layout->cells);
    layout->cells = nullptr;

    for (node = layout->cursors; node; node = node->next)
    {
        CellBlock *cursor = static_cast<CellBlock *>(node->data);

        gnc_cellblock_destroy (cursor);
    }
    g_list_free (layout->cursors);
    layout->cursors = nullptr;

    g_free (layout);
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_add_cell,
    (TableLayout *layout, BasicCell *cell),
	(layout, cell))
{
    GList *node;

    g_return_if_fail (layout != nullptr);
    g_return_if_fail (cell != nullptr);

    for (node = layout->cells; node; node = node->next)
    {
        BasicCell *list_cell = static_cast<BasicCell *>(node->data);

        if (gnc_basic_cell_has_name (list_cell, cell->cell_name))
        {
            if (list_cell == cell)
                return;

            gnc_basic_cell_destroy (list_cell);
            break;
        }
    }

    if (!node)
        layout->cells = g_list_append (layout->cells, cell);
    else
        node->data = cell;
}

SAFE_C_API_ARGS(BasicCell *, gnc_table_layout_get_cell,
    (TableLayout *layout, const char *cell_name),
	(layout, cell_name))
{
    GList *node;

    g_return_val_if_fail (layout != nullptr, nullptr);

    for (node = layout->cells; node; node = node->next)
    {
        BasicCell *list_cell = static_cast<BasicCell *>(node->data);

        if (gnc_basic_cell_has_name (list_cell, cell_name))
            return list_cell;
    }

    return nullptr;
}

SAFE_C_API_ARGS(const char *, gnc_table_layout_get_cell_value,
    (TableLayout *layout, const char * cell_name),
	(layout, cell_name))
{
    BasicCell *cell;

    g_return_val_if_fail (layout != nullptr, nullptr);

    cell = gnc_table_layout_get_cell (layout, cell_name);
    if (!cell) return nullptr;

    return gnc_basic_cell_get_value (cell);
}

SAFE_C_API_ARGS(gboolean, gnc_table_layout_get_cell_changed,
    (TableLayout *layout, const char *cell_name, gboolean include_conditional),
	(layout, cell_name, include_conditional))
{
    BasicCell *cell;

    g_return_val_if_fail (layout != nullptr, FALSE);

    cell = gnc_table_layout_get_cell (layout, cell_name);
    if (!cell) return FALSE;

    if (!include_conditional)
        return gnc_basic_cell_get_changed (cell);
    else
        return (gnc_basic_cell_get_changed (cell) ||
                gnc_basic_cell_get_conditionally_changed (cell));
}

SAFE_C_API_ARGS(GList *, gnc_table_layout_get_cells, (TableLayout *layout), (layout))
{
    if (!layout)
        return nullptr;

    return layout->cells;
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_add_cursor,
    (TableLayout *layout, CellBlock *cursor),
	(layout, cursor))
{
    GList *node;

    g_return_if_fail (layout != nullptr);
    g_return_if_fail (cursor != nullptr);

    if (g_list_find (layout->cursors, cursor))
        return;

    for (node = layout->cursors; node; node = node->next)
    {
        CellBlock *list_cursor = static_cast<CellBlock *>(node->data);

        if (strcmp (list_cursor->cursor_name, cursor->cursor_name) == 0)
        {
            layout->cursors = g_list_remove (layout->cursors, list_cursor);
            gnc_cellblock_destroy (list_cursor);
            break;
        }
    }

    layout->cursors = g_list_append (layout->cursors, cursor);
}

SAFE_C_API_ARGS(CellBlock *, gnc_table_layout_get_cursor,
    (TableLayout *layout, const char *cursor_name),
	(layout, cursor_name))
{
    GList *node;

    g_return_val_if_fail (layout != nullptr, nullptr);

    if (!cursor_name)
        return nullptr;

    for (node = layout->cursors; node; node = node->next)
    {
        CellBlock *cursor = static_cast<CellBlock *>(node->data);

        if (strcmp (cursor_name, cursor->cursor_name) == 0)
            return cursor;
    }

    return nullptr;
}

SAFE_C_API_ARGS(GList *, gnc_table_layout_get_cursors, (TableLayout *layout), (layout))
{
    g_return_val_if_fail (layout != nullptr, nullptr);
    return layout->cursors;
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_set_primary_cursor,
    (TableLayout *layout, CellBlock *cursor),
	(layout, cursor))
{
    g_return_if_fail (layout != nullptr);
    layout->primary_cursor = cursor;
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_set_cell,
    (TableLayout *layout, CellBlock *cursor, const char *cell_name, int row, int col),
	(layout, cursor, cell_name, row, col))
{
    CellBlock *header;
    BasicCell *cell;

    g_return_if_fail (layout != nullptr);
    g_return_if_fail (layout->primary_cursor != nullptr);
    g_return_if_fail (cursor != nullptr);
    g_return_if_fail (cell_name != nullptr);
    g_return_if_fail (row >= 0);
    g_return_if_fail (col >= 0);
    g_return_if_fail (row < cursor->num_rows);
    g_return_if_fail (col < cursor->num_cols);

    header = gnc_table_layout_get_cursor (layout, CURSOR_HEADER);
    cell = gnc_table_layout_get_cell (layout, cell_name);

    g_return_if_fail (header != nullptr);
    g_return_if_fail (cell != nullptr);

    cursor->start_col = MIN (cursor->start_col, col);
    cursor->stop_col  = MAX (cursor->stop_col,  col);

    header->start_col = MIN (header->start_col, col);
    header->stop_col  = MAX (header->stop_col,  col);

    gnc_cellblock_set_cell (cursor, row, col, cell);

    if (cursor == layout->primary_cursor)
        gnc_cellblock_set_cell (header, row, col, cell);
}

SAFE_C_API_NOARGS(CursorBuffer *, gnc_cursor_buffer_new)
{
    CursorBuffer *buffer;

    buffer = g_new0 (CursorBuffer, 1);

    return buffer;
}

static void
destroy_cell_buffer (CellBuffer *cb)
{
    if (cb == nullptr)
        return;

    g_free (cb->cell_name);
    cb->cell_name = nullptr;

    g_free (cb->value);
    cb->value = nullptr;

    g_free (cb);
}

static void
gnc_cursor_buffer_clear (CursorBuffer *buffer)
{
    GList *node;

    if (!buffer) return;

    for (node = buffer->cell_buffers; node; node = node->next)
    {
        CellBuffer *cb = static_cast<CellBuffer *>(node->data);

        destroy_cell_buffer (cb);
    }

    g_list_free (buffer->cell_buffers);
    buffer->cell_buffers = nullptr;
}

SAFE_C_API_VOID_ARGS(gnc_cursor_buffer_destroy, (CursorBuffer *buffer), (buffer))
{
    if (!buffer) return;

    gnc_cursor_buffer_clear (buffer);

    g_free (buffer);
}

static CellBuffer *
save_cell (BasicCell *bcell)
{
    CellBuffer *cb;

    if (!bcell)
        return nullptr;

    cb = g_new0 (CellBuffer, 1);

    cb->cell_name = g_strdup (bcell->cell_name);
    cb->value = g_strdup (bcell->value);
    cb->changed = bcell->changed;
    cb->conditionally_changed = bcell->conditionally_changed;

    return cb;
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_save_cursor,
    (TableLayout *layout, CellBlock *cursor, CursorBuffer *buffer),
	(layout, cursor, buffer))
{
    GList *node;

    if (!layout || !cursor || !buffer)
        return;

    gnc_cursor_buffer_clear (buffer);

    for (node = layout->cells; node; node = node->next)
    {
        BasicCell *list_cell = static_cast<BasicCell *>(node->data);
        CellBuffer *cb;

        if (!gnc_basic_cell_get_changed (list_cell) &&
                !gnc_basic_cell_get_conditionally_changed (list_cell))
            continue;

        cb = save_cell (list_cell);

        buffer->cell_buffers = g_list_prepend (buffer->cell_buffers, cb);
    }
}

static void
restore_cell (BasicCell *bcell, CellBuffer *cb, CellBlock *cursor)
{
    int r, c;

    if (!bcell || !cb || !cursor)
        return;

    if (!cb->changed && !cb->conditionally_changed)
        return;

    /* only restore if it's in the current cursor */
    for (r = 0; r < cursor->num_rows; r++)
        for (c = 0; c < cursor->num_cols; c++)
        {
            BasicCell *cell;

            cell = gnc_cellblock_get_cell (cursor, r, c);
            if (!cell)
                continue;

            if (cell == bcell)
            {
                gnc_basic_cell_set_value (bcell, cb->value);
                bcell->changed = cb->changed;
                bcell->conditionally_changed = cb->conditionally_changed;
                return;
            }
        }
}

SAFE_C_API_VOID_ARGS(gnc_table_layout_restore_cursor,
    (TableLayout *layout, CellBlock *cursor, CursorBuffer *buffer),
	(layout, cursor, buffer))
{
    GList *node;

    if (!layout || !cursor || !buffer)
        return;

    for (node = buffer->cell_buffers; node; node = node->next)
    {
        CellBuffer *cb = static_cast<CellBuffer *>(node->data);
        BasicCell *cell;

        cell = gnc_table_layout_get_cell (layout, cb->cell_name);

        restore_cell (cell, cb, cursor);
    }
}
