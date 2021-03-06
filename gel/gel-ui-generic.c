/*
 * gel/gel-ui-generic.c
 *
 * Copyright (C) 2004-2011 Eina
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

#include "gel-ui-generic.h"

#include <glib/gi18n.h>
#include "gel-ui.h"

G_DEFINE_TYPE (GelUIGeneric, gel_ui_generic, GTK_TYPE_BOX)

enum {
	PROP_XML_STRING = 1
};

struct _GelUIGenericPrivate {
	GtkBuilder *builder;
};

static void
set_xml_string(GelUIGeneric *self, const gchar *xml_string);

static void
gel_ui_generic_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	switch (property_id)
	{
	case PROP_XML_STRING:
		set_xml_string((GelUIGeneric *) object, g_value_get_string(value));
		return;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
gel_ui_generic_dispose (GObject *object)
{
	gel_free_and_invalidate(GEL_UI_GENERIC(object)->priv->builder, NULL, g_object_unref);

	G_OBJECT_CLASS (gel_ui_generic_parent_class)->dispose (object);
}

static void
gel_ui_generic_class_init (GelUIGenericClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GelUIGenericPrivate));

	object_class->set_property = gel_ui_generic_set_property;
	object_class->dispose = gel_ui_generic_dispose;

	g_object_class_install_property(object_class, PROP_XML_STRING,
		g_param_spec_string("xml-string", "xml-string", "xml-string",
		NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gel_ui_generic_init (GelUIGeneric *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), GEL_UI_TYPE_GENERIC, GelUIGenericPrivate);
}

/**
 * gel_ui_generic_new:
 * @xml_string: XML string with widget definition. See #GtkBuild doc for more info.
 *
 * Creates a new widget from the UI description from @xml_string
 *
 * Returns: The #GtkWidget
 */
GtkWidget*
gel_ui_generic_new (const gchar *xml_string)
{
	return g_object_new (GEL_UI_TYPE_GENERIC, "xml-string", xml_string, NULL);
}

/**
 * gel_ui_generic_new_from_file:
 * @filename: XML File with the widget definition, see gel_ui_generic_new()
 *
 * Loads @filename into memory and calls gel_ui_generic_new()
 *
 * Returns: the #GtkWidget
 */
GtkWidget*
gel_ui_generic_new_from_file (const gchar *filename)
{
	gchar *buffer = NULL;
	GError *err = NULL;
	if (!g_file_get_contents(filename, &buffer, NULL, &err))
	{
		g_warning(_("Cannot load UI from file '%s': %s"), filename, err->message);
		g_error_free(err);
	}
	GtkWidget *self = gel_ui_generic_new(buffer);
	gel_free_and_invalidate(buffer, NULL, g_free);

	return self;
}

/**
 * gel_ui_generic_get_builder:
 * @self: A #GelUIGeneric
 *
 * Gets the #GtkBuilder for @self
 *
 * Returns: (transfer none): The #GtkBuilder
 */
GtkBuilder*
gel_ui_generic_get_builder(GelUIGeneric *self)
{
	g_return_val_if_fail(GEL_UI_IS_GENERIC(self), NULL);
	return self->priv->builder;
}

static void
set_xml_string(GelUIGeneric *self, const gchar *xml_string)
{
	g_return_if_fail(GEL_UI_IS_GENERIC(self));
	g_return_if_fail(xml_string != NULL);

	GelUIGenericPrivate *priv = self->priv;
	g_return_if_fail(priv->builder == NULL);

	priv->builder = gtk_builder_new();

	GError *error = NULL;
	if (!gtk_builder_add_from_string(priv->builder, xml_string, -1, &error))
	{
		g_warning(_("Cannot load UI from string: %s"), error->message);
		g_error_free(error);
		return;
	}

	GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(priv->builder, "main-widget"));
	if (w == NULL)
	{
		g_warning(_("Cannot find widget 'main-widget' on UI"));
		return;
	}

	gtk_widget_reparent(w, (GtkWidget *) self);
	gtk_box_set_child_packing ((GtkBox *) self, w, TRUE, TRUE, 0, GTK_PACK_START);

	g_object_notify(G_OBJECT(self), "xml-string");
}

