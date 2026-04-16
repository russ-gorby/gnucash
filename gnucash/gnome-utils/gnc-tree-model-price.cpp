/*
 * gnc-tree-model-price.cpp -- GtkTreeModel implementation to display
 *	prices in a GtkTreeView.
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

/*
 * In this model, valid paths take the form "X", "X:Y", or "X:Y:Z", where:
 *   X is an index into the namespaces list held by the commodity db
 *   Y is an index into the commodity list for the namespace
 *   Z is an index into the price list for the commodity
 *
 * Iterators are populated with the following private data:
 *   iter->user_data   Type NAMESPACE | COMMODITY | PRICE
 *   iter->user_data2  A pointer to the namespace|commodity|item structure
 *   iter->user_data3  The index of the item within its parent list
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <cstring>

#include "gnc-component-manager.h"
#include "gnc-engine.h"
#include "gnc-gobject-utils.h"
#include "gnc-pricedb.h"
#include "gnc-tree-model-price.hpp"
#include "gnc-ui-util.h"

#define ITER_IS_NAMESPACE GINT_TO_POINTER(1)
#define ITER_IS_COMMODITY GINT_TO_POINTER(2)
#define ITER_IS_PRICE     GINT_TO_POINTER(3)
constexpr int ITER_IS_NAMESPACE_INT = 1;
constexpr int ITER_IS_COMMODITY_INT = 2;
constexpr int ITER_IS_PRICE_INT = 3;

/*
 * There's a race condition in this code where a redraw of the tree
 * view widget gets in between the two phases of removing a GNCPrice
 * from the model.  I tried bumping the priority of the idle function
 * by 100, which should have put it well above the priority of GTK's
 * redraw function, but the race condition persisted.  I also tried
 * eliminating the second phase of the removal, but that screws up the
 * view filter (which lives above this code and therefore there's no
 * way to access it) and causes other problems.  The workaround is to
 * accept a tree path that points to a nullptr price, and simply return
 * the null string to be printed by the view.  The removal kicks in
 * immediately after the redraw and causes the blank line to be
 * removed.
 *
 * Charles Day: I found that by the time the main loop is reached and
 * the idle timer goes off, many qof events may have been generated and
 * handled. In particular, a commodity could be removed, edited, and
 * re-added by the security editor and all those events would happen
 * before the timer goes off. This caused a problem where a re-added
 * commodity would get whacked when the timer went off. I found that
 * adding a check for pending removals at the beginning of the event
 * handler fixes that problem and also resolves the race condition.
 *
 */
#define RACE_CONDITION_SOLVED

/** Static Globals *******************************************************/
static QofLogModule log_module = GNC_MOD_GUI;

/** Declarations *********************************************************/
static void gnc_tree_model_price_finalize (GObject *object);
static void gnc_tree_model_price_dispose (GObject *object);

static void gnc_tree_model_price_tree_model_init (GtkTreeModelIface *iface);
static GtkTreeModelFlags gnc_tree_model_price_get_flags (GtkTreeModel *tree_model);
static int gnc_tree_model_price_get_n_columns (GtkTreeModel *tree_model);
static GType gnc_tree_model_price_get_column_type (GtkTreeModel *tree_model,
        int index);
static gboolean gnc_tree_model_price_get_iter (GtkTreeModel *tree_model,
        GtkTreeIter *iter,
        GtkTreePath *path);
static GtkTreePath *gnc_tree_model_price_get_path (GtkTreeModel *tree_model,
        GtkTreeIter *iter);
static void gnc_tree_model_price_get_value (GtkTreeModel *tree_model,
        GtkTreeIter *iter,
        int column,
        GValue *value);
static gboolean	gnc_tree_model_price_iter_next (GtkTreeModel *tree_model,
        GtkTreeIter *iter);
static gboolean	gnc_tree_model_price_iter_children (GtkTreeModel *tree_model,
        GtkTreeIter *iter,
        GtkTreeIter *parent);
static gboolean	gnc_tree_model_price_iter_has_child (GtkTreeModel *tree_model,
        GtkTreeIter *iter);
static int gnc_tree_model_price_iter_n_children (GtkTreeModel *tree_model,
        GtkTreeIter *iter);
static gboolean	gnc_tree_model_price_iter_nth_child (GtkTreeModel *tree_model,
        GtkTreeIter *iter,
        GtkTreeIter *parent,
        int n);
static gboolean	gnc_tree_model_price_iter_parent (GtkTreeModel *tree_model,
        GtkTreeIter *iter,
        GtkTreeIter *child);
static void gnc_tree_model_price_event_handler (QofInstance *entity,
        QofEventId event_type,
        gpointer user_data,
        gpointer event_data);

/** The instance data structure for a price tree model. */
struct _GncTreeModelPrice
{
    GncTreeModel gnc_tree_model;    /**< The parent object data. */
    int stamp;                      /**< The state of the model. Any state
                                     *   change increments this number. */
    QofBook *book;
    GNCPriceDB *price_db;
    gint event_handler_id;
    GNCPrintAmountInfo print_info;
};

G_DEFINE_TYPE_WITH_CODE(GncTreeModelPrice, gnc_tree_model_price, GNC_TYPE_TREE_MODEL,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                              gnc_tree_model_price_tree_model_init))

static void
gnc_tree_model_price_class_init (GncTreeModelPriceClass *klass)
{
    GObjectClass *o_class = G_OBJECT_CLASS (klass);

    o_class->finalize = gnc_tree_model_price_finalize;
    o_class->dispose = gnc_tree_model_price_dispose;
}

static void
gnc_tree_model_price_init (GncTreeModelPrice *model)
{
    while (model->stamp == 0)
    {
        model->stamp = g_random_int ();
    }

    model->print_info = gnc_default_price_print_info(nullptr);
}

static void
gnc_tree_model_price_finalize (GObject *object)
{
    GncTreeModelPrice *model;

    ENTER("model %p", object);

    g_return_if_fail (object != nullptr);
    g_return_if_fail (GNC_IS_TREE_MODEL_PRICE (object));

    model = GNC_TREE_MODEL_PRICE (object);

    model->book = nullptr;
    model->price_db = nullptr;

    G_OBJECT_CLASS (gnc_tree_model_price_parent_class)->finalize (object);
    LEAVE(" ");
}

static void
gnc_tree_model_price_dispose (GObject *object)
{
    GncTreeModelPrice *model;

    ENTER("model %p", object);
    g_return_if_fail (object != nullptr);
    g_return_if_fail (GNC_IS_TREE_MODEL_PRICE (object));

    model = GNC_TREE_MODEL_PRICE (object);

    if (model->event_handler_id)
    {
        qof_event_unregister_handler (model->event_handler_id);
        model->event_handler_id = 0;
    }

    G_OBJECT_CLASS (gnc_tree_model_price_parent_class)->dispose (object);
    LEAVE(" ");
}

GtkTreeModel *
gnc_tree_model_price_new (QofBook *book, GNCPriceDB *price_db)
{
    GncTreeModelPrice *model = nullptr;

    ENTER(" ");

    const GList *item = gnc_gobject_tracking_get_list(GNC_TREE_MODEL_PRICE_NAME);
    for ( ; item; item = g_list_next(item))
    {
        model = (GncTreeModelPrice *)item->data;
        if (model->price_db == price_db)
        {
            g_object_ref(G_OBJECT(model));
            LEAVE("returning existing model %p", model);
            return GTK_TREE_MODEL(model);
        }
    }

    model = static_cast<GncTreeModelPrice *>(
        g_object_new (GNC_TYPE_TREE_MODEL_PRICE, nullptr)
    );

    model->book = book;
    model->price_db = price_db;

    model->event_handler_id =
        qof_event_register_handler (gnc_tree_model_price_event_handler, model);

    LEAVE("returning new model %p", model);
    return GTK_TREE_MODEL (model);
}

gboolean
gnc_tree_model_price_iter_is_namespace (GncTreeModelPrice *model,
                                        GtkTreeIter *iter)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);
    g_return_val_if_fail (iter->user_data != nullptr, FALSE);
    g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

    return (iter->user_data == ITER_IS_NAMESPACE);
}

gboolean
gnc_tree_model_price_iter_is_commodity (GncTreeModelPrice *model,
                                        GtkTreeIter *iter)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);
    g_return_val_if_fail (iter->user_data != nullptr, FALSE);
    g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

    return (iter->user_data == ITER_IS_COMMODITY);
}

gboolean
gnc_tree_model_price_iter_is_price (GncTreeModelPrice *model,
                                    GtkTreeIter *iter)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);
    g_return_val_if_fail (iter->user_data != nullptr, FALSE);
    g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

    return (iter->user_data == ITER_IS_PRICE);
}

gnc_commodity_namespace *
gnc_tree_model_price_get_namespace (GncTreeModelPrice *model,
                                    GtkTreeIter *iter)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), nullptr);
    g_return_val_if_fail (iter != nullptr, nullptr);
    g_return_val_if_fail (iter->user_data != nullptr, nullptr);
    g_return_val_if_fail (iter->stamp == model->stamp, nullptr);

    if (iter->user_data != ITER_IS_NAMESPACE)
        return nullptr;
    return (gnc_commodity_namespace *)iter->user_data2;
}

gnc_commodity *
gnc_tree_model_price_get_commodity (GncTreeModelPrice *model,
                                    GtkTreeIter *iter)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), nullptr);
    g_return_val_if_fail (iter != nullptr, nullptr);
    g_return_val_if_fail (iter->user_data != nullptr, nullptr);
    g_return_val_if_fail (iter->stamp == model->stamp, nullptr);

    if (iter->user_data != ITER_IS_COMMODITY)
        return nullptr;
    return (gnc_commodity *)iter->user_data2;
}

GNCPrice *
gnc_tree_model_price_get_price (GncTreeModelPrice *model,
                                GtkTreeIter *iter)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), nullptr);
    g_return_val_if_fail (iter != nullptr, nullptr);
    g_return_val_if_fail (iter->user_data != nullptr, nullptr);
    g_return_val_if_fail (iter->stamp == model->stamp, nullptr);

    if (iter->user_data != ITER_IS_PRICE)
        return nullptr;
    return (GNCPrice *)iter->user_data2;
}

/************************************************************/
/*        Gnc Tree Model Debugging Utility Function         */
/************************************************************/

#define debug_path(fn, path) {					\
    gchar *path_string = gtk_tree_path_to_string(path); 	\
    fn("tree path %s", path_string? path_string : "(nullptr)");	\
    g_free(path_string);					\
  }

#define ITER_STRING_LEN 256

static const gchar *
iter_to_string (GncTreeModelPrice *model, GtkTreeIter *iter)
{
    gnc_commodity_namespace *name_space;
    gnc_commodity *commodity;
    GNCPrice *price;
#ifdef G_THREADS_ENABLED
    static GPrivate gtmits_buffer_key = G_PRIVATE_INIT(g_free);

    auto string = static_cast<gchar *>(g_private_get (&gtmits_buffer_key));
    if (string == nullptr)
    {
        string = static_cast<gchar *>(g_malloc(ITER_STRING_LEN + 1));
        g_private_set (&gtmits_buffer_key, string);
    }
#else
    static char string[ITER_STRING_LEN + 1];
#endif

    if (iter)
    {
        switch (GPOINTER_TO_INT(iter->user_data))
        {
        case ITER_IS_NAMESPACE_INT:
            name_space = (gnc_commodity_namespace *) iter->user_data2;
            snprintf(string, ITER_STRING_LEN,
                     "[stamp:%x data:%d (NAMESPACE), %p (%s), %d]",
                     iter->stamp, GPOINTER_TO_INT(iter->user_data),
                     iter->user_data2, gnc_commodity_namespace_get_name (name_space),
                     GPOINTER_TO_INT(iter->user_data3));
            break;

        case ITER_IS_COMMODITY_INT:
            commodity = (gnc_commodity *) iter->user_data2;
            snprintf(string, ITER_STRING_LEN,
                     "[stamp:%x data:%d (COMMODITY), %p (%s), %d]",
                     iter->stamp, GPOINTER_TO_INT(iter->user_data),
                     iter->user_data2, gnc_commodity_get_mnemonic (commodity),
                     GPOINTER_TO_INT(iter->user_data3));
            break;

        case ITER_IS_PRICE_INT:
            price = (GNCPrice *) iter->user_data2;
            commodity = gnc_price_get_commodity(price);
            snprintf(string, ITER_STRING_LEN,
                     "[stamp:%x data:%d (PRICE), %p (%s:%s), %d]",
                     iter->stamp, GPOINTER_TO_INT(iter->user_data),
                     iter->user_data2, gnc_commodity_get_mnemonic (commodity),
                     xaccPrintAmount (gnc_price_get_value (price), model->print_info),
                     GPOINTER_TO_INT(iter->user_data3));
            break;

        default:
            snprintf(string, ITER_STRING_LEN,
                     "[stamp:%x data:%d (UNKNOWN), %p, %d]",
                     iter->stamp,
                     GPOINTER_TO_INT(iter->user_data),
                     iter->user_data2,
                     GPOINTER_TO_INT(iter->user_data3));
            break;
        }
    }
    return string;
}


/************************************************************/
/*       Gtk Tree Model Required Interface Functions        */
/************************************************************/

static void
gnc_tree_model_price_tree_model_init (GtkTreeModelIface *iface)
{
    iface->get_flags       = gnc_tree_model_price_get_flags;
    iface->get_n_columns   = gnc_tree_model_price_get_n_columns;
    iface->get_column_type = gnc_tree_model_price_get_column_type;
    iface->get_iter        = gnc_tree_model_price_get_iter;
    iface->get_path        = gnc_tree_model_price_get_path;
    iface->get_value       = gnc_tree_model_price_get_value;
    iface->iter_next       = gnc_tree_model_price_iter_next;
    iface->iter_children   = gnc_tree_model_price_iter_children;
    iface->iter_has_child  = gnc_tree_model_price_iter_has_child;
    iface->iter_n_children = gnc_tree_model_price_iter_n_children;
    iface->iter_nth_child  = gnc_tree_model_price_iter_nth_child;
    iface->iter_parent     = gnc_tree_model_price_iter_parent;
}

static GtkTreeModelFlags
gnc_tree_model_price_get_flags (GtkTreeModel *tree_model)
{
    return static_cast<GtkTreeModelFlags>(0);
}

static int
gnc_tree_model_price_get_n_columns (GtkTreeModel *tree_model)
{
    return GNC_TREE_MODEL_PRICE_NUM_COLUMNS;
}

static GType
gnc_tree_model_price_get_column_type (GtkTreeModel *tree_model,
                                      int index)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (tree_model), G_TYPE_INVALID);
    g_return_val_if_fail ((index < GNC_TREE_MODEL_PRICE_NUM_COLUMNS) && (index >= 0), G_TYPE_INVALID);

    switch (index)
    {
    case GNC_TREE_MODEL_PRICE_COL_COMMODITY:
    case GNC_TREE_MODEL_PRICE_COL_CURRENCY:
    case GNC_TREE_MODEL_PRICE_COL_DATE:
    case GNC_TREE_MODEL_PRICE_COL_SOURCE:
    case GNC_TREE_MODEL_PRICE_COL_TYPE:
    case GNC_TREE_MODEL_PRICE_COL_VALUE:
        return G_TYPE_STRING;
    case GNC_TREE_MODEL_PRICE_COL_VISIBILITY:
        return G_TYPE_BOOLEAN;
    default:
        g_assert_not_reached ();
        return G_TYPE_INVALID;
    }
}

static gboolean
gnc_tree_model_price_get_iter (GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               GtkTreePath *path)
{
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (tree_model), FALSE);

    int depth = gtk_tree_path_get_depth (path);
    ENTER("model %p, iter %p, path %p (depth %d)", tree_model, iter, path, depth);
    debug_path(DEBUG, path);

    /* Check the path depth. */
    if (depth == 0)
    {
        LEAVE("depth too small");
        return FALSE;
    }
    if (depth > 3)
    {
        LEAVE("depth too big");
        return FALSE;
    }

    /* Make sure the model has a price db. */
    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    if (model->price_db == nullptr)
    {
        LEAVE("no price db");
        return FALSE;
    }

    /* Verify the first part of the path: the namespace. */
    auto ct = static_cast<gnc_commodity_table *>(
        qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
    );
    GList *ns_list = gnc_commodity_table_get_namespaces_list(ct);
    int i = gtk_tree_path_get_indices (path)[0];
    auto name_space = static_cast<gnc_commodity_namespace *>(
        g_list_nth_data (ns_list, i)
    );
    if (!name_space)
    {
        LEAVE("invalid path at namespace");
        return FALSE;
    }
    g_list_free (ns_list);

    if (depth == 1)
    {
        /* Return an iterator for the namespace. */
        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_NAMESPACE;
        iter->user_data2 = name_space;
        iter->user_data3 = GINT_TO_POINTER(i);
        LEAVE("iter (ns) %s", iter_to_string(model, iter));
        return TRUE;
    }

    /* Verify the second part of the path: the commodity. */
    GList *cm_list = gnc_commodity_namespace_get_commodity_list(name_space);
    i = gtk_tree_path_get_indices (path)[1];
    auto commodity = static_cast<gnc_commodity *>(g_list_nth_data (cm_list, i));
    g_list_free (cm_list);
    if (!commodity)
    {
        LEAVE("invalid path at commodity");
        return FALSE;
    }

    if (depth == 2)
    {
        /* Return an iterator for the commodity. */
        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_COMMODITY;
        iter->user_data2 = commodity;
        iter->user_data3 = GINT_TO_POINTER(i);
        LEAVE("iter (cm) %s", iter_to_string(model, iter));
        return TRUE;
    }

    /* Verify the third part of the path: the price. */
    i = gtk_tree_path_get_indices (path)[2];
    GNCPrice *price = gnc_pricedb_nth_price(model->price_db, commodity, i);
    /* There's a race condition here that I can't resolve.
     * Comment this check out for now, and we'll handle the
     * resulting problem elsewhere. */
#ifdef RACE_CONDITION_SOLVED
    if (!price)
    {
        LEAVE("invalid path at price");
        return FALSE;
    }
#endif

    /* Return an iterator for the price. */
    iter->stamp      = model->stamp;
    iter->user_data  = ITER_IS_PRICE;
    iter->user_data2 = price;
    iter->user_data3 = GINT_TO_POINTER(i);
    LEAVE("iter (pc) %s", iter_to_string(model, iter));
    return TRUE;
}

static GtkTreePath *
gnc_tree_model_price_get_path (GtkTreeModel *tree_model,
                               GtkTreeIter *iter)
{
    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    gnc_commodity_namespace *name_space = nullptr;
    gnc_commodity *commodity = nullptr;
    GtkTreePath *path = nullptr;

    ENTER("model %p, iter %p (%s)", tree_model, iter, iter_to_string(model, iter));
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), nullptr);
    g_return_val_if_fail (iter != nullptr, nullptr);
    g_return_val_if_fail (iter->user_data != nullptr, nullptr);
    g_return_val_if_fail (iter->stamp == model->stamp, nullptr);

    /* Make sure this model has a price db. */
    if (model->price_db == nullptr)
    {
        LEAVE("no price db");
        return FALSE;
    }

    if (iter->user_data == ITER_IS_NAMESPACE)
    {
        /* Create a path to the namespace. This is just the index into
         * the namespace list, which we already stored in user_data3. */
        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, GPOINTER_TO_INT(iter->user_data3));
        debug_path(LEAVE, path);
        return path;
    }

    /* Get the namespaces list. */
    auto ct = static_cast<gnc_commodity_table *>(
        qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
    );
    GList *ns_list = gnc_commodity_table_get_namespaces_list(ct);

    if (iter->user_data == ITER_IS_COMMODITY)
    {
        /* Create a path to the commodity. */
        commodity = (gnc_commodity*)iter->user_data2;
        name_space = gnc_commodity_get_namespace_ds(commodity);
        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, g_list_index (ns_list, name_space));
        gtk_tree_path_append_index (path, GPOINTER_TO_INT(iter->user_data3));
        debug_path(LEAVE, path);
        g_list_free (ns_list);
        return path;
    }

    /* Create a path to the price. */
    commodity = gnc_price_get_commodity((GNCPrice*)iter->user_data2);
    name_space = gnc_commodity_get_namespace_ds(commodity);
    GList *cm_list = gnc_commodity_namespace_get_commodity_list(name_space);
    path = gtk_tree_path_new ();
    gtk_tree_path_append_index (path, g_list_index (ns_list, name_space));
    gtk_tree_path_append_index (path, g_list_index (cm_list, commodity));
    gtk_tree_path_append_index (path, GPOINTER_TO_INT(iter->user_data3));
    debug_path(LEAVE, path);
    g_list_free (cm_list);
    g_list_free (ns_list);
    return path;
}

static void
gnc_tree_model_price_get_value (GtkTreeModel *tree_model,
                                GtkTreeIter *iter,
                                int column,
                                GValue *value)
{
    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    gnc_commodity_namespace *name_space;
    gnc_commodity *commodity;
    GNCPrice *price;
    char datebuff[MAX_DATE_LENGTH + 1];
    memset (datebuff, 0, sizeof(datebuff));

    g_return_if_fail (GNC_IS_TREE_MODEL_PRICE (model));
    g_return_if_fail (iter != nullptr);
#ifdef RACE_CONDITION_SOLVED
    g_return_if_fail (iter->user_data != nullptr);
#endif
    g_return_if_fail (iter->stamp == model->stamp);

    if (iter->user_data == ITER_IS_NAMESPACE)
    {
        name_space = (gnc_commodity_namespace *)iter->user_data2;
        switch (column)
        {
        case GNC_TREE_MODEL_PRICE_COL_COMMODITY:
            g_value_init (value, G_TYPE_STRING);
            g_value_set_string (value, gnc_commodity_namespace_get_gui_name (name_space));
            break;
        case GNC_TREE_MODEL_PRICE_COL_VISIBILITY:
            g_value_init (value, G_TYPE_BOOLEAN);
            g_value_set_boolean (value, FALSE);
            break;
        default:
            g_value_init (value, G_TYPE_STRING);
            g_value_set_string (value, "");
            break;
        }
        return;
    }

    if (iter->user_data == ITER_IS_COMMODITY)
    {
        commodity = (gnc_commodity *)iter->user_data2;
        switch (column)
        {
        case GNC_TREE_MODEL_PRICE_COL_COMMODITY:
            g_value_init (value, G_TYPE_STRING);
            g_value_set_string (value, gnc_commodity_get_printname (commodity));
            break;
        case GNC_TREE_MODEL_PRICE_COL_VISIBILITY:
            g_value_init (value, G_TYPE_BOOLEAN);
            g_value_set_boolean (value, FALSE);
            break;
        default:
            g_value_init (value, G_TYPE_STRING);
            g_value_set_string (value, "");
            break;
        }
        return;
    }

    price = (GNCPrice *)iter->user_data2;
#ifdef RACE_CONDITION_SOLVED
    g_return_if_fail (price != nullptr);
#else
    if (price == nullptr)
    {
        switch (column)
        {
        case GNC_TREE_MODEL_PRICE_COL_COMMODITY:
            g_value_init (value, G_TYPE_STRING);
            g_value_set_string (value, "");
            break;
        case GNC_TREE_MODEL_PRICE_COL_VISIBILITY:
            g_value_init (value, G_TYPE_BOOLEAN);
            g_value_set_boolean (value, FALSE);
            break;
        default:
            g_value_init (value, G_TYPE_STRING);
            g_value_set_string (value, "");
            break;
        }
        return;
    }
#endif

    switch (column)
    {
    case GNC_TREE_MODEL_PRICE_COL_COMMODITY:
        g_value_init (value, G_TYPE_STRING);
        commodity = gnc_price_get_commodity (price);
        g_value_set_string (value, gnc_commodity_get_printname (commodity));
        break;
    case GNC_TREE_MODEL_PRICE_COL_CURRENCY:
        g_value_init (value, G_TYPE_STRING);
        commodity = gnc_price_get_currency (price);
        g_value_set_string (value, gnc_commodity_get_printname (commodity));
        break;
    case GNC_TREE_MODEL_PRICE_COL_DATE:
        qof_print_date_buff (datebuff, MAX_DATE_LENGTH,
                             gnc_price_get_time64 (price));
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, datebuff);
        break;
    case GNC_TREE_MODEL_PRICE_COL_SOURCE:
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, gettext (gnc_price_get_source_string (price)));
        break;
    case GNC_TREE_MODEL_PRICE_COL_TYPE:
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, gnc_price_get_typestr (price));
        break;
    case GNC_TREE_MODEL_PRICE_COL_VALUE:
        g_value_init (value, G_TYPE_STRING);
        g_value_set_string (value, xaccPrintAmount (gnc_price_get_value (price),
                            model->print_info));
        break;
    case GNC_TREE_MODEL_PRICE_COL_VISIBILITY:
        g_value_init (value, G_TYPE_BOOLEAN);
        g_value_set_boolean (value, TRUE);
        break;
    default:
        g_assert_not_reached ();
    }
}

static gboolean
gnc_tree_model_price_iter_next (GtkTreeModel *tree_model,
                                GtkTreeIter *iter)
{
    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    GList *list = nullptr;
    int n;

    ENTER("model %p, iter %p(%s)", tree_model, iter, iter_to_string(model, iter));
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);
    g_return_val_if_fail (iter->user_data != nullptr, FALSE);
    g_return_val_if_fail (iter->stamp == model->stamp, FALSE);

    if (iter->user_data == ITER_IS_NAMESPACE)
    {
        auto ct = static_cast<gnc_commodity_table *>(
            qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
        );
        list = gnc_commodity_table_get_namespaces_list(ct);
        n = GPOINTER_TO_INT(iter->user_data3) + 1;
        iter->user_data2 = g_list_nth_data(list, n);
        g_list_free (list);
        if (iter->user_data2 == nullptr)
        {
            LEAVE("no next iter");
            return FALSE;
        }
        iter->user_data3 = GINT_TO_POINTER(n);
        LEAVE("iter %p(%s)", iter, iter_to_string(model, iter));
        return TRUE;
    }
    else if (iter->user_data == ITER_IS_COMMODITY)
    {
        gnc_commodity_namespace *name_space = gnc_commodity_get_namespace_ds((gnc_commodity *)iter->user_data2);
        list = gnc_commodity_namespace_get_commodity_list(name_space);
        n = GPOINTER_TO_INT(iter->user_data3) + 1;
        iter->user_data2 = g_list_nth_data(list, n);
        g_list_free (list);
        if (iter->user_data2 == nullptr)
        {
            LEAVE("no next iter");
            return FALSE;
        }
        iter->user_data3 = GINT_TO_POINTER(n);
        LEAVE("iter %p(%s)", iter, iter_to_string(model, iter));
        return TRUE;
    }
    else if (iter->user_data == ITER_IS_PRICE)
    {
        gnc_commodity *commodity = gnc_price_get_commodity((GNCPrice*)iter->user_data2);
        n = GPOINTER_TO_INT(iter->user_data3) + 1;
        iter->user_data2 = gnc_pricedb_nth_price(model->price_db, commodity, n);
        if (iter->user_data2 == nullptr)
        {
            LEAVE("no next iter");
            return FALSE;
        }
        iter->user_data3 = GINT_TO_POINTER(n);
        LEAVE("iter %p(%s)", iter, iter_to_string(model, iter));
        return TRUE;
    }
    else
    {
        LEAVE("unknown iter type");
        return FALSE;
    }
}

static gboolean
gnc_tree_model_price_iter_children (GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    GtkTreeIter *parent)
{
    GList *list = nullptr;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (tree_model), FALSE);

    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    ENTER("model %p, iter %p, parent %p (%s)",
          tree_model, iter, parent, iter_to_string(model, parent));

    if (parent == nullptr)
    {
        auto ct = static_cast<gnc_commodity_table *>(
            qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
        );
        list = gnc_commodity_table_get_namespaces_list(ct);
        if (list == nullptr)
        {
            LEAVE("no namespaces");
            return FALSE;
        }

        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_NAMESPACE;
        iter->user_data2 = g_list_nth_data(list, 0);
        iter->user_data3 = GINT_TO_POINTER(0);
        LEAVE("ns iter %p (%s)", iter, iter_to_string(model, iter));
        g_list_free (list);
        return TRUE;
    }

    if (parent->user_data == ITER_IS_NAMESPACE)
    {
        gnc_commodity_namespace *name_space = (gnc_commodity_namespace *)parent->user_data2;
        list = gnc_commodity_namespace_get_commodity_list(name_space);
        if (list == nullptr)
        {
            LEAVE("no commodities");
            return FALSE;
        }

        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_COMMODITY;
        iter->user_data2 = g_list_nth_data(list, 0);
        iter->user_data3 = GINT_TO_POINTER(0);
        LEAVE("cm iter %p (%s)", iter, iter_to_string(model, iter));
        g_list_free (list);
        return TRUE;
    }

    if (parent->user_data == ITER_IS_COMMODITY)
    {
        GNCPrice *price;
        gnc_commodity *commodity = (gnc_commodity *)parent->user_data2;
        price = gnc_pricedb_nth_price(model->price_db, commodity, 0);
        if (price == nullptr)
        {
            LEAVE("no prices");
            return FALSE;
        }
        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_PRICE;
        iter->user_data2 = price;
        iter->user_data3 = GINT_TO_POINTER(0);
        LEAVE("price iter %p (%s)", iter, iter_to_string(model, iter));
        return TRUE;
    }

    LEAVE("FALSE");
    return FALSE;
}

static gboolean
gnc_tree_model_price_iter_has_child (GtkTreeModel *tree_model,
                                     GtkTreeIter *iter)
{
    GncTreeModelPrice *model;
    gnc_commodity_namespace *name_space;
    gnc_commodity *commodity;
    gboolean result;
    GList *list;

    model = GNC_TREE_MODEL_PRICE (tree_model);
    ENTER("model %p, iter %p (%s)", tree_model,
          iter, iter_to_string(model, iter));
    g_return_val_if_fail (tree_model != nullptr, FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);

    if (iter->user_data == ITER_IS_PRICE)
    {
        LEAVE("price has no children");
        return FALSE;
    }

    if (iter->user_data == ITER_IS_NAMESPACE)
    {
        name_space = (gnc_commodity_namespace *)iter->user_data2;
        list = gnc_commodity_namespace_get_commodity_list(name_space);
        LEAVE("%s children", list ? "has" : "no");
        gboolean rv = (list != nullptr);
        g_list_free (list);
        return rv;
    }

    if (iter->user_data == ITER_IS_COMMODITY)
    {
        commodity = (gnc_commodity *)iter->user_data2;
        result = gnc_pricedb_has_prices(model->price_db, commodity, nullptr);
        LEAVE("%s children", result ? "has" : "no");
        return result;
    }

    LEAVE("no children (unknown type)");
    return FALSE;
}

static int
gnc_tree_model_price_iter_n_children (GtkTreeModel *tree_model,
                                      GtkTreeIter *iter)
{
    GList *list = nullptr;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (tree_model), -1);

    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    ENTER("model %p, iter %p (%s)", tree_model, iter,
          iter_to_string(model, iter));

    if (iter == nullptr)
    {
        auto ct = static_cast<gnc_commodity_table *>(
            qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
        );
        list = gnc_commodity_table_get_namespaces_list(ct);
        LEAVE("ns list length %d", g_list_length(list));
        guint rv = g_list_length (list);
        g_list_free (list);
        return rv;
    }

    if (iter->user_data == ITER_IS_NAMESPACE)
    {
        gnc_commodity_namespace *name_space = (gnc_commodity_namespace *)iter->user_data2;
        list = gnc_commodity_namespace_get_commodity_list(name_space);
        LEAVE("cm list length %d", g_list_length(list));
        guint rv = g_list_length (list);
        g_list_free (list);
        return rv;
    }

    if (iter->user_data == ITER_IS_COMMODITY)
    {
        gnc_commodity *commodity = (gnc_commodity *)iter->user_data2;
        int n = gnc_pricedb_num_prices(model->price_db, commodity);
        LEAVE("price list length %d", n);
        return n;
    }

    LEAVE("0");
    return 0;
}

static gboolean
gnc_tree_model_price_iter_nth_child (GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     GtkTreeIter *parent,
                                     int n)
{
    GList *list = nullptr;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (tree_model), FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);

    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    ENTER("model %p, iter %p, parent %p (%s), n %d",
          tree_model, iter, parent, iter_to_string(model, parent), n);

    if (parent == nullptr)
    {
        auto ct = static_cast<gnc_commodity_table *>(
            qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
        );
        list = gnc_commodity_table_get_namespaces_list(ct);

        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_NAMESPACE;
        iter->user_data2 = g_list_nth_data(list, n);
        iter->user_data3 = GINT_TO_POINTER(n);
        LEAVE("ns iter %p (%s)", iter, iter_to_string(model, iter));
        g_list_free (list);
        return iter->user_data2 != nullptr;
    }

    if (parent->user_data == ITER_IS_NAMESPACE)
    {
        gnc_commodity_namespace *name_space = (gnc_commodity_namespace *)parent->user_data2;
        list = gnc_commodity_namespace_get_commodity_list(name_space);

        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_COMMODITY;
        iter->user_data2 = g_list_nth_data(list, n);
        iter->user_data3 = GINT_TO_POINTER(n);
        LEAVE("cm iter %p (%s)", iter, iter_to_string(model, iter));
        g_list_free (list);
        return iter->user_data2 != nullptr;
    }

    if (parent->user_data == ITER_IS_COMMODITY)
    {
        gnc_commodity *commodity = (gnc_commodity *)parent->user_data2;

        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_PRICE;
        iter->user_data2 = gnc_pricedb_nth_price(model->price_db, commodity, n);
        iter->user_data3 = GINT_TO_POINTER(n);
        LEAVE("price iter %p (%s)", iter, iter_to_string(model, iter));
        return iter->user_data2 != nullptr;
    }

    iter->stamp = 0;
    LEAVE("FALSE");
    return FALSE;
}

static gboolean
gnc_tree_model_price_iter_parent (GtkTreeModel *tree_model,
                                  GtkTreeIter *iter,
                                  GtkTreeIter *child)
{
    gnc_commodity_namespace *name_space = nullptr;
    GList *list = nullptr;

    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (tree_model), FALSE);
    g_return_val_if_fail (iter != nullptr, FALSE);
    g_return_val_if_fail (child != nullptr, FALSE);

    GncTreeModelPrice *model = GNC_TREE_MODEL_PRICE (tree_model);
    ENTER("model %p, iter %p, child %p (%s)",
          tree_model, iter, child, iter_to_string(model, child));

    if (child->user_data == ITER_IS_NAMESPACE)
    {
        LEAVE("ns has no parent");
        return FALSE;
    }

    if (child->user_data == ITER_IS_COMMODITY)
    {
        auto ct = static_cast<gnc_commodity_table *>(
            qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
        );
        list = gnc_commodity_table_get_namespaces_list(ct);
        name_space = gnc_commodity_get_namespace_ds((gnc_commodity*)child->user_data2);

        iter->stamp      = model->stamp;
        iter->user_data  = ITER_IS_NAMESPACE;
        iter->user_data2 = name_space;
        iter->user_data3 = GINT_TO_POINTER(g_list_index(list, name_space));
        LEAVE("ns iter %p (%s)", iter, iter_to_string(model, iter));
        g_list_free (list);
        return TRUE;
    }

    gnc_commodity *commodity = gnc_price_get_commodity ((GNCPrice*)child->user_data2);
    name_space = gnc_commodity_get_namespace_ds(commodity);
    list = gnc_commodity_namespace_get_commodity_list(name_space);

    iter->stamp      = model->stamp;
    iter->user_data  = ITER_IS_COMMODITY;
    iter->user_data2 = commodity;
    iter->user_data3 = GINT_TO_POINTER(g_list_index(list, commodity));
    LEAVE("cm iter %p (%s)", iter, iter_to_string(model, iter));
    g_list_free (list);
    return TRUE;
}

/************************************************************/
/*                Price Tree View Functions                 */
/************************************************************/

/*
 * Convert a model/price pair into a gtk_tree_model_iter.  This
 * routine should only be called from the file
 * gnc-tree-view-price.cpp.
 */
gboolean
gnc_tree_model_price_get_iter_from_price (GncTreeModelPrice *model,
        GNCPrice *price,
        GtkTreeIter *iter)
{
    gnc_commodity *commodity;
    GList *list;
    gint n;

    ENTER("model %p, price %p, iter %p", model, price, iter);
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail ((price != nullptr), FALSE);
    g_return_val_if_fail ((iter != nullptr), FALSE);

    commodity = gnc_price_get_commodity(price);
    if (commodity == nullptr)
    {
        LEAVE("no commodity");
        return FALSE;
    }

    list = gnc_pricedb_get_prices(model->price_db, commodity, nullptr);
    if (list == nullptr)
    {
        LEAVE("empty list");
        return FALSE;
    }

    n = g_list_index(list, price);
    if (n == -1)
    {
        gnc_price_list_destroy(list);
        LEAVE("not in list");
        return FALSE;
    }

    iter->stamp = model->stamp;
    iter->user_data  = ITER_IS_PRICE;
    iter->user_data2 = price;
    iter->user_data3 = GINT_TO_POINTER(n);
    gnc_price_list_destroy(list);
    LEAVE("iter %s", iter_to_string(model, iter));
    return TRUE;
}

/*
 * Convert a model/price pair into a gtk_tree_model_path.  This
 * routine should only be called from the file
 * gnc-tree-view-price.cpp.
 */
GtkTreePath *
gnc_tree_model_price_get_path_from_price (GncTreeModelPrice *model,
        GNCPrice *price)
{
    GtkTreeIter tree_iter;
    GtkTreePath *tree_path;

    ENTER("model %p, price %p", model, price);
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), nullptr);
    g_return_val_if_fail (price != nullptr, nullptr);

    if (!gnc_tree_model_price_get_iter_from_price (model, price, &tree_iter))
    {
        LEAVE("no iter");
        return nullptr;
    }

    tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL(model), &tree_iter);
    if (tree_path)
    {
        gchar *path_string = gtk_tree_path_to_string(tree_path);
        LEAVE("path (2) %s", path_string);
        g_free(path_string);
    }
    else
    {
        LEAVE("no path");
    }
    return tree_path;
}

/*
 * Convert a model/commodity pair into a gtk_tree_model_iter.  This
 * routine should only be called from the file
 * gnc-tree-view-price.cpp.
 */
gboolean
gnc_tree_model_price_get_iter_from_commodity (GncTreeModelPrice *model,
        gnc_commodity *commodity,
        GtkTreeIter *iter)
{
    gnc_commodity_namespace *name_space;
    GList *list;
    gint n;

    ENTER("model %p, commodity %p, iter %p", model, commodity, iter);
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail ((commodity != nullptr), FALSE);
    g_return_val_if_fail ((iter != nullptr), FALSE);

    name_space = gnc_commodity_get_namespace_ds(commodity);
    if (name_space == nullptr)
    {
        LEAVE("no namespace");
        return FALSE;
    }

    list = gnc_commodity_namespace_get_commodity_list(name_space);
    if (list == nullptr)
    {
        LEAVE("empty list");
        return FALSE;
    }

    n = g_list_index(list, commodity);
    g_list_free (list);
    if (n == -1)
    {
        LEAVE("commodity not in list");
        return FALSE;
    }

    iter->stamp = model->stamp;
    iter->user_data  = ITER_IS_COMMODITY;
    iter->user_data2 = commodity;
    iter->user_data3 = GINT_TO_POINTER(n);
    LEAVE("iter %s", iter_to_string(model, iter));
    return TRUE;
}

/*
 * Convert a model/namespace pair into a gtk_tree_model_iter.  This
 * routine should only be called from the file
 * gnc-tree-view-price.cpp.
 */
gboolean
gnc_tree_model_price_get_iter_from_namespace (GncTreeModelPrice *model,
        gnc_commodity_namespace *name_space,
        GtkTreeIter *iter)
{
    ENTER("model %p, namespace %p, iter %p", model, name_space, iter);
    g_return_val_if_fail (GNC_IS_TREE_MODEL_PRICE (model), FALSE);
    g_return_val_if_fail ((name_space != nullptr), FALSE);
    g_return_val_if_fail ((iter != nullptr), FALSE);

    auto ct = static_cast<gnc_commodity_table *>(
        qof_book_get_data (model->book, GNC_COMMODITY_TABLE)
    );
    GList *list = gnc_commodity_table_get_namespaces_list(ct);
    if (list == nullptr)
    {
        LEAVE("namespace list empty");
        return FALSE;
    }

    int n = g_list_index(list, name_space);
    g_list_free (list);
    if (n == -1)
    {
        LEAVE("namespace not found");
        return FALSE;
    }

    iter->stamp = model->stamp;
    iter->user_data  = ITER_IS_NAMESPACE;
    iter->user_data2 = name_space;
    iter->user_data3 = GINT_TO_POINTER(n);
    LEAVE("iter %s", iter_to_string(model, iter));
    return TRUE;
}

/************************************************************/
/*    Price Tree Model - Engine Event Handling Functions    */
/************************************************************/

struct remove_data
{
    GncTreeModelPrice *model;
    GtkTreePath       *path;
};

static GSList *pending_removals = nullptr;

/** This function updates the model when a row is being added.
 *  The immediate parent needs to be tapped on the shoulder so that it
 *  can correctly update the disclosure triangle (if this is its first
 *  child.)
 *
 *  @internal
 *
 *  @param model The price tree model.
 *
 *  @param iter The iterator of the new row.
 */
static void
gnc_tree_model_price_row_add (GncTreeModelPrice *model,
                              GtkTreeIter *iter)
{
    GtkTreePath *path;
    GtkTreeModel *tree_model;
    GtkTreeIter tmp_iter;

    ENTER("model %p, iter (%p)%s", model, iter, iter_to_string(model, iter));

    /* We're adding a row, so the lists on which this model is based have
     * changed. Since existing iterators (except the one just passed in)
     * are all based on old indexes into those lists, we need to invalidate
     * them, which we can do by changing the model's stamp. */
    do
    {
        model->stamp++;
    }
    while (model->stamp == 0);
    iter->stamp = model->stamp;

    /* Tag the new row as inserted. */
    tree_model = GTK_TREE_MODEL(model);
    path = gnc_tree_model_price_get_path (tree_model, iter);
    gtk_tree_model_row_inserted (tree_model, path, iter);

    /* Inform all ancestors. */
    /*
     * Charles Day: I don't think calls to gtk_tree_model_row_changed() should
     * be necessary. It is just a workaround for bug #540201.
     */
    if (gtk_tree_path_up(path) &&
            gtk_tree_path_get_depth(path) > 0 &&
            gtk_tree_model_get_iter(tree_model, &tmp_iter, path))
    {
        /* Signal the change to the parent. */
        gtk_tree_model_row_changed(tree_model, path, &tmp_iter);

        /* Is this the parent's first child? */
        if (gtk_tree_model_iter_n_children(tree_model, &tmp_iter) == 1)
            gtk_tree_model_row_has_child_toggled(tree_model, path, &tmp_iter);

        /* Signal any other ancestors. */
        while (gtk_tree_path_up(path) &&
                gtk_tree_path_get_depth(path) > 0 &&
                gtk_tree_model_get_iter(tree_model, &tmp_iter, path))
        {
            gtk_tree_model_row_changed(tree_model, path, &tmp_iter);
        }
    }
    gtk_tree_path_free(path);

    /* If the new row already has children, signal that so the expander
     * can be shown. This can happen, for example, if a namespace or
     * commodity is changed in another place (like the Security Editor)
     * and gets removed and then re-added to the commodity db. */
    if (gnc_tree_model_price_iter_has_child(tree_model, iter))
    {
        path = gnc_tree_model_price_get_path(tree_model, iter);
        gtk_tree_model_row_has_child_toggled(tree_model, path, iter);
        gtk_tree_path_free(path);
    }

    LEAVE(" ");
}

/** This function updates the model when a row is being deleted.
 *  The immediate parent needs to be tapped on the shoulder so that it
 *  can correctly update the disclosure triangle (if this was its last
 *  child.)
 *
 *  @internal
 *
 *  @param model The price tree model that is losing a row.
 *
 *  @param path The path of the row being removed.
 */
static void
gnc_tree_model_price_row_delete (GncTreeModelPrice *model,
                                 GtkTreePath *path)
{
    GtkTreeModel *tree_model;
    GtkTreeIter iter;

    g_return_if_fail(GNC_IS_TREE_MODEL_PRICE(model));
    g_return_if_fail(path);

    debug_path(ENTER, path);

    tree_model = GTK_TREE_MODEL(model);

    /* We're removing a row, so the lists on which this model is based have
     * changed. Since existing iterators are all based on old indexes into
     * those lists, we need to invalidate them, which we can do by changing
     * the model's stamp. */
    do
    {
        model->stamp++;
    }
    while (model->stamp == 0);

    /* Signal that the path has been deleted. */
    gtk_tree_model_row_deleted(tree_model, path);

    /* Issue any appropriate signals to ancestors. */
    if (gtk_tree_path_up(path) &&
            gtk_tree_path_get_depth(path) > 0 &&
            gtk_tree_model_get_iter(tree_model, &iter, path))
    {
        DEBUG("iter %s", iter_to_string(model, &iter));

        /* Signal the change to the parent. */
        gtk_tree_model_row_changed(tree_model, path, &iter);

        /* Was this the parent's only child? */
        if (!gtk_tree_model_iter_has_child(tree_model, &iter))
            gtk_tree_model_row_has_child_toggled(tree_model, path, &iter);

        /* Signal any other ancestors. */
        while (gtk_tree_path_up(path) &&
                gtk_tree_path_get_depth(path) > 0 &&
                gtk_tree_model_get_iter(tree_model, &iter, path))
        {
            DEBUG("iter %s", iter_to_string(model, &iter));
            gtk_tree_model_row_changed(tree_model, path, &iter);
        }
    }

    LEAVE(" ");
}


/** This function is a one-shot helper routine for the following
 *  gnc_tree_model_price_event_handler() function.  It must be armed
 *  each time an item is removed from the model.  This function will
 *  be called as an idle function some time after the user requests
 *  the deletion.  (Most likely when the event handler for the "OK"
 *  button click returns.)  This function will send the "row_deleted"
 *  signal to the model for each entry deleted.
 *
 *  @internal
 *
 *  @param unused
 *
 *  @return FALSE.  Tells the glib idle function to remove this
 *  handler, making it a one-shot that will be re-armed at the next
 *  item removal.
 */
static gboolean
gnc_tree_model_price_do_deletions (gpointer price_db)
{
    ENTER(" ");

    /* Go through the list of paths needing removal. */
    while (pending_removals)
    {
        auto data = static_cast<remove_data *>(pending_removals->data);
        pending_removals = g_slist_delete_link(pending_removals, pending_removals);

        if (data)
        {
            debug_path(DEBUG, data->path);

            /* Remove the path. */
            gnc_tree_model_price_row_delete(data->model, data->path);
            gnc_pricedb_nth_price_reset_cache (static_cast<GNCPriceDB *>(price_db));

            gtk_tree_path_free(data->path);
            g_free(data);
        }
    }

    LEAVE(" ");
    /* Don't call me again. */
    return FALSE;
}


/** This function is the handler for all event messages from the engine.
 *  Its purpose is to update the tree model any time a price, commodity, or
 *  namespace is added to the engine, modified, or deleted from the engine.
 *  This change to the model is then propagated to any/all overlying filters
 *  and views. This function listens to the ADD, REMOVE, MODIFY, and DESTROY
 *  events.
 *
 *  @internal
 *
 *  @warning There is a "Catch 22" situation here. The REMOVE event
 *  indicates that a namespace, commodity, or price is *about* to be
 *  removed. But we can't actually delete the row by calling
 *  gtk_tree_model_row_deleted() until after the item has *actually*
 *  been removed from the real model (which is the engine's commodity
 *  and price db's for us). But once the item has been deleted
 *  from the engine we have no way to determine the path to pass to
 *  row_deleted().  So we will save the path now, then call a function
 *  to delete the row when we receive the next event or we're idle
 *  (whichever comes first.) This is a PITA, but the only other choice
 *  is to have this model mirror the engine's price table instead of
 *  referencing it directly.
 *
 *  @param entity The affected item.
 *
 *  @param event type The type of the event. This function only cares
 *  about items of type ADD, REMOVE, MODIFY, and DESTROY.
 *
 *  @param user_data A pointer to the tree model.
 *
 *  @param event_data A pointer to additional data about this event.
 */
static void
gnc_tree_model_price_event_handler (QofInstance *entity,
                                    QofEventId event_type,
                                    gpointer user_data,
                                    gpointer event_data)
{
    GncTreeModelPrice *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    remove_data *data;
    const gchar *name;

    ENTER("entity %p, event %d, model %p, event data %p",
          entity, event_type, user_data, event_data);
    model = (GncTreeModelPrice *)user_data;

    /* Do deletions if any are pending. */
    if (pending_removals)
        gnc_tree_model_price_do_deletions (model->price_db);

    /* hard failures */
    g_return_if_fail(GNC_IS_TREE_MODEL_PRICE(model));

    /* get type specific data */
    if (GNC_IS_COMMODITY(entity))
    {
        gnc_commodity *commodity;

        commodity = GNC_COMMODITY(entity);
        name = gnc_commodity_get_mnemonic(commodity);
        if (event_type != QOF_EVENT_DESTROY)
        {
            if (!gnc_tree_model_price_get_iter_from_commodity (model, commodity, &iter))
            {
                LEAVE("no iter");
                return;
            }
        }
    }
    else if (GNC_IS_COMMODITY_NAMESPACE(entity))
    {
        gnc_commodity_namespace *name_space;

        name_space = GNC_COMMODITY_NAMESPACE(entity);
        name = gnc_commodity_namespace_get_name(name_space);
        if (event_type != QOF_EVENT_DESTROY)
        {
            if (!gnc_tree_model_price_get_iter_from_namespace (model, name_space, &iter))
            {
                LEAVE("no iter");
                return;
            }
        }
    }
    else if (GNC_IS_PRICE(entity))
    {
        GNCPrice *price;

        price = GNC_PRICE(entity);
        name = "price";
        if (event_type != QOF_EVENT_DESTROY)
        {
            if (!gnc_tree_model_price_get_iter_from_price (model, price, &iter))
            {
                LEAVE("no iter");
                return;
            }
        }
    }
    else
    {
        LEAVE(" ");
        return;
    }

    switch (event_type)
    {
    case QOF_EVENT_ADD:
        /* Tell the filters/views where the new price was added. */
        DEBUG("add %s", name);
        gnc_pricedb_nth_price_reset_cache (model->price_db);
        gnc_tree_model_price_row_add (model, &iter);
        break;

    case QOF_EVENT_REMOVE:
        /* Record the path of this account for later use in destruction */
        DEBUG("remove %s", name);
        path = gtk_tree_model_get_path (GTK_TREE_MODEL(model), &iter);
        if (path == nullptr)
        {
            LEAVE("not in model");
            return;
        }

        data = g_new0 (remove_data, 1);
        data->model = model;
        data->path = path;
        pending_removals = g_slist_append (pending_removals, data);
        g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                        gnc_tree_model_price_do_deletions, model->price_db, nullptr);

        LEAVE(" ");
        return;

    case QOF_EVENT_MODIFY:
        DEBUG("change %s", name);
        path = gtk_tree_model_get_path (GTK_TREE_MODEL(model), &iter);
        if (path == nullptr)
        {
            LEAVE("not in model");
            return;
        }
        if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(model), &iter, path))
        {
            gtk_tree_path_free(path);
            LEAVE("can't find iter for path");
            return;
        }
        gtk_tree_model_row_changed(GTK_TREE_MODEL(model), path, &iter);
        gtk_tree_path_free(path);
        LEAVE(" ");
        return;

    default:
        LEAVE("ignored event for %s", name);
        return;
    }
    LEAVE(" new stamp %u", model->stamp);
}
