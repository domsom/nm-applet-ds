#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include <nm-setting-resources.h>
#include <nm-utils.h>

#include "page-resources.h"

G_DEFINE_TYPE (CEPageResources, ce_page_resources, CE_TYPE_PAGE)

#define CE_PAGE_RESOURCES_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_RESOURCES, CEPageResourcesPrivate))

#define COL_SCHEME 0
#define COL_HOST 1
#define COL_FOLDER 2
#define COL_LAST COL_FOLDER

typedef struct {
	NMSettingResources *setting;

	GtkButton *network_drive_add;
	GtkButton *network_drive_delete;
	GtkTreeView *network_drives_list;
	GtkCellRenderer *network_drives_cells[COL_LAST + 1];

	/* Cached tree view entry for editing-canceled */
	/* Used also for saving old value when switching between cells via mouse
	 * clicks - GTK3 produces neither editing-canceled nor editing-done for
	 * that :( */
	char *last_edited; /* cell text */
	char *last_path;   /* row in treeview */
	int last_column;   /* column in treeview */
} CEPageResourcesPrivate;

static void
resources_private_init (CEPageResources *self)
{
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->network_drive_add = GTK_BUTTON (GTK_WIDGET (gtk_builder_get_object (builder, "network_drives_add_button")));
	priv->network_drive_delete = GTK_BUTTON (GTK_WIDGET (gtk_builder_get_object (builder, "network_drives_delete_button")));
	priv->network_drives_list = GTK_TREE_VIEW (GTK_WIDGET (gtk_builder_get_object (builder, "network_drives")));
}

static void
populate_ui (CEPageResources *self)
{
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);
	NMSettingResources *setting = priv->setting;
	GtkListStore *store;
	GtkTreeIter model_iter;
	int i;

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	for (i = 0; i < nm_setting_resources_get_num_network_drives (setting); i++) {
		const char *network_drive = nm_setting_resources_get_network_drive (setting, i);
		char *scheme, *host, *folder;

		if (!nm_utils_parse_mount_uri(network_drive, &scheme, &host, &folder)) {
			g_warning ("%s: invalid network drive uri!", __func__);
			continue;
		}

		gtk_list_store_append (store, &model_iter);
		gtk_list_store_set (store, &model_iter, COL_SCHEME, scheme, -1);
		gtk_list_store_set (store, &model_iter, COL_HOST, host, -1);
		gtk_list_store_set (store, &model_iter, COL_FOLDER, folder, -1);
	}

	gtk_tree_view_set_model (priv->network_drives_list, GTK_TREE_MODEL (store));
	g_signal_connect_swapped (store, "row-inserted", G_CALLBACK (ce_page_changed), self);
	g_signal_connect_swapped (store, "row-deleted", G_CALLBACK (ce_page_changed), self);
	g_object_unref (store);
}

static void
network_drive_add_clicked (GtkButton *button, gpointer user_data)
{
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (user_data);
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GList *cells;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->network_drives_list));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, COL_SCHEME, "", -1);

	selection = gtk_tree_view_get_selection (priv->network_drives_list);
	gtk_tree_selection_select_iter (selection, &iter);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
	column = gtk_tree_view_get_column (priv->network_drives_list, COL_SCHEME);

	/* FIXME: using cells->data is pretty fragile but GTK apparently doesn't
	 * have a way to get a cell renderer from a column based on path or iter
	 * or whatever.
	 */
	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	gtk_tree_view_set_cursor_on_cell (priv->network_drives_list, path, column, cells->data, TRUE);

	g_list_free (cells);
	gtk_tree_path_free (path);
}

static void
network_drive_delete_clicked (GtkButton *button, gpointer user_data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW (user_data);
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	int num_rows;

	selection = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_count_selected_rows (selection) != 1)
		return;

	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (!selected_rows)
		return;

	if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) selected_rows->data))
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	g_list_foreach (selected_rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_rows);

	num_rows = gtk_tree_model_iter_n_children (model, NULL);
	if (num_rows && gtk_tree_model_iter_nth_child (model, &iter, NULL, num_rows - 1)) {
		selection = gtk_tree_view_get_selection (treeview);
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void
list_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GtkWidget *button = GTK_WIDGET (user_data);
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		gtk_widget_set_sensitive (button, TRUE);
	else
		gtk_widget_set_sensitive (button, FALSE);
}

static void
cell_editing_canceled (GtkCellRenderer *renderer, gpointer user_data)
{
	CEPageResources *self;
	CEPageResourcesPrivate *priv;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	guint32 column;

	// user_data disposed?
	if (GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), "ce-page-not-valid")))
		return;

	self = CE_PAGE_RESOURCES (user_data);
	priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	if (priv->last_edited) {
		selection = gtk_tree_view_get_selection (priv->network_drives_list);
		if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
			column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (renderer), "column"));
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, priv->last_edited, -1);
		}

		g_free (priv->last_edited);
		priv->last_edited = NULL;

		ce_page_changed (CE_PAGE (self));
	}

	g_free (priv->last_path);
	priv->last_path = NULL;
	priv->last_column = -1;
}

static void
cell_edited (GtkCellRendererText *cell,
             const gchar *path_string,
             const gchar *new_text,
             gpointer user_data)
{
	CEPageResources *self = CE_PAGE_RESOURCES (user_data);
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);
	GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->network_drives_list));
	GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
	GtkTreeIter iter;
	guint32 column;
	GtkTreeViewColumn *next_col;

	// Free auxiliary stuff
	g_free (priv->last_edited);
	priv->last_edited = NULL;
	g_free (priv->last_path);
	priv->last_path = NULL;
	priv->last_column = -1;

	column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));

	gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);
	gtk_list_store_set (store, &iter, column, new_text, -1);

	// Move focus to the next column
	column = (column >= COL_LAST) ? 0 : column + 1;
	next_col = gtk_tree_view_get_column (priv->network_drives_list, column);
	gtk_tree_view_set_cursor_on_cell (priv->network_drives_list, path, next_col, priv->network_drives_cells[column], TRUE);

	gtk_tree_path_free (path);
	ce_page_changed (CE_PAGE (self));
}

static void
delete_text_cb (GtkEditable *editable,
                    gint start_pos,
                    gint end_pos,
                    gpointer user_data)
{
	CEPageResources *self = CE_PAGE_RESOURCES (user_data);
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	// Keep last_edited up-to-date
	g_free (priv->last_edited);
	priv->last_edited = g_strdup (gtk_editable_get_chars (editable, 0, -1));
}

static gboolean
cell_changed_cb (GtkEditable *editable,
                 gpointer user_data)
{
	char *cell_text;
//	guint column;
#if GTK_CHECK_VERSION(3,0,0)
	GdkRGBA rgba;
#else
	GdkColor color;
#endif
	gboolean value_valid = TRUE;
	const char *colorname = NULL;

	cell_text = gtk_editable_get_chars (editable, 0, -1);

	// Change cell's background color while editing
	colorname = value_valid ? "lightgreen" : "red";

#if GTK_CHECK_VERSION(3,0,0)
	gdk_rgba_parse (&rgba, colorname);
	gtk_widget_override_background_color (GTK_WIDGET (editable), GTK_STATE_NORMAL, &rgba);
#else
	gdk_color_parse (colorname, &color);
	gtk_widget_modify_base (GTK_WIDGET (editable), GTK_STATE_NORMAL, &color);
#endif

	g_free (cell_text);
	return FALSE;
}

static gboolean
key_pressed_cb (GtkWidget *widget,
                GdkEvent *event,
                gpointer user_data)
{
#if !GDK_KEY_Tab
	#define GDK_KEY_Tab GDK_Tab
#endif

	// Tab should behave the same way as Enter (finish editing)
	if (event->type == GDK_KEY_PRESS && event->key.keyval == GDK_KEY_Tab)
		gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (widget));

	return FALSE;
}

static void
cell_editing_started (GtkCellRenderer *cell,
                      GtkCellEditable *editable,
                      const gchar     *path,
                      gpointer         user_data)
{
	CEPageResources *self = CE_PAGE_RESOURCES (user_data);
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	if (!GTK_IS_ENTRY (editable)) {
		g_warning ("%s: Unexpected cell editable type.", __func__);
		return;
	}

	// Initialize last_path and last_column, last_edited is initialized when the cell is edited
	g_free (priv->last_edited);
	priv->last_edited = NULL;
	g_free (priv->last_path);
	priv->last_path = g_strdup (path);
	priv->last_column = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (cell), "column"));

	// Set up the entry filter
	g_signal_connect_after (G_OBJECT (editable), "delete-text",
	                        (GCallback) delete_text_cb,
	                        user_data);

	// Set up handler for changing cell background
	g_signal_connect (G_OBJECT (editable), "changed",
	                  (GCallback) cell_changed_cb,
	                  cell);

	// Set up key pressed handler - need to handle Tab key
	g_signal_connect (G_OBJECT (editable), "key-press-event",
	                  (GCallback) key_pressed_cb,
	                  user_data);
}

static gboolean
tree_view_button_pressed_cb (GtkWidget *widget,
                             GdkEvent *event,
                             gpointer user_data)
{
	CEPageResources *self = CE_PAGE_RESOURCES (user_data);
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	// last_edited can be set e.g. when we get here by clicking an cell while
	// editing another cell. GTK3 issue neither editing-canceled nor editing-done
	// for cell renderer. Thus the previous cell value isn't saved. Store it now.
	if (priv->last_edited && priv->last_path) {
		GtkTreeIter iter;
		GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (priv->network_drives_list));
		GtkTreePath *last_treepath = gtk_tree_path_new_from_string (priv->last_path);

		gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, last_treepath);
		gtk_list_store_set (store, &iter, priv->last_column, priv->last_edited, -1);
		gtk_tree_path_free (last_treepath);

		g_free (priv->last_edited);
		priv->last_edited = NULL;
		g_free (priv->last_path);
		priv->last_path = NULL;
		priv->last_column = -1;
	}

	// Ignore double clicks events. (They are issued after the single clicks, see GdkEventButton)
	if (event->type == GDK_2BUTTON_PRESS)
		return TRUE;

	gtk_widget_grab_focus (GTK_WIDGET (priv->network_drives_list));
	return FALSE;
}

static void
finish_setup (CEPageResources *self, gpointer unused, GError *error, gpointer user_data)
{
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);
	GtkTreeSelection *selection;
	gint offset;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkListStore *schemes;
	GtkTreeIter iter;
	const char **allowed_schemes;
	int i = 0;

	if (error)
		return;

	populate_ui (self);

	// Scheme column (combobox with allowed schemes)
	schemes = gtk_list_store_new(1, G_TYPE_STRING);
	allowed_schemes = nm_setting_resources_get_allowed_schemes();
	while (allowed_schemes[i] != NULL)
	{
		gtk_list_store_append (schemes, &iter);
		gtk_list_store_set (schemes, &iter, 0, allowed_schemes[i], -1);
		i++;
	}

    renderer = gtk_cell_renderer_combo_new ();
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), self);
	g_object_set(G_OBJECT(renderer),
				 "model", GTK_TREE_MODEL (schemes),
                 "text-column", 0,
				 "editable", TRUE,
				 "has_entry", FALSE,
				 NULL);
	priv->network_drives_cells[COL_SCHEME] = GTK_CELL_RENDERER (renderer);
	offset = gtk_tree_view_insert_column_with_attributes (priv->network_drives_list,
	                                                      -1, _("Scheme"), renderer,
	                                                      "text", COL_SCHEME,
	                                                      NULL);

	// Host column
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), self);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_HOST));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (cell_editing_started), self);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), self);
	priv->network_drives_cells[COL_HOST] = GTK_CELL_RENDERER (renderer);

	offset = gtk_tree_view_insert_column_with_attributes (priv->network_drives_list,
	                                                      -1, _("Host"), renderer,
	                                                      "text", COL_HOST,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->network_drives_list), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	// Folder column
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), self);
	g_object_set_data (G_OBJECT (renderer), "column", GUINT_TO_POINTER (COL_FOLDER));
	g_signal_connect (renderer, "editing-started", G_CALLBACK (cell_editing_started), self);
	g_signal_connect (renderer, "editing-canceled", G_CALLBACK (cell_editing_canceled), self);
	priv->network_drives_cells[COL_FOLDER] = GTK_CELL_RENDERER (renderer);

	offset = gtk_tree_view_insert_column_with_attributes (priv->network_drives_list,
	                                                      -1, _("Folder/Share"), renderer,
	                                                      "text", COL_FOLDER,
	                                                      NULL);
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->network_drives_list), offset - 1);
	gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	// Buttons
	g_signal_connect (priv->network_drives_list, "button-press-event", G_CALLBACK (tree_view_button_pressed_cb), self);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->network_drive_add), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->network_drive_delete), FALSE);

	g_signal_connect (priv->network_drive_add, "clicked", G_CALLBACK (network_drive_add_clicked), self);
	g_signal_connect (priv->network_drive_delete, "clicked", G_CALLBACK (network_drive_delete_clicked), priv->network_drives_list);
	selection = gtk_tree_view_get_selection (priv->network_drives_list);
	g_signal_connect (selection, "changed", G_CALLBACK (list_selection_changed), priv->network_drive_delete);
}

CEPage *
ce_page_resources_new (NMConnection *connection,
                      GtkWindow *parent_window,
                      NMClient *client,
                      const char **out_secrets_setting_name,
                      GError **error)
{
	CEPageResources *self;
	CEPageResourcesPrivate *priv;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	self = CE_PAGE_RESOURCES (ce_page_new (CE_TYPE_PAGE_RESOURCES,
	                                      connection,
	                                      parent_window,
	                                      client,
	                                      UIDIR "/ce-page-resources.ui",
	                                      "ResourcesPage",
	                                      _("Resources")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load Resources user interface."));
		return NULL;
	}

	resources_private_init (self);
	priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_resources (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_RESOURCES (nm_setting_resources_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static gboolean
ui_to_setting (CEPageResources *self)
{
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	GSList *network_drives = NULL;
	gboolean valid = FALSE, iter_valid;

	// Network drives
	model = gtk_tree_view_get_model (priv->network_drives_list);
	iter_valid = gtk_tree_model_get_iter_first (model, &tree_iter);

	while (iter_valid) {
		char *scheme = NULL, *host = NULL, *folder = NULL, *uri = NULL;

		// Check if given data is valid
		gtk_tree_model_get (model, &tree_iter, COL_SCHEME, &scheme, -1);
		gtk_tree_model_get (model, &tree_iter, COL_HOST, &host, -1);
		gtk_tree_model_get (model, &tree_iter, COL_FOLDER, &folder, -1);
		uri = g_strdup_printf ("%s://%s/%s", scheme, host, folder);
		if (!nm_utils_parse_mount_uri(uri, NULL, NULL, NULL) ||
			!nm_setting_resources_is_scheme_allowed(scheme))
		{
			g_warning ("%s: network drive '%s' missing or invalid!",
					   __func__, uri);
			g_free (uri);
			goto out;
		}

		// If successful, values are OK, add to setting & proceed
		network_drives = g_slist_append (network_drives, uri);
		iter_valid = gtk_tree_model_iter_next (model, &tree_iter);
	}

	// Update setting
	g_object_set (priv->setting,
				  NM_SETTING_RESOURCES_NETWORK_DRIVES, network_drives,
				  NULL);
	valid = TRUE;

out:
	if (network_drives) {
		nm_utils_slist_free(network_drives, g_free);
	}

	return valid;
}

static gboolean
validate (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageResources *self = CE_PAGE_RESOURCES (page);
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	if (!ui_to_setting (self))
		return FALSE;

	return nm_setting_verify (NM_SETTING (priv->setting), NULL, error);
}

static void
ce_page_resources_init (CEPageResources *self)
{
	CEPageResourcesPrivate *priv = CE_PAGE_RESOURCES_GET_PRIVATE (self);

	priv->last_column = -1;
}

static void
ce_page_resources_class_init (CEPageResourcesClass *resources_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (resources_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (resources_class);

	g_type_class_add_private (object_class, sizeof (CEPageResourcesPrivate));

	/* virtual methods */
	parent_class->validate = validate;
}
