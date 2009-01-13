#define GEL_DOMAIN "Eina::Template"

#include <config.h>
#include <gmodule.h>
#include <eina/template.h>
#include <eina/eina-plugin.h>

// --
// Define ourselves 
// --
struct _EinaTemplate {
	EinaObj  parent;

	// Add your own fields here
	gpointer dummy;
};

// --
// init/fini hooks
// --
static gboolean 
template_init (EinaPlugin *plugin, GError **error)
{
	GelApp *app = gel_plugin_get_app(plugin);
	EinaTemplate *self;

	// Initialize ourself and automagically register into app using EinaObj
	self = g_new0(EinaTemplate, 1);
	if (!eina_obj_init(EINA_OBJ(self), app, "template", EINA_OBJ_NONE, error))
	{	
		g_free(self);
		return FALSE;
	}

	// You may want to get info from another plugin (some sort of dependence)
	/*
	EinaFoo *foo;
	if ((foo = eina_obj_require(self, "foo", error)) == NULL)
		return FALSE;
	*/

	return TRUE;
}

static gboolean
template_fini (EinaPlugin *plugin, GError **error)
{
	GelApp *app = gel_plugin_get_app(plugin);

	// Find yourself, rememer your macros in template.h?
	EinaTemplate *self = GEL_APP_GET_TEMPLATE(app);

	// Unref dependences
	/*
	eina_obj_unrequire(self, "foo", NULL);
	*/

	// Free ourserves
	eina_obj_fini(EINA_OBJ(self));
	return TRUE;
}

// --
// Plugin definition
// --
G_MODULE_EXPORT EinaPlugin template_plugin = {
	EINA_PLUGIN_SERIAL,
	"template", PACKAGE_VERSION,
	N_("Build-in template plugin"), NULL,
	NULL, NULL, NULL,
	template_init, template_fini,
	NULL, NULL
};

