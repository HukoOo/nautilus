/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-list-base-private.h"
#include "nautilus-grid-view.h"

#include "nautilus-grid-cell.h"
#include "nautilus-global-preferences.h"

struct _NautilusGridView
{
    NautilusListBase parent_instance;

    GtkGridView *view_ui;

    guint icon_size;

    gboolean directories_first;

    GQuark caption_attributes[NAUTILUS_GRID_CELL_N_CAPTIONS];

    GQuark sort_attribute;
    gboolean reversed;
};

G_DEFINE_TYPE (NautilusGridView, nautilus_grid_view, NAUTILUS_TYPE_LIST_BASE)

#define get_view_item(li) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

static const guint grid_zoom_icon_sizes[] =
{
    NAUTILUS_GRID_ICON_SIZE_EXTRA_SMALL,
    NAUTILUS_GRID_ICON_SIZE_SMALL,
    NAUTILUS_GRID_ICON_SIZE_SMALL_PLUS,
    NAUTILUS_GRID_ICON_SIZE_MEDIUM,
    NAUTILUS_GRID_ICON_SIZE_LARGE,
    NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE,
    NAUTILUS_GRID_ICON_SIZE_HUGE,
    NAUTILUS_GRID_ICON_SIZE_GIGANTIC,
};

static const NautilusViewInfo grid_view_info =
{
    .view_id = NAUTILUS_VIEW_GRID_ID,
    .zoom_level_min = NAUTILUS_GRID_ZOOM_LEVEL_EXTRA_SMALL,
    .zoom_level_max = NAUTILUS_GRID_ZOOM_LEVEL_GIGANTIC,
    .zoom_level_standard = NAUTILUS_GRID_ZOOM_LEVEL_MEDIUM,
    .zoom_icon_sizes = grid_zoom_icon_sizes,
};

static NautilusViewInfo
real_get_view_info (NautilusListBase *list_base)
{
    return grid_view_info;
}

static gint
nautilus_grid_view_sort (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data)
{
    NautilusGridView *self = user_data;
    NautilusFile *file_a;
    NautilusFile *file_b;

    file_a = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) a));
    file_b = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) b));

    return nautilus_file_compare_for_sort_by_attribute_q (file_a, file_b,
                                                          self->sort_attribute,
                                                          self->directories_first,
                                                          self->reversed);
}

static void
update_sort_directories_first (NautilusGridView *self)
{
    NautilusFile *directory_as_file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (self));
    NautilusViewModel *model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    if (nautilus_file_is_in_search (directory_as_file))
    {
        self->directories_first = FALSE;
    }
    else
    {
        self->directories_first = g_settings_get_boolean (gtk_filechooser_preferences,
                                                          NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
    }

    if (model != NULL)
    {
        nautilus_view_model_sort (model);
    }
}

static void
nautilus_grid_view_setup_directory (NautilusListBase  *list_base,
                                    NautilusDirectory *new_directory)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);

    NAUTILUS_LIST_BASE_CLASS (nautilus_grid_view_parent_class)->setup_directory (list_base, new_directory);

    update_sort_directories_first (self);
}

static guint
get_icon_size_for_zoom_level (int zoom_level)
{
    g_return_val_if_fail (zoom_level >= grid_view_info.zoom_level_min &&
                          zoom_level <= grid_view_info.zoom_level_max,
                          NAUTILUS_GRID_ICON_SIZE_MEDIUM);

    return grid_zoom_icon_sizes[zoom_level - grid_view_info.zoom_level_min];
}

/* The largest zoom level not bigger than @icon_size. */
static NautilusGridZoomLevel
get_zoom_level_for_icon_size (guint icon_size)
{
    NautilusGridZoomLevel zoom_level = grid_view_info.zoom_level_min;

    for (int i = grid_view_info.zoom_level_min; i <= grid_view_info.zoom_level_max; i++)
    {
        if (get_icon_size_for_zoom_level (i) <= icon_size)
        {
            zoom_level = i;
        }
    }

    return zoom_level;
}

static gint
get_default_zoom_level (void)
{
    int default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
                                                  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

    /* Sanitize preference value */
    return CLAMP (default_zoom_level,
                  grid_view_info.zoom_level_min,
                  grid_view_info.zoom_level_max);
}

static guint
get_default_icon_size (void)
{
    guint icon_size = g_settings_get_uint (nautilus_icon_view_preferences,
                                           NAUTILUS_PREFERENCES_ICON_VIEW_ICON_SIZE);

    return icon_size != 0 ? icon_size : get_icon_size_for_zoom_level (get_default_zoom_level ());
}

static void
set_captions_from_preferences (NautilusGridView *self)
{
    g_auto (GStrv) value = NULL;
    gint n_captions_for_zoom_level;

    value = g_settings_get_strv (nautilus_icon_view_preferences,
                                 NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

    /* Set a celling on the number of captions depending on the zoom level. */
    n_captions_for_zoom_level = MIN ((uint) MAX ((int) get_zoom_level_for_icon_size (self->icon_size) -
                                                 NAUTILUS_GRID_ZOOM_LEVEL_SMALL + 1,
                                                 0),
                                     G_N_ELEMENTS (self->caption_attributes));

    /* Reset array to zeros beforehand, as we may not refill all elements. */
    memset (&self->caption_attributes, 0, sizeof (self->caption_attributes));
    for (gint i = 0, quark_i = 0;
         value[i] != NULL && quark_i < n_captions_for_zoom_level;
         i++)
    {
        if (g_strcmp0 (value[i], "none") == 0)
        {
            continue;
        }

        /* Convert to quarks in advance, otherwise each NautilusFile attribute
         * getter would call g_quark_from_string() once for each file. */
        self->caption_attributes[quark_i] = g_quark_from_string (value[i]);
        quark_i++;
    }
}

static void
real_set_icon_size (NautilusListBase *list_base,
                    guint             icon_size)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);
    NautilusGridZoomLevel zoom_level;

    icon_size = CLAMP (icon_size,
                       get_icon_size_for_zoom_level (grid_view_info.zoom_level_min),
                       get_icon_size_for_zoom_level (grid_view_info.zoom_level_max));
    zoom_level = get_zoom_level_for_icon_size (icon_size);

    self->icon_size = icon_size;

    if (g_settings_get_uint (nautilus_icon_view_preferences,
                             NAUTILUS_PREFERENCES_ICON_VIEW_ICON_SIZE) != icon_size)
    {
        g_settings_set_uint (nautilus_icon_view_preferences,
                             NAUTILUS_PREFERENCES_ICON_VIEW_ICON_SIZE,
                             icon_size);
    }

    /* Keep the zoom level in sync too, for the sake of upstream Files sharing
     * the same settings. */
    if (g_settings_get_enum (nautilus_icon_view_preferences,
                             NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL) != (int) zoom_level)
    {
        g_settings_set_enum (nautilus_icon_view_preferences,
                             NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                             zoom_level);
    }

    /* The zoom level may change how many captions are allowed. Update it before
     * notifying the icon size change, under the assumption that NautilusGridCell
     * updates captions whenever the icon size is set*/
    set_captions_from_preferences (self);

    g_object_notify (G_OBJECT (self), "icon-size");
}

static void
real_set_zoom_level (NautilusListBase *list_base,
                     int               new_level)
{
    g_return_if_fail (new_level >= grid_view_info.zoom_level_min &&
                      new_level <= grid_view_info.zoom_level_max);

    real_set_icon_size (list_base, get_icon_size_for_zoom_level (new_level));
}

/* The generic implementation in src/nautilus-list-base.c doesn't allow the
 * 2-dimensional movements expected from a grid. Let's hack GTK here. */
static void
real_preview_selection_event (NautilusListBase *list_base,
                              GtkDirectionType  direction)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);
    guint direction_keyval;
    g_autoptr (GtkShortcutTrigger) direction_trigger = NULL;
    g_autoptr (GListModel) controllers = NULL;
    gboolean success = FALSE;

    /* We want the same behavior as when the user presses the arrow keys while
     * the focus is in the view. So, let's get the matching arrow key. */
    switch (direction)
    {
        case GTK_DIR_UP:
        {
            direction_keyval = GDK_KEY_Up;
        }
        break;

        case GTK_DIR_DOWN:
        {
            direction_keyval = GDK_KEY_Down;
        }
        break;

        case GTK_DIR_LEFT:
        {
            direction_keyval = GDK_KEY_Left;
        }
        break;

        case GTK_DIR_RIGHT:
        {
            direction_keyval = GDK_KEY_Right;
        }
        break;

        default:
        {
            g_return_if_reached ();
        }
    }

    /* We cannot simulate a click, but we can find the shortcut it triggers and
     * activate its action programatically.
     *
     * First, we create out would-be trigger.*/
    direction_trigger = gtk_keyval_trigger_new (direction_keyval, 0);

    /* Then we iterate over the shortcut installed in GtkGridView until we find
     * a matching trigger. There may be multiple shortcut controllers, and each
     * shortcut controller may hold multiple shortcuts each. Let's loop. */
    controllers = gtk_widget_observe_controllers (GTK_WIDGET (self->view_ui));
    for (guint i = 0; i < g_list_model_get_n_items (controllers); i++)
    {
        g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);

        if (!GTK_IS_SHORTCUT_CONTROLLER (controller))
        {
            continue;
        }

        for (guint j = 0; j < g_list_model_get_n_items (G_LIST_MODEL (controller)); j++)
        {
            g_autoptr (GtkShortcut) shortcut = g_list_model_get_item (G_LIST_MODEL (controller), j);
            GtkShortcutTrigger *trigger = gtk_shortcut_get_trigger (shortcut);

            if (gtk_shortcut_trigger_equal (trigger, direction_trigger))
            {
                /* Match found. Activate the action to move cursor. */
                success = gtk_shortcut_action_activate (gtk_shortcut_get_action (shortcut),
                                                        0,
                                                        GTK_WIDGET (self->view_ui),
                                                        gtk_shortcut_get_arguments (shortcut));
                break;
            }
        }
    }

    /* If the hack fails (GTK may change it's internal behavior), fallback. */
    if (!success)
    {
        NAUTILUS_LIST_BASE_CLASS (nautilus_grid_view_parent_class)->preview_selection_event (list_base, direction);
    }
}

/* We only care about the keyboard activation part that GtkGridView provides,
 * but we don't need any special filtering here. Indeed, we ask GtkGridView
 * to not activate on single click, and we get to handle double clicks before
 * GtkGridView does (as one of widget subclassing's goal is to modify the parent
 * class's behavior), while claiming the click gestures, so it means GtkGridView
 * will never react to a click event to emit this signal. So we should be pretty
 * safe here with regards to our custom item click handling.
 */
static void
on_grid_view_item_activated (GtkGridView *grid_view,
                             guint        position,
                             gpointer     user_data)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (user_data);

    nautilus_list_base_activate_selection (NAUTILUS_LIST_BASE (self), FALSE);
}

static guint
real_get_icon_size (NautilusListBase *list_base_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    return self->icon_size;
}

static GtkWidget *
real_get_view_ui (NautilusListBase *list_base_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    return GTK_WIDGET (self->view_ui);
}

static int
real_get_zoom_level (NautilusListBase *list_base_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    return get_zoom_level_for_icon_size (self->icon_size);
}

static void
real_scroll_to (NautilusListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    gtk_grid_view_scroll_to (self->view_ui, position, flags, scroll);
}

static GVariant *
real_get_sort_state (NautilusListBase *list_base)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);

    return g_variant_take_ref (g_variant_new ("(sb)",
                                              g_quark_to_string (self->sort_attribute),
                                              self->reversed));
}

static void
real_set_sort_state (NautilusListBase *list_base,
                     GVariant         *value)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);
    const gchar *target_name;
    NautilusViewModel *model = nautilus_list_base_get_model (list_base);
    g_autoptr (GtkCustomSorter) sorter = NULL;

    /* Sort state binding should be set after the model is set.*/
    g_return_if_fail (model != NULL);

    g_variant_get (value, "(&sb)", &target_name, &self->reversed);
    self->sort_attribute = g_quark_from_string (target_name);

    sorter = gtk_custom_sorter_new (nautilus_grid_view_sort, self, NULL);
    nautilus_view_model_set_sorter (model, GTK_SORTER (sorter));
}

static void
on_captions_preferences_changed (NautilusGridView *self)
{
    set_captions_from_preferences (self);

    /* Hack: this relies on the assumption that NautilusGridCell updates
     * captions whenever the icon size is set (even if it's the same value). */
    g_object_notify (G_OBJECT (self), "icon-size");
}

static void
dispose (GObject *object)
{
    G_OBJECT_CLASS (nautilus_grid_view_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_grid_view_parent_class)->finalize (object);
}

static void
bind_cell (GtkSignalListItemFactory *factory,
           GtkListItem              *listitem,
           gpointer                  user_data)
{
    GtkWidget *cell;
    g_autoptr (NautilusViewItem) item = NULL;

    cell = gtk_list_item_get_child (listitem);
    item = get_view_item (listitem);
    g_return_if_fail (item != NULL);

    nautilus_view_item_set_item_ui (item, cell);

    if (nautilus_view_cell_once (NAUTILUS_VIEW_CELL (cell)))
    {
        GtkWidget *parent;

        /* At the time of ::setup emission, the item ui has got no parent yet,
         * that's why we need to complete the widget setup process here, on the
         * first time ::bind is emitted. */
        parent = gtk_widget_get_parent (cell);
        gtk_widget_set_halign (parent, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (parent, GTK_ALIGN_START);
    }
}

static void
unbind_cell (GtkSignalListItemFactory *factory,
             GtkListItem              *listitem,
             gpointer                  user_data)
{
    g_autoptr (NautilusViewItem) item = NULL;

    item = get_view_item (listitem);

    /* item may be NULL when row has just been destroyed. */
    if (item != NULL)
    {
        nautilus_view_item_set_item_ui (item, NULL);
    }
}

static void
setup_cell (GtkSignalListItemFactory *factory,
            GtkListItem              *listitem,
            gpointer                  user_data)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (user_data);
    NautilusGridCell *cell;
    GtkExpression *expression;

    cell = nautilus_grid_cell_new (NAUTILUS_LIST_BASE (self));
    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), NAUTILUS_VIEW_CELL (cell));
    setup_cell_hover (NAUTILUS_VIEW_CELL (cell));

    g_object_bind_property (self, "icon-size",
                            cell, "icon-size",
                            G_BINDING_SYNC_CREATE);

    nautilus_grid_cell_set_caption_attributes (cell, self->caption_attributes);

    /* Use file display name as accessible label. Explaining in pseudo-code:
     * listitem:accessible-name :- listitem:item:item:file:display-name */
    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_VIEW_ITEM, expression, "file");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_FILE, expression, "display-name");
    gtk_expression_bind (expression, listitem, "accessible-label", listitem);
}

static GtkGridView *
create_view_ui (NautilusGridView *self)
{
    GtkListItemFactory *factory;
    GtkWidget *widget;

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_cell), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_cell), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cell), self);

    widget = gtk_grid_view_new (NULL, factory);
    g_object_bind_property (self, "model", widget, "model", G_BINDING_SYNC_CREATE);

    /* We don't use the built-in child activation feature for clicks because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkGridView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_grid_view_set_single_click_activate (GTK_GRID_VIEW (widget), FALSE);
    gtk_grid_view_set_max_columns (GTK_GRID_VIEW (widget), 20);
    gtk_grid_view_set_enable_rubberband (GTK_GRID_VIEW (widget), TRUE);
    gtk_grid_view_set_tab_behavior (GTK_GRID_VIEW (widget), GTK_LIST_TAB_ITEM);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter).
     */
    g_signal_connect (widget, "activate", G_CALLBACK (on_grid_view_item_activated), self);

    return GTK_GRID_VIEW (widget);
}

static void
nautilus_grid_view_class_init (NautilusGridViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->dispose = dispose;
    object_class->finalize = finalize;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_sort_state = real_get_sort_state;
    list_base_view_class->get_view_info = real_get_view_info;
    list_base_view_class->get_view_ui = real_get_view_ui;
    list_base_view_class->get_zoom_level = real_get_zoom_level;
    list_base_view_class->preview_selection_event = real_preview_selection_event;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->set_sort_state = real_set_sort_state;
    list_base_view_class->set_zoom_level = real_set_zoom_level;
    list_base_view_class->set_icon_size = real_set_icon_size;
    list_base_view_class->setup_directory = nautilus_grid_view_setup_directory;
}

static void
nautilus_grid_view_init (NautilusGridView *self)
{
    GtkWidget *scrolled_window = nautilus_list_base_get_scrolled_window (NAUTILUS_LIST_BASE (self));

    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-grid-view");

    set_captions_from_preferences (self);
    g_signal_connect_object (nautilus_icon_view_preferences,
                             "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
                             G_CALLBACK (on_captions_preferences_changed),
                             self,
                             G_CONNECT_SWAPPED);


    self->view_ui = create_view_ui (self);
    nautilus_list_base_setup_gestures (NAUTILUS_LIST_BASE (self));
    nautilus_list_base_setup_background_longpress (NAUTILUS_LIST_BASE (self),
                                                   GTK_WIDGET (self->view_ui));

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   GTK_WIDGET (self->view_ui));

    g_signal_connect_object (gtk_filechooser_preferences,
                             "changed::" NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
                             G_CALLBACK (update_sort_directories_first),
                             self,
                             G_CONNECT_SWAPPED);

    nautilus_list_base_set_icon_size (NAUTILUS_LIST_BASE (self), get_default_icon_size ());
}

NautilusGridView *
nautilus_grid_view_new (void)
{
    return g_object_new (NAUTILUS_TYPE_GRID_VIEW, NULL);
}
