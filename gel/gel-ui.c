/*
 * gel/gel-ui.c
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

#define GEL_DOMAIN "Gel"
#include "gel-ui.h"
#include <glib/gi18n.h>

static GQuark
gel_ui_quark(void)
{
	GQuark ret = 0;
	if (!ret)
		ret = g_quark_from_static_string("gel-ui");
	return ret;
}

/*
 * UI creation
 */
GelUI *
gel_ui_load_resource(gchar *ui_filename, GError **error)
{
	GelUI *ret = NULL;
	gchar *ui_pathname;
	gchar *tmp;

	tmp = g_strconcat(ui_filename, ".ui", NULL);
	ui_pathname = gel_resource_locate(GEL_RESOURCE_UI, tmp);

	if (ui_pathname == NULL)
	{
		g_set_error(error, gel_ui_quark(), GEL_UI_ERROR_RESOURCE_NOT_FOUND,
			N_("Cannot locate resource '%s'"), tmp);
		g_free(tmp);
		return NULL;
	}
	g_free(tmp);

	ret = gtk_builder_new();
	if (gtk_builder_add_from_file(ret, ui_pathname, error) == 0) {
		gel_error("Error loading GtkBuilder: %s", (*error)->message);
		g_object_unref(ret);
		ret = NULL;
	}

	return ret;
}

/*
 * Signal handling
 */
gboolean
gel_ui_signal_connect_from_def(GelUI *ui, GelUISignalDef def, gpointer data, GError **error)
{
	GObject *object;

	object = (GObject *) gtk_builder_get_object(ui, def.widget);

	if ((object == NULL) || !G_OBJECT(object))
	{ 
		gel_warn("Can not find object '%s' at GelUI %p", def.widget, ui);
		return FALSE;
	}

	g_signal_connect(object, def.signal, def.callback, data);
	return TRUE;
}

gboolean
gel_ui_signal_connect_from_def_multiple(GelUI *ui, GelUISignalDef defs[], gpointer data, guint *count)
{
	guint _count = 0;
	gboolean ret = TRUE;
	guint i;
	
	for (i = 0; defs[i].widget != NULL; i++)
	{
		if (gel_ui_signal_connect_from_def(ui, defs[i], data, NULL))
			_count++;
		else
			ret = FALSE;
	}

	if (count != NULL)
		*count = _count;
	return ret;
}

/*
 * Images on UI's
 */
GdkPixbuf *
gel_ui_load_pixbuf_from_imagedef(GelUIImageDef def, GError **error)
{
	GdkPixbuf *ret;

	// gchar *pathname = gel_app_resource_get_pathname(GEL_APP_RESOURCE_IMAGE, def.image);
	gchar *pathname = gel_plugin_get_resource(NULL, GEL_RESOURCE_IMAGE, def.image);
	if (pathname == NULL)
	{
		// Handle error
		return NULL;
	}

	ret = gdk_pixbuf_new_from_file_at_size(pathname, def.w, def.h, error);
	g_free(pathname);
	return ret;
}

void
gel_ui_container_replace_children(GtkContainer *container, GtkWidget *widget)
{
	gtk_container_foreach(container, (GtkCallback) gtk_widget_destroy, NULL);
	gtk_box_pack_start(GTK_BOX(container), widget, TRUE, TRUE, 0); 
}

G_END_DECLS
