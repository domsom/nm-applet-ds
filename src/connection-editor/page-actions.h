#ifndef __PAGE_ACTIONS_H__
#define __PAGE_ACTIONS_H__

#include <nm-connection.h>

#include <glib.h>
#include <glib-object.h>

#include "ce-page.h"

#define CE_TYPE_PAGE_ACTIONS            (ce_page_actions_get_type ())
#define CE_PAGE_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE_ACTIONS, CEPageActions))
#define CE_PAGE_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE_ACTIONS, CEPageActionsClass))
#define CE_IS_PAGE_ACTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE_ACTIONS))
#define CE_IS_PAGE_ACTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CE_TYPE_PAGE_ACTIONS))
#define CE_PAGE_ACTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE_ACTIONS, CEPageActionsClass))

#define NM_SETTING_ACTIONS_CONNECT_SCRIPT 		"connect-script"
#define NM_SETTING_ACTIONS_DISCONNECT_SCRIPT 	"disconnect-script"

typedef struct {
	CEPage parent;
} CEPageActions;

typedef struct {
	CEPageClass parent;
} CEPageActionsClass;

GType ce_page_actions_get_type (void);

CEPage *ce_page_actions_new (NMConnection *connection,
                              GtkWindow *parent,
                              NMClient *client,
                              const char **out_secrets_setting_name,
                              GError **error);


#endif  /* __PAGE_ACTIONS_H__ */
