#ifndef __PAGE_RESOURCES_H__
#define __PAGE_RESOURCES_H__

#include <nm-connection.h>

#include <glib.h>
#include <glib-object.h>

#include "ce-page.h"

#define CE_TYPE_PAGE_RESOURCES            (ce_page_resources_get_type ())
#define CE_PAGE_RESOURCES(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CE_TYPE_PAGE_RESOURCES, CEPageResources))
#define CE_PAGE_RESOURCES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CE_TYPE_PAGE_RESOURCES, CEPageResourcesClass))
#define CE_IS_PAGE_RESOURCES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CE_TYPE_PAGE_RESOURCES))
#define CE_IS_PAGE_RESOURCES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CE_TYPE_PAGE_RESOURCES))
#define CE_PAGE_RESOURCES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CE_TYPE_PAGE_RESOURCES, CEPageResourcesClass))

#define NM_SETTING_RESOURCES_CONNECT_SCRIPT 		"connect-script"
#define NM_SETTING_RESOURCES_DISCONNECT_SCRIPT 	"disconnect-script"

typedef struct {
	CEPage parent;
} CEPageResources;

typedef struct {
	CEPageClass parent;
} CEPageResourcesClass;

GType ce_page_resources_get_type (void);

CEPage *ce_page_resources_new (NMConnection *connection,
                              GtkWindow *parent,
                              NMClient *client,
                              const char **out_secrets_setting_name,
                              GError **error);


#endif  /* __PAGE_RESOURCES_H__ */
