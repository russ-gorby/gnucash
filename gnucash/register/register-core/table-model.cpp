/********************************************************************\
 * table-model.c -- 2D grid table object model                      *
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

#include "table-model.h"

#include "except-fence.hpp"


#define DEFAULT_HANDLER ""

typedef struct
{
    char *cell_name;
    gpointer handler;
} HandlerNode;


static GHashTable *
gnc_table_model_handler_hash_new (void)
{
    return g_hash_table_new (g_str_hash, g_str_equal);
}

static void
hash_destroy_helper (gpointer key, gpointer value, gpointer user_data)
{
    HandlerNode *node = static_cast<HandlerNode *>(value);

    g_free (node->cell_name);
    node->cell_name = nullptr;

    g_free (node);
}

static void
gnc_table_model_handler_hash_destroy (GHashTable *hash)
{
    if (!hash) return;

    g_hash_table_foreach (hash, hash_destroy_helper, nullptr);
    g_hash_table_destroy (hash);
}

static void
gnc_table_model_handler_hash_remove (GHashTable *hash, const char *cell_name)
{
    HandlerNode *node;

    if (!hash) return;

    node = static_cast<HandlerNode *>(g_hash_table_lookup (hash, cell_name));
    if (!node) return;

    g_hash_table_remove (hash, cell_name);

    g_free (node->cell_name);
    node->cell_name = nullptr;

    g_free (node);
}

static void
gnc_table_model_handler_hash_insert (GHashTable *hash,
                                     const char *cell_name,
                                     gpointer handler)
{
    HandlerNode *node;

    g_return_if_fail (hash != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_remove (hash, cell_name);
    if (!handler) return;

    node = g_new0 (HandlerNode, 1);

    node->cell_name = g_strdup (cell_name);
    node->handler = handler;

    g_hash_table_insert (hash, node->cell_name, node);
}

static gpointer
gnc_table_model_handler_hash_lookup (GHashTable *hash, const char *cell_name)
{
    HandlerNode *node;

    if (!hash) return nullptr;

    if (cell_name)
    {
        node = static_cast<HandlerNode *>(g_hash_table_lookup (hash, cell_name));
        if (node) return node->handler;
    }

    cell_name = DEFAULT_HANDLER;
    node = static_cast<HandlerNode *>(g_hash_table_lookup (hash, cell_name));
    if (node) return node->handler;

    return nullptr;
}

SAFE_C_API_NOARGS(TableModel *, gnc_table_model_new)
{
    TableModel *model;

    model = g_new0 (TableModel, 1);

    model->entry_handlers = gnc_table_model_handler_hash_new ();
    model->label_handlers = gnc_table_model_handler_hash_new ();
    model->help_handlers = gnc_table_model_handler_hash_new ();
    model->tooltip_handlers = gnc_table_model_handler_hash_new ();
    model->io_flags_handlers = gnc_table_model_handler_hash_new ();
    model->cell_color_handlers = gnc_table_model_handler_hash_new ();
    model->cell_border_handlers = gnc_table_model_handler_hash_new ();
    model->confirm_handlers = gnc_table_model_handler_hash_new ();
    model->save_handlers = gnc_table_model_handler_hash_new ();

    model->read_only = FALSE;
    model->dividing_row_upper = -1;
    model->dividing_row = -1;
    model->dividing_row_lower = -1;

    model->blank_trans_row = -1;

    return model;
}

SAFE_C_API_VOID_ARGS(gnc_table_model_destroy, (TableModel *model), (model))
{
    if (!model) return;

    gnc_table_model_handler_hash_destroy (model->entry_handlers);
    model->entry_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->label_handlers);
    model->label_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->tooltip_handlers);
    model->tooltip_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->help_handlers);
    model->help_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->io_flags_handlers);
    model->io_flags_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->cell_color_handlers);
    model->cell_color_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->cell_border_handlers);
    model->cell_border_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->confirm_handlers);
    model->confirm_handlers = nullptr;

    gnc_table_model_handler_hash_destroy (model->save_handlers);
    model->save_handlers = nullptr;

    g_free (model);
}

SAFE_C_API_VOID_ARGS(gnc_table_model_set_read_only, 
    (TableModel *model, gboolean read_only),
	(model, read_only))
{
    g_return_if_fail (model);

    model->read_only = read_only;
}

SAFE_C_API_ARGS(gboolean, gnc_table_model_read_only, (TableModel *model), (model))
{
    g_return_val_if_fail (model, FALSE);

    return model->read_only;
}

SAFE_C_API_VOID_ARGS(gnc_table_model_set_reverse_sort,
    (TableModel *model, gboolean reverse_sort),
	(model, reverse_sort))
{
    g_return_if_fail (model);
    model->reverse_sort = reverse_sort;
}

SAFE_C_API_VOID_ARGS(gnc_table_model_set_entry_handler,
    (TableModel *model, TableGetEntryHandler entry_handler, const char * cell_name),
	(model, entry_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->entry_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(entry_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_entry_handler,
    (TableModel *model, TableGetEntryHandler entry_handler),
	(model, entry_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->entry_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(entry_handler));
}
SAFE_C_API_ARGS(TableGetEntryHandler, gnc_table_model_get_entry_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetEntryHandler handler = reinterpret_cast<TableGetEntryHandler>(
        gnc_table_model_handler_hash_lookup (
            model->entry_handlers,
            cell_name));

    return handler ;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_label_handler,
    (TableModel *model, TableGetLabelHandler label_handler, const char * cell_name),
	(model, label_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->label_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(label_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_label_handler,
    (TableModel *model, TableGetLabelHandler label_handler),
	(model, label_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->label_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(label_handler));
}
SAFE_C_API_ARGS(TableGetLabelHandler, gnc_table_model_get_label_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetLabelHandler handler = reinterpret_cast<TableGetLabelHandler>(
        gnc_table_model_handler_hash_lookup (model->label_handlers,
            cell_name));

    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_tooltip_handler,
    (TableModel *model, TableGetTooltipHandler tooltip_handler, const char * cell_name),
	(model, tooltip_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->tooltip_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(tooltip_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_tooltip_handler,
    (TableModel *model, TableGetTooltipHandler tooltip_handler),
	(model, tooltip_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->tooltip_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(tooltip_handler));
}
SAFE_C_API_ARGS(TableGetTooltipHandler, gnc_table_model_get_tooltip_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetTooltipHandler handler = reinterpret_cast<TableGetTooltipHandler>(
        gnc_table_model_handler_hash_lookup (model->tooltip_handlers,
            cell_name));
    
    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_help_handler,
    (TableModel *model, TableGetHelpHandler help_handler, const char * cell_name),
	(model, help_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->help_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(help_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_help_handler,
    (TableModel *model, TableGetHelpHandler help_handler),
	(model, help_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->help_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(help_handler));
}
SAFE_C_API_ARGS(TableGetHelpHandler, gnc_table_model_get_help_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetHelpHandler handler = reinterpret_cast<TableGetHelpHandler>(
        gnc_table_model_handler_hash_lookup (model->help_handlers, cell_name));
    
    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_io_flags_handler,
    (TableModel *model, TableGetCellIOFlagsHandler io_flags_handler, const char * cell_name),
	(model, io_flags_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->io_flags_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(io_flags_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_io_flags_handler,
    (TableModel *model, TableGetCellIOFlagsHandler io_flags_handler),
	(model, io_flags_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->io_flags_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(io_flags_handler));
}
SAFE_C_API_ARGS(TableGetCellIOFlagsHandler, gnc_table_model_get_io_flags_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetCellIOFlagsHandler handler = reinterpret_cast<TableGetCellIOFlagsHandler>(
        gnc_table_model_handler_hash_lookup (model->io_flags_handlers,
            cell_name));
    
    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_cell_color_handler,
    (TableModel *model, TableGetCellColorHandler color_handler,
    const char * cell_name),
	(model, color_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->cell_color_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(color_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_cell_color_handler,
    (TableModel *model, TableGetCellColorHandler color_handler),
	(model, color_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->cell_color_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(color_handler));
}
SAFE_C_API_ARGS(TableGetCellColorHandler, gnc_table_model_get_cell_color_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetCellColorHandler handler = reinterpret_cast<TableGetCellColorHandler>(
        gnc_table_model_handler_hash_lookup (model->cell_color_handlers,
            cell_name));
    
    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_cell_border_handler,
    (TableModel *model, TableGetCellBorderHandler cell_border_handler, const char * cell_name),
	(model, cell_border_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->cell_border_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(cell_border_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_cell_border_handler,
    (TableModel *model, TableGetCellBorderHandler cell_border_handler),
	(model, cell_border_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->cell_border_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(cell_border_handler));
}
SAFE_C_API_ARGS(TableGetCellBorderHandler, gnc_table_model_get_cell_border_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableGetCellBorderHandler handler = reinterpret_cast<TableGetCellBorderHandler>(
        gnc_table_model_handler_hash_lookup (model->cell_border_handlers,
            cell_name)); 
    
    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_confirm_handler,
    (TableModel *model, TableConfirmHandler confirm_handler, const char * cell_name),
	(model, confirm_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->confirm_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(confirm_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_default_confirm_handler,
    (TableModel *model, TableConfirmHandler confirm_handler),
	(model, confirm_handler))
{
    g_return_if_fail (model != nullptr);

    gnc_table_model_handler_hash_insert (model->confirm_handlers,
                                         DEFAULT_HANDLER,
                                         reinterpret_cast<gpointer>(confirm_handler));
}
SAFE_C_API_ARGS(TableConfirmHandler, gnc_table_model_get_confirm_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableConfirmHandler handler = reinterpret_cast<TableConfirmHandler>(
        gnc_table_model_handler_hash_lookup (model->confirm_handlers,
            cell_name));
    
    return handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_save_handler,
    (TableModel *model, TableSaveCellHandler save_handler, const char * cell_name),
	(model, save_handler, cell_name))
{
    g_return_if_fail (model != nullptr);
    g_return_if_fail (cell_name != nullptr);

    gnc_table_model_handler_hash_insert (model->save_handlers,
                                         cell_name,
                                         reinterpret_cast<gpointer>(save_handler));
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_pre_save_handler,
    (TableModel *model, TableSaveHandler save_handler),
	(model, save_handler))
{
    g_return_if_fail (model != nullptr);

    model->pre_save_handler = save_handler;
}
SAFE_C_API_VOID_ARGS(gnc_table_model_set_post_save_handler,
    (TableModel *model, TableSaveHandler save_handler),
	(model, save_handler))
{
    g_return_if_fail (model != nullptr);

    model->post_save_handler = save_handler;
}
SAFE_C_API_ARGS(TableSaveCellHandler, gnc_table_model_get_save_handler,
    (TableModel *model, const char * cell_name),
	(model, cell_name))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    TableSaveCellHandler handler = reinterpret_cast<TableSaveCellHandler>(
        gnc_table_model_handler_hash_lookup (model->save_handlers,
            cell_name));
    
    return handler;
}
SAFE_C_API_ARGS(TableSaveHandler, gnc_table_model_get_pre_save_handler,
    (TableModel *model), (model))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    return model->pre_save_handler;
}
SAFE_C_API_ARGS(TableSaveHandler, gnc_table_model_get_post_save_handler,
    (TableModel *model), (model))
{
    g_return_val_if_fail (model != nullptr, nullptr);

    return model->post_save_handler;
}
