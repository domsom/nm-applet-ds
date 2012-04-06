#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>

#include <nm-setting-actions.h>

#include "page-actions.h"

G_DEFINE_TYPE (CEPageActions, ce_page_actions, CE_TYPE_PAGE)

#define CE_PAGE_ACTIONS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_ACTIONS, CEPageActionsPrivate))

typedef struct {
	NMSettingActions *setting;

	GtkEntry *connect_script;
	GtkEntry *disconnect_script;
} CEPageActionsPrivate;

static void
actions_private_init (CEPageActions *self)
{
	CEPageActionsPrivate *priv = CE_PAGE_ACTIONS_GET_PRIVATE (self);
	GtkBuilder *builder;

	builder = CE_PAGE (self)->builder;

	priv->connect_script    = GTK_ENTRY (GTK_WIDGET (gtk_builder_get_object (builder, "actions_connect")));
	priv->disconnect_script = GTK_ENTRY (GTK_WIDGET (gtk_builder_get_object (builder, "actions_disconnect")));
}

static void
populate_ui (CEPageActions *self)
{
	CEPageActionsPrivate *priv = CE_PAGE_ACTIONS_GET_PRIVATE (self);
	NMSettingActions *setting = priv->setting;

	const char *connect_script = NULL;
	const char *disconnect_script = NULL;

	g_object_get (setting,
				  NM_SETTING_ACTIONS_CONNECT_SCRIPT, &connect_script,
				  NM_SETTING_ACTIONS_DISCONNECT_SCRIPT, &disconnect_script,
				  NULL);

	gtk_entry_set_text (priv->connect_script, connect_script);
	g_signal_connect_swapped (priv->connect_script, "changed", G_CALLBACK (ce_page_changed), self);

	gtk_entry_set_text (priv->disconnect_script, disconnect_script);
	g_signal_connect_swapped (priv->disconnect_script, "changed", G_CALLBACK (ce_page_changed), self);
}

static void
finish_setup (CEPageActions *self, gpointer unused, GError *error, gpointer user_data)
{
//	CEPage *parent = CE_PAGE (self);
//	GtkWidget *widget;

	if (error)
		return;

	populate_ui (self);
}

CEPage *
ce_page_actions_new (NMConnection *connection,
                      GtkWindow *parent_window,
                      NMClient *client,
                      const char **out_secrets_setting_name,
                      GError **error)
{
	CEPageActions *self;
	CEPageActionsPrivate *priv;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	self = CE_PAGE_ACTIONS (ce_page_new (CE_TYPE_PAGE_ACTIONS,
	                                      connection,
	                                      parent_window,
	                                      client,
	                                      UIDIR "/ce-page-actions.ui",
	                                      "ActionsPage",
	                                      _("Actions")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Could not load Actions user interface."));
		return NULL;
	}

	actions_private_init (self);
	priv = CE_PAGE_ACTIONS_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_actions (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_ACTIONS (nm_setting_actions_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageActions *self)
{
	CEPageActionsPrivate *priv = CE_PAGE_ACTIONS_GET_PRIVATE (self);
	const char *connect_script = NULL;
	const char *disconnect_script = NULL;

	connect_script = gtk_entry_get_text (priv->connect_script);
	disconnect_script = gtk_entry_get_text (priv->disconnect_script);

	g_object_set (priv->setting,
				  NM_SETTING_ACTIONS_CONNECT_SCRIPT, connect_script,
				  NM_SETTING_ACTIONS_DISCONNECT_SCRIPT, disconnect_script,
				  NULL);
}

static gboolean
validate (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageActions *self = CE_PAGE_ACTIONS (page);

	ui_to_setting (self);

	return TRUE;
}

static void
ce_page_actions_init (CEPageActions *self)
{
}

static void
ce_page_actions_class_init (CEPageActionsClass *actions_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (actions_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (actions_class);

	g_type_class_add_private (object_class, sizeof (CEPageActionsPrivate));

	/* virtual methods */
	parent_class->validate = validate;
}
