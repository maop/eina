/*
 * eina/ext/eina-plugin-dialog.c
 *
 * Copyright (C) 2004-2009 Eina
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <glib/gi18n.h>
#include "eina-plugin-dialog.h"
#include "eina-plugin-dialog-ui.h"
#include "eina-plugin-properties.h"

G_DEFINE_TYPE (EinaPluginDialog, eina_plugin_dialog, GTK_TYPE_DIALOG)

#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), EINA_TYPE_PLUGIN_DIALOG, EinaPluginDialogPrivate))

typedef struct _EinaPluginDialogPrivate EinaPluginDialogPrivate;

struct _EinaPluginDialogPrivate {
	GelApp   *app;

	GList       *plugins;
	GtkTreeView *tv;
	GtkNotebook *tabs;
	GtkWidget   *info, *close;

	guint n_plugins;
};

enum {
	PROPERTY_APP = 1,
};

enum {
	COLUMN_ENABLED = 0,
	COLUMN_ICON,
	COLUMN_MARKUP,
	COLUMN_PLUGIN
};

static void
set_app(EinaPluginDialog *self, GelApp *app);
static void
insert_plugin(EinaPluginDialog *self, GtkListStore *model, GtkTreeIter *iter, GelPlugin *plugin);
static void
update_plugin(EinaPluginDialog *self, GelPlugin *plugin);
static gboolean
get_iter_from_plugin(EinaPluginDialog *self, GtkTreeIter *iter, GelPlugin *plugin);
static void
enabled_renderer_toggled_cb(GtkCellRendererToggle *render, gchar *path, EinaPluginDialog *self);
static void
plugin_init_cb(GelApp *app, GelPlugin *plugin, EinaPluginDialog *self);
static void
plugin_fini_cb(GelApp *app, GelPlugin *plugin, EinaPluginDialog *self);

static void
eina_plugin_dialog_get_property (GObject *object, guint property_id,
		                          GValue *value, GParamSpec *pspec)
{
	EinaPluginDialog *self = EINA_PLUGIN_DIALOG(object);

	switch (property_id) {
	case PROPERTY_APP:
		g_value_set_object(value, (GObject *) eina_plugin_dialog_get_app(self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eina_plugin_dialog_set_property (GObject *object, guint property_id,
		                          const GValue *value, GParamSpec *pspec)
{
	EinaPluginDialog *self = EINA_PLUGIN_DIALOG(object);

	switch (property_id) {
	case PROPERTY_APP:
		set_app(self, GEL_APP(g_value_get_object(value)));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
eina_plugin_dialog_dispose (GObject *object)
{
	EinaPluginDialog *self = EINA_PLUGIN_DIALOG(object);
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);
	
	GelApp *app = priv->app;
	if (priv->app)
	{
		g_signal_handlers_disconnect_by_func(priv->app, plugin_init_cb, self);
		g_signal_handlers_disconnect_by_func(priv->app, plugin_fini_cb, self);
		priv->app = NULL;
	}

	if (priv->plugins)
	{
		g_list_foreach(priv->plugins, (GFunc) gel_plugin_remove_lock, NULL);
	
		GList *tmp = g_list_sort(priv->plugins, (GCompareFunc) gel_plugin_compare_by_usage);
		GList *l = tmp;
		while (l)
		{
			GelPlugin *plugin = GEL_PLUGIN(l->data);
			if (!gel_plugin_is_enabled(plugin) && app)
				gel_app_unload_plugin(app, plugin, NULL);
			l = l->next;
		}
		g_list_free(tmp);

		if (app)
			gel_app_purge(app);
		priv->plugins = NULL;
	}

	G_OBJECT_CLASS (eina_plugin_dialog_parent_class)->dispose (object);
}

static void
eina_plugin_dialog_class_init (EinaPluginDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EinaPluginDialogPrivate));

	object_class->get_property = eina_plugin_dialog_get_property;
	object_class->set_property = eina_plugin_dialog_set_property;
	object_class->dispose = eina_plugin_dialog_dispose;

	g_object_class_install_property(object_class, PROPERTY_APP,
		g_param_spec_object("app", "Gel App", "GelApp object to handleto display",
		GEL_TYPE_APP, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT
		));
}

static void
eina_plugin_dialog_init (EinaPluginDialog *self)
{
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);

	// Build UI
	gchar *objs[] = { "tabs", "liststore", NULL };
	GError *error = NULL;
	GtkBuilder *builder = gtk_builder_new();
	if (gtk_builder_add_objects_from_string(builder, ui_xml, -1, objs, &error) == 0)
	{
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	gtk_container_add(
		(GtkContainer *) gtk_dialog_get_content_area((GtkDialog *) self),
		(GtkWidget    *)  gtk_builder_get_object(builder, "tabs"));

	priv->tv = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview"));
	priv->tabs = GTK_NOTEBOOK(gtk_builder_get_object(builder, "tabs"));

	g_signal_connect(gtk_builder_get_object(builder, "enabled-renderer"), "toggled", (GCallback) enabled_renderer_toggled_cb, self);
	g_object_set(self, "title", N_("Select plugins"), NULL);

	g_object_unref(builder);

	priv->info  = gtk_dialog_add_button((GtkDialog *) self, GTK_STOCK_INFO, EINA_PLUGIN_DIALOG_RESPONSE_INFO);
	priv->close = gtk_dialog_add_button((GtkDialog *) self, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
}

EinaPluginDialog*
eina_plugin_dialog_new (GelApp *app)
{
	return g_object_new (EINA_TYPE_PLUGIN_DIALOG, "app", app, NULL);
}

GelApp *
eina_plugin_dialog_get_app(EinaPluginDialog *self)
{
	return GET_PRIVATE(self)->app;
}

static void
set_app(EinaPluginDialog *self, GelApp *app)
{
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);
	g_return_if_fail(priv->app == NULL);
	g_return_if_fail(app != NULL);

	priv->app = app;

	// Get plugins, lock and count
	GList *l = priv->plugins = g_list_sort(gel_app_query_plugins(priv->app), (GCompareFunc) gel_plugin_compare_by_name);
	while (l)
	{
		GelPlugin *plugin = GEL_PLUGIN(l->data);
		if (gel_plugin_get_pathname(plugin))
		{
			gel_plugin_add_lock(plugin);
			priv->n_plugins++;
		}
		l = l->next;
	}

	// Tabs
	gtk_notebook_set_show_tabs(priv->tabs, FALSE);
	if (priv->n_plugins == 0)
	{
		gtk_notebook_set_current_page(priv->tabs, 0);
		return;
	}
	gtk_notebook_set_current_page(priv->tabs, 1);

	// Fill notebook
	GtkTreeIter   iter;
	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(priv->tv));
	l = priv->plugins;
	while (l)
	{
		gtk_list_store_append(model, &iter);
		insert_plugin(self, model, &iter, GEL_PLUGIN(l->data));

		l = l->next;
	}

	GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(self));
	gint w = gdk_screen_get_width(screen)  / 4;
	gint h = gdk_screen_get_height(screen) / 2;
	gtk_window_resize(GTK_WINDOW(self), w, h);

	g_signal_connect(app,  "plugin-init", (GCallback) plugin_init_cb, self);
	g_signal_connect(app,  "plugin-fini", (GCallback) plugin_fini_cb, self);
}


GelPlugin *
eina_plugin_dialog_get_selected_plugin(EinaPluginDialog *self)
{
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);

	GtkTreeSelection *tvsel;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	GelPlugin    *plugin;

	tvsel = gtk_tree_view_get_selection(priv->tv);
	if (!gtk_tree_selection_get_selected(tvsel, &model, &iter))
		return NULL;

	gtk_tree_model_get(model, &iter, COLUMN_PLUGIN, &plugin, -1);
	return plugin;
}

static gboolean
get_iter_from_plugin(EinaPluginDialog *self, GtkTreeIter *iter, GelPlugin *plugin)
{
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);
	GtkTreeModel *model = gtk_tree_view_get_model(priv->tv);
	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;
	do
	{
		GelPlugin *test;
		gtk_tree_model_get(model, iter, COLUMN_PLUGIN, &test, -1);
		if (test == plugin)
			return TRUE;

	} while (gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static void
insert_plugin(EinaPluginDialog *self, GtkListStore *model, GtkTreeIter *iter, GelPlugin *plugin)
{
	gchar *pb_path = NULL;
	GdkPixbuf *pb = NULL;
	if (plugin->icon)
	{
		pb_path = gel_plugin_get_resource(plugin, GEL_RESOURCE_IMAGE, (gchar *) plugin->icon);
		if (pb_path)
		{
			pb = gdk_pixbuf_new_from_file_at_size(pb_path, 64, 64, NULL);
			g_free(pb_path);
		}
	}

	if (pb == NULL)
	{
		pb_path = gel_resource_locate(GEL_RESOURCE_IMAGE, "plugin.png");
		pb = gdk_pixbuf_new_from_file_at_size(pb_path, 64, 64, NULL);
		g_free(pb_path);
	}

	gchar *markup = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>\n%s",
		plugin->name,
		plugin->short_desc
		);

	gtk_list_store_set(model, iter,
		COLUMN_ENABLED, gel_plugin_is_enabled(plugin),
		COLUMN_ICON,    pb,
		COLUMN_MARKUP,  markup,
		COLUMN_PLUGIN,  plugin,
		-1);
	g_free(markup);
}

static void
update_plugin(EinaPluginDialog *self, GelPlugin *plugin)
{
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);

	GtkTreeIter iter;
	if (!get_iter_from_plugin(self, &iter, plugin))
	{
		g_warning(N_("Loaded unknow plugin: %s"), gel_plugin_stringify(plugin));
		return;
	}

	GtkListStore *model = GTK_LIST_STORE(gtk_tree_view_get_model(priv->tv));
	gtk_list_store_set(model, &iter, COLUMN_ENABLED, gel_plugin_is_enabled(plugin), -1);
}

static void
enabled_renderer_toggled_cb(GtkCellRendererToggle *render, gchar *path, EinaPluginDialog *self)
{
	EinaPluginDialogPrivate *priv = GET_PRIVATE(self);

	GtkTreePath *tp = gtk_tree_path_new_from_string(path);
	GtkTreeIter iter;

	GtkTreeModel *model = gtk_tree_view_get_model(priv->tv);
	gtk_tree_model_get_iter(model, &iter, tp);
	gtk_tree_path_free(tp);

	GelPlugin *plugin;
	gtk_tree_model_get(model, &iter,
		COLUMN_PLUGIN, &plugin,
		-1);

	if (gel_plugin_is_enabled(plugin))
	{
		gel_app_unload_plugin(priv->app, plugin, NULL);
	}
	else
	{
		gel_app_load_plugin(priv->app, (gchar *) gel_plugin_get_pathname(plugin), (gchar *) plugin->name, NULL);
	}
}

static void
plugin_init_cb(GelApp *app, GelPlugin *plugin, EinaPluginDialog *self)
{
	update_plugin(self, plugin);
}

static void
plugin_fini_cb(GelApp *app, GelPlugin *plugin, EinaPluginDialog *self)
{
	update_plugin(self, plugin);
}
