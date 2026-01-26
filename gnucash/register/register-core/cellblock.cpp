/********************************************************************\
 * cellblock.c -- group of cells that act as cursor within a table  *
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
 * cellblock.c
 *
 * FUNCTION:
 * implements a rectangular array of cells. See the header file for
 * additional documentation.
 *
 * HISTORY:
 * Copyright (c) 1998 Linas Vepstas
 * Copyright (c) 2000-2001 Dave Peticolas <dave@krondo.com>
 */

#include <config.h>

#include "cellblock.h"

#include "except-fence.hpp"

static void gnc_cellblock_init (CellBlock *cellblock, int rows, int cols);

SAFE_C_API_ARGS(CellBlock *, gnc_cellblock_new,
    (int rows, int cols, const char *cursor_name),
	(rows, cols, cursor_name))
{
    CellBlock *cellblock;

    g_return_val_if_fail (rows > 0, nullptr);
    g_return_val_if_fail (cols > 0, nullptr);
    g_return_val_if_fail (cursor_name != nullptr, nullptr);

    cellblock = g_new0 (CellBlock, 1);

    gnc_cellblock_init (cellblock, rows, cols);

    cellblock->cursor_name = g_strdup (cursor_name);

    return cellblock;
}

static void
gnc_cellblock_init (CellBlock *cellblock, int rows, int cols)
{
    /* record new size */
    cellblock->num_rows = rows;
    cellblock->num_cols = cols;

    cellblock->start_col = cols;
    cellblock->stop_col  = -1;

    /* malloc new cell table */
    cellblock->cells = g_ptr_array_new ();

    g_ptr_array_set_size (cellblock->cells, rows * cols);
}

SAFE_C_API_VOID_ARGS(gnc_cellblock_destroy, (CellBlock *cellblock), (cellblock))
{
    if (!cellblock) return;

    g_ptr_array_free (cellblock->cells, TRUE);
    cellblock->cells = nullptr;

    g_free (cellblock->cursor_name);
    cellblock->cursor_name = nullptr;

    g_free (cellblock);
}

SAFE_C_API_VOID_ARGS(gnc_cellblock_set_cell,
    (CellBlock *cellblock, int row, int col, BasicCell *cell),
	(cellblock, row, col, cell))
{
    if (cellblock == nullptr)
        return;

    if (row < 0 || row >= cellblock->num_rows)
        return;

    if (col < 0 || col >= cellblock->num_cols)
        return;

    cellblock->cells->pdata[(row * cellblock->num_cols) + col] = cell;
}

SAFE_C_API_ARGS(BasicCell *, gnc_cellblock_get_cell,
    (CellBlock *cellblock, int row, int col),
	(cellblock, row, col))
{
    if (cellblock == nullptr)
        return nullptr;

    if (row < 0 || row >= cellblock->num_rows)
        return nullptr;

    if (col < 0 || col >= cellblock->num_cols)
        return nullptr;

    return static_cast<BasicCell *>(cellblock->cells->pdata[(row * cellblock->num_cols) + col]);
}

SAFE_C_API_ARGS(BasicCell *, gnc_cellblock_get_cell_by_name,
    (CellBlock *cellblock, const char *cell_name, int *row, int *col),
	(cellblock, cell_name, row, col))
{
    int r, c, num_rows, num_cols;

    if (cellblock == nullptr)
        return nullptr;

    if (cell_name == nullptr)
        return nullptr;

    num_rows = cellblock->num_rows;
    num_cols = cellblock->num_cols;
    for (r = 0; r < num_rows; r++)
        for (c = 0; c < num_cols; c++)
        {
            BasicCell *cell = static_cast<BasicCell *>(cellblock->cells->pdata[(r * num_cols) + c]);
            if (!cell) continue;
            if (gnc_cell_name_equal(cell->cell_name, cell_name))
            {
                if (row)
                    *row = r;
                if (col)
                    *col = c;
                return cell;
            }
        }

    return nullptr;
}

SAFE_C_API_ARGS(int, gnc_cellblock_changed,
    (CellBlock *cursor, gboolean include_conditional),
	(cursor, include_conditional))
{
    int changed = 0;
    int r, c;

    if (!cursor)
        return FALSE;

    for (r = 0; r < cursor->num_rows; r++)
        for (c = 0; c < cursor->num_cols; c++)
        {
            BasicCell *cell;

            cell = gnc_cellblock_get_cell (cursor, r, c);
            if (cell == nullptr)
                continue;

            if (gnc_basic_cell_get_changed (cell))
            {
                changed++;
                continue;
            }

            if (include_conditional &&
                    gnc_basic_cell_get_conditionally_changed (cell))
                changed++;
        }

    return changed;
}

SAFE_C_API_VOID_ARGS(gnc_cellblock_clear_changes, (CellBlock *cursor), (cursor))
{
    int r, c;

    if (!cursor)
        return;

    for (r = 0; r < cursor->num_rows; r++)
        for (c = 0; c < cursor->num_cols; c++)
        {
            BasicCell *cell;

            cell = gnc_cellblock_get_cell (cursor, r, c);
            if (cell == nullptr)
                continue;

            gnc_basic_cell_set_changed (cell, FALSE);
            gnc_basic_cell_set_conditionally_changed (cell, FALSE);
        }
}
